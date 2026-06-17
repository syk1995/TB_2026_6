#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TUTORIAL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${TUTORIAL_DIR}/../.." && pwd)"

RUN_NAME="nosource_asu_2026_002_th250_run_000012"
RUN_DIR="${TUTORIAL_DIR}/data/${RUN_NAME}"
OUTPUT_DIR="${TUTORIAL_DIR}/output"
OUTPUT_ROOT="${OUTPUT_DIR}/converted_${RUN_NAME}_decoded_bin.root"

KEY4HEP_SETUP="${KEY4HEP_SETUP:-/cvmfs/sw.hsf.org/key4hep/setup.sh}"
ROOT_ENV="${ROOT_ENV:-}"
CONDA_ENV="${CONDA_ENV:-root_torch}"

if [[ ! -f "${RUN_DIR}/${RUN_NAME}.bin" ]]; then
  echo "Missing tutorial input: ${RUN_DIR}/${RUN_NAME}.bin" >&2
  exit 2
fi

mkdir -p "${OUTPUT_DIR}"

if [[ -n "${KEY4HEP_STACK:-}" ]]; then
  :
elif [[ -f "${KEY4HEP_SETUP}" ]]; then
  set +u
  source "${KEY4HEP_SETUP}"
  set -u
elif [[ -n "${ROOT_ENV}" && -f "${ROOT_ENV}" ]]; then
  set +u
  source "${ROOT_ENV}"
  conda activate "${CONDA_ENV}"
  set -u
elif ! command -v root >/dev/null 2>&1; then
  echo "Missing ROOT executable and environment setup scripts." >&2
  echo "Set KEY4HEP_SETUP=/path/to/setup.sh, ROOT_ENV=/path/to/conda.sh, or make sure 'root' is on PATH." >&2
  exit 2
fi

root -l -b -q \
  -e ".L ${REPO_ROOT}/Decode/macros/SLBdecodedBin2ROOT.cc" \
  -e "ConvertDecodedBinRunDirectory(\"${RUN_DIR}\",\"${OUTPUT_ROOT}\",true,false)"

echo "decoded_root: ${OUTPUT_ROOT}"
