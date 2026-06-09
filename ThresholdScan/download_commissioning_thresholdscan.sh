#!/usr/bin/env bash
set -euo pipefail

# Download TB2026-06 Commissioning data through lxplus with rsync.
# lxplus can see /eos; this local server does not need EOS mounted.

USER_NAME="${CERN_USER:-shiy}"
HOST="${LXPLUS_HOST:-lxplus.cern.ch}"
REMOTE_DIR="${REMOTE_DIR:-/eos/project/s/siw-ecal/TB2026-06/Commissioning}"
DEST="${DEST:-/home/llr/ilc/shi/data/SiWECAL-Prototype/TB2026-06/Comission/ThresholdScan}"
DRY_RUN=0
DELETE_EXTRA=0

usage() {
  cat <<EOF
Usage: $(basename "$0") [--dry-run] [--delete] [--user USER]

Syncs from:
  ${USER_NAME}@${HOST}:${REMOTE_DIR}/

To:
  ${DEST}/

Options:
  --dry-run       show what would be copied
  --delete        delete local files that are not present remotely
  --user USER     lxplus/CERN username, default: ${USER_NAME}

Environment overrides:
  CERN_USER       lxplus/CERN username
  LXPLUS_HOST     default: lxplus.cern.ch
  REMOTE_DIR      remote EOS directory
  DEST            local destination directory

Example:
  $(basename "$0") --dry-run
  $(basename "$0")
EOF
}

while (($#)); do
  case "$1" in
    --dry-run)
      DRY_RUN=1
      ;;
    --delete)
      DELETE_EXTRA=1
      ;;
    --user)
      shift
      USER_NAME="${1:-}"
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
  shift
done

command -v rsync >/dev/null 2>&1 || { echo "Missing rsync" >&2; exit 127; }
command -v ssh >/dev/null 2>&1 || { echo "Missing ssh" >&2; exit 127; }

if [[ -z "$USER_NAME" ]]; then
  echo "ERROR: empty username. Use --user USER or set CERN_USER." >&2
  exit 2
fi

mkdir -p "$DEST"

rsync_args=(-avP)
if [[ "$DRY_RUN" == "1" ]]; then
  rsync_args+=(--dry-run)
fi
if [[ "$DELETE_EXTRA" == "1" ]]; then
  rsync_args+=(--delete)
fi

echo "Syncing from ${USER_NAME}@${HOST}:${REMOTE_DIR}/"
echo "Syncing to   ${DEST}/"
echo

rsync "${rsync_args[@]}" \
  -e ssh \
  "${USER_NAME}@${HOST}:${REMOTE_DIR}/" \
  "${DEST}/"

echo
echo "Done."
echo "Files are in: ${DEST}"
