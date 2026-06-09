#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MACRO_DIR="${SCRIPT_DIR}/macros"
DATA_ROOT="${DATA_ROOT:-${HOME}/data/SiWECAL-Prototype/TB2026-06/Comission/tdc}"
CONDA_SH="${CONDA_SH:-/data_ilc/flc/shi/miniconda3/etc/profile.d/conda.sh}"
ROOT_TORCH_ENV="${ROOT_TORCH_ENV:-/data_ilc/flc/shi/miniconda3/envs/root_torch}"
MODE="ascii"
FORCE=0
DRY_RUN=0
RUNS=()

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Convert TB2026-06 commissioning TDC runs to ROOT.
Output ROOT files are written next to the input .dat files:
  <run_dir>/converted_<dat_file>.root

Options:
  --data-root PATH   Directory containing ilc_run_* folders.
                     Default: ${DATA_ROOT}
  --run N            Convert one run number, e.g. --run 15.
                     Can be repeated. Default: all ilc_run_* folders.
  --mode MODE        Converter mode: ascii or raw. Default: ascii.
  --force            Recreate ROOT files even if all expected outputs exist.
  --dry-run          Print commands without running ROOT.
  -h, --help         Show this help.

Environment:
  Uses the root_torch conda environment by default:
    ${ROOT_TORCH_ENV}
  Override with ROOT_TORCH_ENV=/path/to/env if needed.

Examples:
  $(basename "$0")
  $(basename "$0") --run 15
  $(basename "$0") --data-root ~/data/SiWECAL-Prototype/TB2026-06/Comission/tdc --force
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --data-root)
      DATA_ROOT="$2"
      shift 2
      ;;
    --run)
      if [[ "$2" == ilc_run_* ]]; then
        RUNS+=("$2")
      else
        RUNS+=("$(printf "ilc_run_%06d" "$2")")
      fi
      shift 2
      ;;
    --mode)
      MODE="$2"
      shift 2
      ;;
    --force)
      FORCE=1
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

DATA_ROOT="${DATA_ROOT/#\~/${HOME}}"

if [[ ! -d "${DATA_ROOT}" ]]; then
  echo "Data root does not exist: ${DATA_ROOT}" >&2
  exit 1
fi

if [[ "${DRY_RUN}" -eq 0 ]]; then
  if [[ -f "${CONDA_SH}" ]]; then
    # root_torch needs conda activation so ROOT/Cling sees its compiler paths.
    # shellcheck disable=SC1090
    set +u
    source "${CONDA_SH}"
    conda activate "${ROOT_TORCH_ENV}"
    set -u
  elif [[ -x "${ROOT_TORCH_ENV}/bin/root" ]]; then
    export PATH="${ROOT_TORCH_ENV}/bin:${PATH}"
  fi

  if ! command -v root >/dev/null 2>&1; then
    echo "ROOT is not available. Check ROOT_TORCH_ENV or CONDA_SH." >&2
    exit 1
  fi
fi

if [[ "${#RUNS[@]}" -eq 0 ]]; then
  while IFS= read -r run_dir; do
    RUNS+=("$(basename "${run_dir}")")
  done < <(find -L "${DATA_ROOT}" -maxdepth 1 -type d -name 'ilc_run_*' | sort)
fi

if [[ "${#RUNS[@]}" -eq 0 ]]; then
  echo "No ilc_run_* directories found under ${DATA_ROOT}" >&2
  exit 1
fi

root_quote() {
  local value="$1"
  value="${value//\\/\\\\}"
  value="${value//\"/\\\"}"
  printf '%s' "${value}"
}

run_has_all_outputs() {
  local run_dir="$1"
  local run_name="$2"
  local input_count=0
  local missing_count=0
  local dat_file
  local base
  local root_file

  shopt -s nullglob
  for dat_file in "${run_dir}/${run_name}.dat" "${run_dir}/${run_name}.dat_"*; do
    [[ -f "${dat_file}" ]] || continue
    input_count=$((input_count + 1))
    base="$(basename "${dat_file}")"
    root_file="${run_dir}/converted_${base}.root"
    [[ -f "${root_file}" ]] || missing_count=$((missing_count + 1))
  done
  shopt -u nullglob

  [[ "${input_count}" -gt 0 && "${missing_count}" -eq 0 ]]
}

for run_name in "${RUNS[@]}"; do
  run_dir="${DATA_ROOT}/${run_name}"
  if [[ ! -d "${run_dir}" ]]; then
    echo "Skip missing run directory: ${run_dir}" >&2
    continue
  fi

  if [[ "${FORCE}" -eq 0 ]] && run_has_all_outputs "${run_dir}" "${run_name}"; then
    echo "[skip] ${run_name}: all converted ROOT files already exist"
    continue
  fi

  echo "[convert] ${run_name}"
  echo "          input/output: ${run_dir}"

  root_cmd=(
    root -l -b -q
    -e ".L $(root_quote "${MACRO_DIR}/ConvertRunDirectory.cc")"
    -e "ConvertRunDirectory(\"$(root_quote "${run_dir}")\",\"$(root_quote "${run_dir}")\",\"$(root_quote "${MODE}")\",\"$(root_quote "${run_name}")\",false,true,true)"
  )

  if [[ "${DRY_RUN}" -eq 1 ]]; then
    printf '  '
    printf '%q ' "${root_cmd[@]}"
    printf '\n'
  else
    "${root_cmd[@]}"
  fi
done
