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

The decode and plotting tools use the existing `root_torch` conda environment:

```bash
source /data_ilc/flc/shi/miniconda3/etc/profile.d/conda.sh
conda activate root_torch
```

This environment currently provides ROOT:

```text
ROOT 6.36.04
```

Check the active ROOT and Python before running the conversion macros or
notebooks:

```bash
which root
root-config --version
which python
python -c "import sys, ROOT; print(sys.executable); print(ROOT.gROOT.GetVersion())"
```

Expected paths and version:

```text
/data_ilc/flc/shi/miniconda3/envs/root_torch/bin/root
6.36.04
/data_ilc/flc/shi/miniconda3/envs/root_torch/bin/python
```

Avoid mixing this conda Python with a different system ROOT installation. The
decode macros and notebooks assume the ROOT and PyROOT libraries from the same
`root_torch` environment.

The digitization notebooks and pybind11 build may also use the TB2026/key4hep
environment documented in `Digitization/README`.

## Decode TDC Runs

Convert all commissioning TDC `ilc_run_*` directories:

```bash
cd Decode
bash convert_tdc_runs.sh
```

Convert one run:

```bash
bash convert_tdc_runs.sh --run 15
```

The converted ROOT files are written next to the input `.dat` files.

## Decode Source-ASU Runs

Batch-decode source-ASU input directories:

```bash
cd asu_source
bash decode_source_asu_all.sh
```

The helper detects decoded-frame `.bin/.bin_000N` runs and ASCII `.dat/.dat_000N`
runs, then writes converted ROOT files back into each input run directory.

Plot all decoded source-ASU hitmaps:

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

## Notes

- The repository does not track converted `.root` files or raw test-beam data.
- Generated plots and local CMake build folders should be regenerated as needed.
- Notebook outputs may contain exploratory results; clear outputs before
  publishing if a clean history is preferred.
