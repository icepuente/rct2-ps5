#!/usr/bin/env bash
# Upload your RollerCoaster Tycoon 2 files into the installed title, where
# OpenRCT2 finds them at /app0/assets/rct2.
#
# Usage: PS5_HOST=192.168.0.134 scripts/install-game-data.sh /path/to/rct2
#   The folder is the game's install directory (the one containing Data/g1.dat),
#   e.g. a Steam depot download of app 285330. It is uploaded as-is; nothing
#   from it is ever added to this repository.
set -euo pipefail

SRC="${1:?usage: $0 RCT2_DIR}"
TITLE_ID=PPSA99702
[[ -f "${SRC}/Data/g1.dat" || -f "${SRC}/data/g1.dat" ]] || {
    echo "${SRC} does not look like an RCT2 install (no Data/g1.dat)" >&2
    exit 1
}

# The rest of the install (installer cabinets, the Windows executable and the
# manual) is not used by OpenRCT2.
STAGE="$(mktemp -d)"
trap 'rm -rf "${STAGE}"' EXIT
ln -s "$(cd "${SRC}" && pwd)" "${STAGE}/rct2"
"$(dirname "$0")/upload.py" "${STAGE}/rct2/" "/data/homebrew/${TITLE_ID}/assets" \
    --exclude Install --exclude RCT2.EXE --exclude MANUAL.PDF \
    --exclude 285330_install.vdf --exclude Readme.txt
