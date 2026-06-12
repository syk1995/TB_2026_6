#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TUTORIAL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${TUTORIAL_DIR}/../.." && pwd)"

RUN_NAME="nosource_asu_2026_002_th250_run_000012"
RUN_DIR="${TUTORIAL_DIR}/data/${RUN_NAME}"
OUTPUT_DIR="${TUTORIAL_DIR}/output"
ROOT_FILE="${OUTPUT_DIR}/converted_${RUN_NAME}_decoded_bin.root"
OUTPUT_PDF="${OUTPUT_DIR}/${RUN_NAME}_hitmap.pdf"

ROOT_ENV="${ROOT_ENV:-/data_ilc/flc/shi/miniconda3/etc/profile.d/conda.sh}"
CONDA_ENV="${CONDA_ENV:-root_torch}"

if [[ ! -s "${ROOT_FILE}" ]]; then
  echo "Missing decoded ROOT file: ${ROOT_FILE}" >&2
  echo "Run scripts/decode_sample.sh first." >&2
  exit 2
fi

if [[ -f "${ROOT_ENV}" ]]; then
  set +u
  source "${ROOT_ENV}"
  conda activate "${CONDA_ENV}"
  set -u
fi

python "${REPO_ROOT}/asu_source/plot_decoded_bin_root_hitmap.py" \
  --run-dir "${RUN_DIR}" \
  --root-file "${ROOT_FILE}" \
  --output-pdf "${OUTPUT_PDF}" \
  --slab-index 0

echo "hitmap_pdf: ${OUTPUT_PDF}"
