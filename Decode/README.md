# TB2026-06 Decode

SiWECAL TB2026-06 commissioning data decode and quick hitmap checks.

This directory keeps the old TB decoder macros as backends, but exposes only a
small set of wrappers for normal use.

## Layout

- `macros/SLBdecoded2ROOT.cc`: legacy ASCII `.dat` decoder kept as the backend.
- `macros/SLBraw2ROOT.cc`: legacy binary raw decoder kept as the backend.
- `macros/SLBdecodedBin2ROOT.cc`: binary decoded-frame converter for SL
  software files whose header says `DATA STRUCTURE INFO : DECODED FRAMES`.
- `macros/ConvertFile.cc`: one-file wrapper for either backend.
- `macros/ConvertRunDirectory.cc`: one directory wrapper for split run files.
- `convert_tdc_runs.sh`: batch converter for TB2026-06 commissioning TDC runs.
- `Hitmap.ipynb`: quick high gain mean hitmap from converted ROOT files.
- `mapping/`: FEV channel-to-xy maps used by the quick plots.

## Environment

Use the `key4hep` environment by default:

```bash
source /cvmfs/sw.hsf.org/key4hep/setup.sh
```

After activation, check the matched ROOT/Python stack:

```bash
which root
root-config --version
which python3
python3 -c "import sys, numpy, matplotlib, uproot; print(sys.executable)"
```

Example output on the tested lxplus machine:

```text
/cvmfs/sft.cern.ch/lcg/views/.../bin/python3
```

The batch wrappers source `/cvmfs/sw.hsf.org/key4hep/setup.sh` automatically if
it exists. On machines without `key4hep`, keep using the older `root_torch`
setup:

```bash
source /data_ilc/flc/shi/miniconda3/etc/profile.d/conda.sh
conda activate root_torch
```

## Convert TDC Data

The input commissioning TDC data are under:

```text
/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/tdc
```

Convert all `ilc_run_*` directories:

```bash
cd /home/llr/ilc/shi/code/TB_2026_6/Decode
bash convert_tdc_runs.sh
```

Convert one run:

```bash
bash convert_tdc_runs.sh --run 15
```

Force recreation of already existing ROOT files:

```bash
bash convert_tdc_runs.sh --run 15 --force
```

The script writes ROOT files next to the input `.dat` files. For example:

```text
/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/tdc/ilc_run_000015/converted_ilc_run_000015.dat.root
```

Useful options:

```bash
bash convert_tdc_runs.sh --help
bash convert_tdc_runs.sh --dry-run
bash convert_tdc_runs.sh --data-root /path/to/tdc
```

## Raw Binary Format Notes

`SLBraw2ROOT.cc` is the common backend for raw binary files. It can handle
both `.raw` and `*raw.bin*` inputs, but the correct reader is selected by the
internal file framing, not by the filename suffix alone:

- EUDAQ-style raw files start with `0xABCD`; use the EUDAQ path
  (`_eudaq = true`).
- SL software non-EUDAQ raw files start frames with `0xEEEEEEEE`; use the
  non-EUDAQ path (`_eudaq = false`).

The TB2026 wrapper currently uses the non-EUDAQ raw-frame reader:

```cpp
_eudaq = false;
```

This matches the old `script_yukun/conversion/run_conversion.sh` note for
`*raw.bin*` files. Use `ConvertFile.cc(..., "raw", ...)` for one non-EUDAQ
raw file, or adapt `ConvertRunDirectory.cc` for a run directory.

Do not use this raw decoder for files whose header says:

```text
DATA STRUCTURE INFO : DECODED FRAMES
```

Those files are already decoded-frame binary dumps from the SL software. They
also often have a `.bin` suffix, but their internal layout is different from
raw frames, and `SLBraw2ROOT.cc` will not produce a valid ROOT file from them.

Convert a decoded-frame `.bin` run directory with:

```bash
cd /home/llr/ilc/shi/code/TB_2026_6/Decode/macros
root -l -b -q \
  -e '.L SLBdecodedBin2ROOT.cc' \
  -e 'ConvertDecodedBinRunDirectory("/path/to/run_dir")'
```

The default output is written in the input run directory as:

```text
converted_<run_dir_name>_decoded_bin.root
```

The converter reads all `.bin` and `.bin_000N` files in that run directory and
writes a `siwecaldecoded` tree with the same main branch layout as the legacy
SLB converters.

For the TB2026-06 `source_asu` directory, the batch helper is:

```bash
/home/llr/ilc/shi/code/TB_2026_6/asu_source/decode_source_asu_all.sh
```

It scans each subdirectory under:

```text
/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/source_asu
```

and writes ROOT outputs back into the corresponding input subdirectory. It uses
`SLBdecodedBin2ROOT.cc` for decoded-frame `.bin/.bin_000N` runs and
`ConvertRunDirectory.cc` for ASCII `.dat/.dat_000N` runs.

## Hitmap Notebook

Open:

```bash
cd /home/llr/ilc/shi/code/TB_2026_6/Decode
jupyter lab Hitmap.ipynb
```

Use a Python kernel from `key4hep` when available, or `root_torch` on the older
LLR setup. In the first notebook code cell, set:

- `RUN_NUMBER`: normal case, for example `15`.
- `ROOT_FILE_PATH`: optional direct path to a converted ROOT file or run
  directory. Leave it as `None` to use `RUN_NUMBER`.

The default notebook input is:

```text
/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/tdc/ilc_run_000015/converted_ilc_run_000015.dat.root
```

The notebook reads `adc_high` from the `siwecaldecoded` tree directly in
Python. The usual high-level `uproot` array reader can hang on this fixed C
array branch, so the notebook reads each `TBasket.data` block and reshapes it
as `adc_high[15][16][15][64]`.

Important ROOT format detail:

- `adc_high` is written as
  `adc_high[15][16][15][64]`.
- The dimensions are `slboard/slab address`, `chip`, `SCA`, `channel`.
- `SLBdecoded2ROOT.cc` initializes missing or unfilled bins to `-999`.
- Analysis must drop `-999`; it is not a physical ADC value and is not drawn
  in the 1D histogram or used in channel means.
- In run 15, the connected slab is stored at slab address `12`, while unused
  slab addresses remain `-999`.

Plot binning:

- XY geometry hitmap: `32 x 32`.
- Chip/channel hitmap: `16 x 64`.

The plotting cells use `matplotlib.pyplot.hist2d`. Plots are shown inline and
written next to the ROOT/dat file. For run 15:

```text
/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/tdc/ilc_run_000015
```

Typical output names:

```text
ilc_run_000015_adc_high_hist1d.png
ilc_run_000015_adc_high_mean_hitmap.png
ilc_run_000015_adc_high_mean_chip_channel.png
```
