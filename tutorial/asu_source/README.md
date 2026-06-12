# Source-ASU Decode Tutorial

This tutorial uses one small undecoded source-ASU decoded-frame binary file:

```text
data/nosource_asu_2026_002_th250_run_000012/nosource_asu_2026_002_th250_run_000012.bin
```

Run the full workflow:

```bash
bash run_all.sh
```

Or run the two steps separately:

```bash
bash scripts/decode_sample.sh
bash scripts/plot_sample_hitmap.sh
```

The scripts use the existing `root_torch` conda environment by default:

```bash
/data_ilc/flc/shi/miniconda3/etc/profile.d/conda.sh
root_torch
```

Override them if needed:

```bash
ROOT_ENV=/path/to/conda.sh CONDA_ENV=my_env bash run_all.sh
```

Outputs are written to:

```text
output/converted_nosource_asu_2026_002_th250_run_000012_decoded_bin.root
output/nosource_asu_2026_002_th250_run_000012_hitmap.pdf
```

The output directory is ignored by Git.
