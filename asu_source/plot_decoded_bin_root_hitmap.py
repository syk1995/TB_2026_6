#!/usr/bin/env python3
"""Plot source-ASU run hitmaps into a single multi-page PDF."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import numpy as np
import uproot


DEFAULT_DATA_ROOT = Path(
    "/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/source_asu"
)
DEFAULT_OUTPUT_PDF = Path(
    "/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/source_asu/"
    "source_asu_all_run_hitmaps.pdf"
)
MAPPING_PATH = Path(
    "/home/llr/ilc/shi/code/TB_2026_6/Decode/mapping/"
    "fev11_cob_rotate_chip_channel_x_y_mapping.txt"
)
BRANCH_SHAPE = (15, 16, 15, 64)


def parse_hits_histogram(path: Path) -> np.ndarray:
    hits = np.zeros((16, 64), dtype=np.int64)
    pattern = re.compile(
        r"SkirocIndex\s+(\d+)\s+ChipId\s+(\d+)\s+Channel\s+(\d+)\s+"
        r"TotalNbOfHits\s+(\d+)"
    )
    for line in path.read_text(errors="replace").splitlines():
        match = pattern.search(line)
        if not match:
            continue
        _, chip, channel, count = map(int, match.groups())
        hits[chip, channel] += count
    return hits


def find_root_files(run_dir: Path) -> list[Path]:
    return sorted(run_dir.glob("converted_*.root"))


def iter_run_dirs(data_root: Path) -> list[Path]:
    return sorted(run_dir for run_dir in data_root.iterdir() if run_dir.is_dir())


def count_root_hits(root_paths: list[Path], branch_name: str, slab_index: int) -> tuple[np.ndarray, int]:
    total = np.zeros((16, 64), dtype=np.int64)
    n_entries = 0
    for root_path in root_paths:
        with uproot.open(root_path) as root_file:
            tree = root_file["siwecaldecoded"]
            branch = tree[branch_name]
            n_entries += tree.num_entries
            for basket_index in range(branch.num_baskets):
                entry_start, entry_stop = branch.basket_entry_start_stop(basket_index)
                basket_entries = int(entry_stop - entry_start)
                data = np.frombuffer(branch.basket(basket_index).data, dtype=">i4")
                values = data.reshape((basket_entries,) + BRANCH_SHAPE)
                total += (values[:, slab_index, :, :, :] == 1).sum(axis=(0, 2))
    return total, n_entries


def load_mapping(path: Path) -> dict[tuple[int, int], tuple[float, float]]:
    rows = np.genfromtxt(path, names=True)
    return {
        (int(row["chip"]), int(row["channel"])): (float(row["x"]), float(row["y"]))
        for row in rows
    }


def chip_channel_to_xy(values: np.ndarray, mapping: dict[tuple[int, int], tuple[float, float]]):
    xs = sorted({xy[0] for xy in mapping.values()})
    ys = sorted({xy[1] for xy in mapping.values()})
    x_index = {x: i for i, x in enumerate(xs)}
    y_index = {y: i for i, y in enumerate(ys)}
    grid = np.full((len(ys), len(xs)), np.nan)
    for chip in range(16):
        for channel in range(64):
            x, y = mapping[(chip, channel)]
            grid[y_index[y], x_index[x]] = values[chip, channel]
    return grid, xs, ys


def save_chip_channel(values: np.ndarray, output: Path, title: str) -> None:
    fig, ax = plt.subplots(figsize=(10, 5.5), constrained_layout=True)
    image = ax.imshow(values, origin="lower", aspect="auto", interpolation="nearest")
    ax.set_title(title)
    ax.set_xlabel("channel")
    ax.set_ylabel("chip")
    fig.colorbar(image, ax=ax, label="hits")
    fig.savefig(output, dpi=180)
    plt.close(fig)


def save_xy(values: np.ndarray, xs: list[float], ys: list[float], output: Path, title: str) -> None:
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
    fig.savefig(output, dpi=180)
    plt.close(fig)


def add_hitmap(image_values: np.ndarray, ax: plt.Axes, title: str) -> None:
    image = ax.imshow(image_values, origin="lower", aspect="auto", interpolation="nearest")
    ax.set_title(title)
    ax.set_xlabel("channel")
    ax.set_ylabel("chip")
    ax.figure.colorbar(image, ax=ax, label="hits", fraction=0.046, pad=0.04)


def save_run_page(
    pdf: PdfPages,
    run_dir: Path,
    root_paths: list[Path],
    root_hits: np.ndarray,
    txt_hits: np.ndarray | None,
    n_entries: int,
    branch_name: str,
) -> dict[str, int | str]:
    run_name = run_dir.name
    root_total = int(root_hits.sum())
    summary: dict[str, int | str] = {
        "run": run_name,
        "entries": n_entries,
        "root_total_hits": root_total,
        "root_files": len(root_paths),
    }

    if txt_hits is None:
        fig, ax = plt.subplots(figsize=(10, 6.2), constrained_layout=True)
        add_hitmap(root_hits, ax, f"ROOT {branch_name}")
        fig.suptitle(
            f"{run_name}\nentries={n_entries:,}  root_hits={root_total:,}  root_files={len(root_paths)}",
            fontsize=12,
        )
    else:
        diff = root_hits - txt_hits
        txt_total = int(txt_hits.sum())
        mismatched = int(np.count_nonzero(diff))
        max_abs_diff = int(np.max(np.abs(diff)))
        summary.update(
            {
                "txt_total_hits": txt_total,
                "n_mismatched_channels": mismatched,
                "max_abs_diff": max_abs_diff,
            }
        )

        fig, axes = plt.subplots(1, 3, figsize=(16, 5.8), constrained_layout=True)
        add_hitmap(root_hits, axes[0], f"ROOT {branch_name}")
        add_hitmap(txt_hits, axes[1], "hitsHistogram.txt")
        add_hitmap(diff, axes[2], "ROOT - hitsHistogram.txt")
        fig.suptitle(
            f"{run_name}\n"
            f"entries={n_entries:,}  root_hits={root_total:,}  txt_hits={txt_total:,}  "
            f"mismatched={mismatched}  max_abs_diff={max_abs_diff}  root_files={len(root_paths)}",
            fontsize=12,
        )

    pdf.savefig(fig, dpi=180)
    plt.close(fig)
    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-root", type=Path, default=DEFAULT_DATA_ROOT)
    parser.add_argument("--run-dir", type=Path, default=None, help="Process one run directory instead of all children.")
    parser.add_argument("--root-file", type=Path, action="append", default=None, help="ROOT file for --run-dir. Can be repeated.")
    parser.add_argument("--output-pdf", type=Path, default=DEFAULT_OUTPUT_PDF)
    parser.add_argument("--branch", default="hitbit_high")
    parser.add_argument("--slab-index", type=int, default=0)
    parser.add_argument("--require-hits-txt", action="store_true")
    args = parser.parse_args()

    data_root = args.data_root.resolve()
    output_pdf = args.output_pdf.resolve()
    output_pdf.parent.mkdir(parents=True, exist_ok=True)

    run_dirs = [args.run_dir.resolve()] if args.run_dir else iter_run_dirs(data_root)
    summaries = []
    skipped = []

    with PdfPages(output_pdf) as pdf:
        for run_dir in run_dirs:
            root_paths = [path.resolve() for path in args.root_file] if args.root_file else find_root_files(run_dir)
            if not root_paths:
                skipped.append((run_dir.name, "no converted_*.root"))
                continue

            hits_path = run_dir / "hitsHistogram.txt"
            if hits_path.exists():
                txt_hits = parse_hits_histogram(hits_path)
            elif args.require_hits_txt:
                skipped.append((run_dir.name, "no hitsHistogram.txt"))
                continue
            else:
                txt_hits = None

            print(f"[plot] {run_dir.name}: {len(root_paths)} ROOT file(s)")
            root_hits, n_entries = count_root_hits(root_paths, args.branch, args.slab_index)
            summaries.append(
                save_run_page(pdf, run_dir, root_paths, root_hits, txt_hits, n_entries, args.branch)
            )

    print(f"output_pdf: {output_pdf}")
    print(f"runs_plotted: {len(summaries)}")
    if skipped:
        print(f"runs_skipped: {len(skipped)}")
        for run_name, reason in skipped:
            print(f"[skip] {run_name}: {reason}")
    for summary in summaries:
        line = (
            f"{summary['run']}: entries={summary['entries']} "
            f"root_hits={summary['root_total_hits']} root_files={summary['root_files']}"
        )
        if "txt_total_hits" in summary:
            line += (
                f" txt_hits={summary['txt_total_hits']} "
                f"mismatched={summary['n_mismatched_channels']} "
                f"max_abs_diff={summary['max_abs_diff']}"
            )
        print(line)


if __name__ == "__main__":
    main()
