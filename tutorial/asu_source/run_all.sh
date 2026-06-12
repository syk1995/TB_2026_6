#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

bash "${SCRIPT_DIR}/scripts/decode_sample.sh"
bash "${SCRIPT_DIR}/scripts/plot_sample_hitmap.sh"

