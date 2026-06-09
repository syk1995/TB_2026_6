#!/usr/bin/env python3
"""Decode one ASU source run and compare binary hitmaps with hitsHistogram.txt."""

from __future__ import annotations

import argparse
import re
import subprocess
import struct
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import uproot


DEFAULT_RUN_DIR = Path(
    "/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/source_asu/"
    "source_asu_2026_002_th250_run_000009"
)
REPO_ROOT = Path("/home/llr/ilc/shi/code/TB_2026_6")
MACROS_DIR = REPO_ROOT / "Decode" / "macros"
MAPPING_PATH = (
    REPO_ROOT / "Decode" / "mapping" / "fev11_cob_rotate_chip_channel_x_y_mapping.txt"
)
TREE_NAME = "siwecaldecoded"
FIXED_HIT_SHAPE = (15, 16, 15, 64)
INVALID_VALUE = -999
MAGIC = b"\xee\xee\xee\xee"
# Per decoded frame:
#   frame header is 50 bytes, then each SCA block is
#   int single_event_number + ushort bcid + uchar sca + uint nhits + 64 channels.
DECODED_FRAME_HEADER_SIZE = 50
DECODED_SINGLE_EVENT_HEADER_SIZE = 11
DECODED_CHANNEL_RECORD_SIZE = 9
DECODED_SINGLE_EVENT_SIZE = DECODED_SINGLE_EVENT_HEADER_SIZE + 64 * DECODED_CHANNEL_RECORD_SIZE


def natural_key(path: Path) -> list[object]:
    return [int(part) if part.isdigit() else part for part in re.split(r"(\d+)", path.name)]


def find_bin_files(run_dir: Path) -> list[Path]:
    files = [
        path
        for path in run_dir.iterdir()
        if path.is_file()
        and ".bin" in path.name
        and not path.name.endswith(".root")
        and not path.name.startswith("converted_")
    ]
    return sorted(files, key=natural_key)


def root_output_path(input_path: Path) -> Path:
    return input_path.with_name(f"converted_{input_path.name}.root")


def decode_bin_file(input_path: Path, output_path: Path, force: bool) -> None:
    if output_path.exists() and not force:
        print(f"Reuse decoded ROOT: {output_path}")
        return

    cmd = [
        "root",
        "-l",
        "-b",
        "-q",
        f'ConvertFile.cc("{input_path}","{output_path}","raw",false,true,true)',
    ]
    print(f"Decode: {input_path.name} -> {output_path.name}")
    subprocess.run(cmd, cwd=MACROS_DIR, check=True)


def detect_input_format(bin_files: list[Path]) -> str:
    head = bin_files[0].read_bytes()[:4096]
    if b"DATA STRUCTURE INFO : DECODED FRAMES" in head:
        return "decoded-bin"
    return "raw-root"


def parse_hits_histogram(path: Path) -> tuple[np.ndarray, dict[str, int]]:
    hits = np.zeros((16, 64), dtype=np.int64)
    summary: dict[str, int] = {}
    channel_re = re.compile(
        r"SkirocIndex\s+(\d+)\s+ChipId\s+(\d+)\s+Channel\s+(\d+)\s+"
        r"TotalNbOfHits\s+(\d+)"
    )
    summary_re = re.compile(
        r"RunElapsedTime\s+(\d+).*TotalNbOfCycles\s+(\d+).*"
        r"TotalNbOfHits\s+(\d+).*TotalNbOfSkirocEvents\s+(\d+).*"
        r"TotalNbOfSingleSkirocEvents\s+(\d+)"
    )

    for line in path.read_text(errors="replace").splitlines():
        match = channel_re.search(line)
        if match:
            skiroc, chip, channel, count = map(int, match.groups())
            if skiroc != chip:
                print(f"Warning: SkirocIndex {skiroc} != ChipId {chip} in {path}")
            hits[chip, channel] += count
            continue
        match = summary_re.search(line)
        if match:
            keys = [
                "run_elapsed_seconds",
                "total_cycles",
                "total_hits",
                "total_skiroc_events",
                "total_single_skiroc_events",
            ]
            summary = dict(zip(keys, map(int, match.groups())))

    return hits, summary


def read_fixed_int_branch(root_path: Path, branch_name: str) -> np.ndarray:
    with uproot.open(root_path) as root_file:
        tree = root_file[TREE_NAME]
        branch = tree[branch_name]
        baskets = []
        for basket_index in range(branch.num_baskets):
            entry_start, entry_stop = branch.basket_entry_start_stop(basket_index)
            basket_entries = int(entry_stop - entry_start)
            data = np.frombuffer(branch.basket(basket_index).data, dtype=">i4")
            baskets.append(data.reshape((basket_entries,) + FIXED_HIT_SHAPE))

    if not baskets:
        raise ValueError(f"No baskets found for {branch_name} in {root_path}")
    return np.concatenate(baskets, axis=0).astype(np.int32, copy=False)


def count_hits_from_root(root_paths: list[Path], branch_name: str, slab_index: int) -> np.ndarray:
    total = np.zeros((16, 64), dtype=np.int64)
    for root_path in root_paths:
        values = read_fixed_int_branch(root_path, branch_name)
        total += (values[:, slab_index, :, :, :] == 1).sum(axis=(0, 2))
    return total


def count_hits_from_decoded_bin(bin_paths: list[Path]) -> tuple[np.ndarray, np.ndarray, dict[str, int]]:
    low_total = np.zeros((16, 64), dtype=np.int64)
    high_total = np.zeros((16, 64), dtype=np.int64)
    stats = {"files": 0, "frames": 0, "single_skiroc_events": 0, "bad_frames": 0}

    for bin_path in bin_paths:
        data = bin_path.read_bytes()
        stats["files"] += 1
        pos = data.find(MAGIC)
        while pos >= 0:
            frame_start = pos + len(MAGIC)
            if frame_start + DECODED_FRAME_HEADER_SIZE > len(data):
                break

            size = struct.unpack_from("<i", data, frame_start + 4)[0]
            chip = data[frame_start + 8]
            skiroc = data[frame_start + 13]
            if not (0 <= size <= 15 and 0 <= chip < 16 and skiroc == chip):
                stats["bad_frames"] += 1
                pos = data.find(MAGIC, pos + 1)
                continue

            frame_end = frame_start + DECODED_FRAME_HEADER_SIZE + size * DECODED_SINGLE_EVENT_SIZE
            if frame_end > len(data):
                stats["bad_frames"] += 1
                break

            for event_index in range(size):
                event_base = frame_start + DECODED_FRAME_HEADER_SIZE + event_index * DECODED_SINGLE_EVENT_SIZE
                channel_base = event_base + DECODED_SINGLE_EVENT_HEADER_SIZE
                for channel_index in range(64):
                    record = channel_base + channel_index * DECODED_CHANNEL_RECORD_SIZE
                    channel = data[record]
                    if channel != channel_index:
                        stats["bad_frames"] += 1
                        continue
                    low_hit = data[record + 3]
                    high_hit = data[record + 7]
                    low_total[chip, channel] += int(low_hit > 0)
                    high_total[chip, channel] += int(high_hit > 0)

            stats["frames"] += 1
            stats["single_skiroc_events"] += size
            pos = data.find(MAGIC, frame_end)

    return low_total, high_total, stats


def load_mapping(path: Path) -> dict[tuple[int, int], tuple[float, float]]:
    rows = np.genfromtxt(path, names=True)
    mapping: dict[tuple[int, int], tuple[float, float]] = {}
    for row in rows:
        mapping[(int(row["chip"]), int(row["channel"]))] = (float(row["x"]), float(row["y"]))
    return mapping


def chip_channel_to_xy(values: np.ndarray, mapping: dict[tuple[int, int], tuple[float, float]]) -> tuple[np.ndarray, list[float], list[float]]:
    xs = sorted({xy[0] for xy in mapping.values()})
    ys = sorted({xy[1] for xy in mapping.values()})
    x_index = {x: i for i, x in enumerate(xs)}
    y_index = {y: i for i, y in enumerate(ys)}
    grid = np.full((len(ys), len(xs)), np.nan)

    for chip in range(values.shape[0]):
        for channel in range(values.shape[1]):
            x, y = mapping[(chip, channel)]
            grid[y_index[y], x_index[x]] = values[chip, channel]

    return grid, xs, ys


def save_hitmap(values: np.ndarray, path: Path, title: str, xlabel: str, ylabel: str) -> None:
    fig, ax = plt.subplots(figsize=(10, 5.5), constrained_layout=True)
    image = ax.imshow(values, origin="lower", aspect="auto", interpolation="nearest")
    ax.set_title(title)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    fig.colorbar(image, ax=ax, label="hits")
    fig.savefig(path, dpi=180)
    plt.close(fig)


def save_xy_hitmap(values: np.ndarray, xs: list[float], ys: list[float], path: Path, title: str) -> None:
    fig, ax = plt.subplots(figsize=(7.2, 6.4), constrained_layout=True)
    image = ax.imshow(
        values,
        origin="lower",
        interpolation="nearest",
        extent=[min(xs), max(xs), min(ys), max(ys)],
        aspect="equal",
    )
    ax.set_title(title)
    ax.set_xlabel("x [mm]")
    ax.set_ylabel("y [mm]")
    fig.colorbar(image, ax=ax, label="hits")
    fig.savefig(path, dpi=180)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-dir", type=Path, default=DEFAULT_RUN_DIR)
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--slab-index", type=int, default=0)
    parser.add_argument("--branch", choices=["hitbit_high", "hitbit_low"], default="hitbit_high")
    parser.add_argument("--input-format", choices=["auto", "decoded-bin", "raw-root"], default="auto")
    parser.add_argument("--force-decode", action="store_true")
    parser.add_argument("--skip-decode", action="store_true")
    args = parser.parse_args()

    run_dir = args.run_dir.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    bin_files = find_bin_files(run_dir)
    if not bin_files:
        raise FileNotFoundError(f"No .bin files found in {run_dir}")

    input_format = detect_input_format(bin_files) if args.input_format == "auto" else args.input_format

    txt_hits, txt_summary = parse_hits_histogram(run_dir / "hitsHistogram.txt")
    root_paths: list[Path] = []
    decoded_extra: dict[str, object] = {}
    if input_format == "raw-root":
        root_paths = [root_output_path(path) for path in bin_files]
        if not args.skip_decode:
            for input_path, output_path in zip(bin_files, root_paths):
                decode_bin_file(input_path, output_path, args.force_decode)

        missing = [path for path in root_paths if not path.exists()]
        if missing:
            raise FileNotFoundError(f"Missing decoded ROOT files: {missing}")
        decoded_hits = count_hits_from_root(root_paths, args.branch, args.slab_index)
    else:
        low_hits, high_hits, decoded_stats = count_hits_from_decoded_bin(bin_files)
        decoded_extra = {
            "low_total": int(low_hits.sum()),
            "high_total": int(high_hits.sum()),
            "low_max_abs_diff": int(np.max(np.abs(low_hits - txt_hits))),
            "high_max_abs_diff": int(np.max(np.abs(high_hits - txt_hits))),
            "low_mismatched_channels": int(np.count_nonzero(low_hits - txt_hits)),
            "high_mismatched_channels": int(np.count_nonzero(high_hits - txt_hits)),
            **decoded_stats,
        }
        decoded_hits = high_hits if args.branch == "hitbit_high" else low_hits

    diff = decoded_hits - txt_hits
    run_name = run_dir.name

    prefix = output_dir / run_name
    save_hitmap(txt_hits, prefix.with_name(f"{run_name}_txt_chip_channel.png"), "hitsHistogram.txt", "channel", "chip")
    save_hitmap(
        decoded_hits,
        prefix.with_name(f"{run_name}_{args.branch}_chip_channel.png"),
        f"Decoded ROOT {args.branch}",
        "channel",
        "chip",
    )

    mapping = load_mapping(MAPPING_PATH)
    txt_xy, xs, ys = chip_channel_to_xy(txt_hits, mapping)
    decoded_xy, _, _ = chip_channel_to_xy(decoded_hits, mapping)
    save_xy_hitmap(txt_xy, xs, ys, prefix.with_name(f"{run_name}_txt_xy.png"), "hitsHistogram.txt XY")
    save_xy_hitmap(decoded_xy, xs, ys, prefix.with_name(f"{run_name}_{args.branch}_xy.png"), f"Decoded ROOT {args.branch} XY")

    lines = [
        f"run_dir: {run_dir}",
        f"input_format: {input_format}",
        (
            f"decoder: {MACROS_DIR / 'ConvertFile.cc'} mode=raw -> {MACROS_DIR / 'SLBraw2ROOT.cc'}"
            if input_format == "raw-root"
            else f"decoder: decoded-frame parser in {Path(__file__).resolve()}"
        ),
        "decoded_root_files:" if root_paths else "decoded_bin_files:",
        *[f"  {path}" for path in (root_paths or bin_files)],
        f"branch: {args.branch}",
        f"slab_index: {args.slab_index}",
        f"txt_total_hits_summary: {txt_summary.get('total_hits', 'missing')}",
        f"txt_total_hits_from_channels: {int(txt_hits.sum())}",
        f"decoded_total_hits: {int(decoded_hits.sum())}",
        f"diff_sum: {int(diff.sum())}",
        f"n_mismatched_channels: {int(np.count_nonzero(diff))}",
        f"max_abs_diff: {int(np.max(np.abs(diff)))}",
    ]
    for key in sorted(decoded_extra):
        lines.append(f"{key}: {decoded_extra[key]}")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
