#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DATA_ROOT="${1:-/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/source_asu}"
DECODE_MACROS="${REPO_ROOT}/Decode/macros"
KEY4HEP_SETUP="${KEY4HEP_SETUP:-/cvmfs/sw.hsf.org/key4hep/setup.sh}"
ROOT_ENV="${ROOT_ENV:-}"
CONDA_ENV="${CONDA_ENV:-root_torch}"
FORCE="${FORCE:-0}"

if [[ ! -d "${DATA_ROOT}" ]]; then
  echo "Missing data root: ${DATA_ROOT}" >&2
  exit 2
fi

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

decode_decoded_bin_dir() {
  local run_dir="$1"
  local run_name
  run_name="$(basename "${run_dir}")"
  local output="${run_dir}/converted_${run_name}_decoded_bin.root"

  if [[ "${FORCE}" != "1" && -s "${output}" ]]; then
    echo "[skip decoded-bin] ${run_name}: ${output}"
    return
  fi

  echo "[decode decoded-bin] ${run_name}"
  root -l -b -q \
    -e ".L ${DECODE_MACROS}/SLBdecodedBin2ROOT.cc" \
    -e "ConvertDecodedBinRunDirectory(\"${run_dir}\",\"${output}\",true,false)"
}

decode_ascii_dat_dir() {
  local run_dir="$1"
  local run_name
  run_name="$(basename "${run_dir}")"

  if [[ "${FORCE}" != "1" ]] && find "${run_dir}" -maxdepth 1 -type f -name 'converted_*.dat*.root' | grep -q .; then
    echo "[skip ascii-dat] ${run_name}: converted ROOT already exists"
    return
  fi

  echo "[decode ascii-dat] ${run_name}"
  root -l -b -q \
    -e ".L ${DECODE_MACROS}/ConvertRunDirectory.cc" \
    -e "ConvertRunDirectory(\"${run_dir}\",\"${run_dir}\",\"ascii\",\"\",false,true,true)"
}

for run_dir in "${DATA_ROOT}"/*; do
  [[ -d "${run_dir}" ]] || continue

  first_bin="$(find "${run_dir}" -maxdepth 1 -type f -name '*.bin*' ! -name '*.root' | sort | head -n 1 || true)"
  first_dat="$(find "${run_dir}" -maxdepth 1 -type f -name '*.dat*' ! -name '*.root' | sort | head -n 1 || true)"

  if [[ -n "${first_bin}" ]]; then
    if head -c 4096 "${first_bin}" | grep -q 'DATA STRUCTURE INFO : DECODED FRAMES'; then
      decode_decoded_bin_dir "${run_dir}"
    else
      echo "[warn] unsupported binary framing in ${run_dir}; not decoded" >&2
    fi
  elif [[ -n "${first_dat}" ]]; then
    decode_ascii_dat_dir "${run_dir}"
  else
    echo "[skip] $(basename "${run_dir}"): no .bin/.dat input"
  fi
done
