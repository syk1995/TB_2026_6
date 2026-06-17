# TB2026-06 Analysis Code

SiWECAL TB2026-06 commissioning utilities for decoding data, checking hitmaps,
running threshold-scan studies, and digitizing simulated energy deposits with a
cell-shaping model.

This repository keeps code and lightweight notebooks only. Large input data,
converted ROOT files, CMake build directories, Python caches, and generated
plots are intentionally kept outside Git.

## Layout

- `Decode/`: ROOT macros and shell wrappers for converting TB2026-06
  commissioning data into ROOT files. See `Decode/README.md`.
- `Digitization/`: C++/pybind11 implementation of the SiWECAL cell shaping
  model, plus calibration and demo notebooks. See `Digitization/README`.
- `ThresholdScan/`: notebooks and helper script for TB2026-06 commissioning
  threshold-scan data.
- `asu_source/`: decoded-frame source-ASU conversion and hitmap plotting
  helpers.
- `tutorial/`: small self-contained examples with sample input data and scripts.

## Data Locations

The scripts assume the local TB2026-06 data area:

```text
/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06
```

Important subdirectories used by the current scripts:

```text
Comission/tdc
Comission/source_asu
Comission/ThresholdScan
CONF6
```

Most scripts allow overriding the data root through command-line options or
environment variables. Check each script's `--help` output before running on a
different machine.

## Environment

### Default environment

The decode and plotting tools now use the `key4hep` environment by default:

```bash
source /cvmfs/sw.hsf.org/key4hep/setup.sh
```

On `lxplus`, the repository was tested on June 17, 2026 with:

```text
source /cvmfs/sw.hsf.org/key4hep/setup.sh -r 2026-04-08
```

Check the active ROOT and Python before running the conversion macros or notebooks:

```bash
which root
root-config --version
which python3
python3 -c "import sys, ROOT; print(sys.executable); print(ROOT.gROOT.GetVersion())"
```

Example output on the tested machine:

```text
/cvmfs/sft.cern.ch/lcg/views/.../bin/root
6.38.04
/cvmfs/sft.cern.ch/lcg/views/.../bin/python3
```

The tutorial and decode wrappers source `key4hep` automatically if
`/cvmfs/sw.hsf.org/key4hep/setup.sh` exists.

### Legacy fallback

```bash
ROOT_ENV=/path/to/conda.sh CONDA_ENV=root_torch bash tutorial/asu_source/run_all.sh
CONDA_SH=/path/to/conda.sh ROOT_TORCH_ENV=/path/to/env bash Decode/convert_tdc_runs.sh
```

This keeps the older `root_torch` setup available on non-`key4hep` machines.

If the plotting step complains about a missing `uproot` module on lxplus,
install it in your user area:

```bash
python3 -m pip install --user uproot
```

Then the tutorial can be run directly:

```bash
source /cvmfs/sw.hsf.org/key4hep/setup.sh
cd tutorial/asu_source
bash run_all.sh
```

## Decode Data

The decode tools convert several TB2026-06 input formats into ROOT files with a
`siwecaldecoded` tree. See `Decode/README.md` for full options and format
details.

ASCII decoded `.dat/.dat_000N` files are handled by the legacy decoded-data
backend:

```bash
cd Decode
bash convert_tdc_runs.sh
```

Convert one run directory when using the TDC batch wrapper:

```bash
bash convert_tdc_runs.sh --run 15
```

Decoded-frame binary `.bin/.bin_000N` files, whose header says
`DATA STRUCTURE INFO : DECODED FRAMES`, are handled by the decoded-frame binary
converter. The source-ASU batch helper auto-detects these files and writes ROOT
outputs back into each input run directory:

```bash
cd asu_source
bash decode_source_asu_all.sh
```

Raw binary `.raw` or `*raw.bin*` files use the raw-frame backend. The TB2026-06
wrapper currently uses the non-EUDAQ raw reader for SL software raw frames.
Do not use the raw decoder for decoded-frame `.bin` files.

Converted ROOT files are written next to the input data unless a helper script
documents a different output path.

Quick hitmap checks are available after conversion:

```bash
python plot_decoded_bin_root_hitmap.py
```

## Threshold Scan

Download commissioning threshold-scan data through lxplus:

```bash
cd ThresholdScan
bash download_commissioning_thresholdscan.sh --dry-run
bash download_commissioning_thresholdscan.sh
```

Use the notebooks in `ThresholdScan/` for hitmap and rate-vs-threshold checks.

## Digitization

Build the C++ test executable:

```bash
cd Digitization
cmake -S . -B build
cmake --build build
./build/test_cell_shaping
```

The main header-only implementation is:

```text
Digitization/include/CellShaping.hh
Digitization/include/Shaping_FastVersion.hh
```

The current default MIP calibration for CONF6 with 0.5 mm silicon is:

```text
0.1472 MeV/MIP
```

See `Digitization/README` for the full shaping configuration and Python module
build instructions.

## Useful Links

- TB2025 legacy analysis code:
  <https://github.com/SiWECAL-TestBeam/SiWECAL-TB-analysis>
- TB2025 monitoring code:
  <https://github.com/SiWECAL-TestBeam/SiWECAL-TB-monitoring>
- TB2025 local analysis scripts used as references in this workspace:
  <https://github.com/syk1995/TB>
- AHCAL raw/BIF utility code used by the older TB workflow:
  <https://github.com/jkvas/AHCAL-RAWutils>
- SKIROC2 datasheet:
  <https://www.weeroc.com/~documents/products/skiroc-2a/skiroc-2-datasheet/?layout=file>

## Notes

- The repository does not track converted `.root` files or raw test-beam data.
- Generated plots and local CMake build folders should be regenerated as needed.
- Notebook outputs may contain exploratory results; clear outputs before
  publishing if a clean history is preferred.
