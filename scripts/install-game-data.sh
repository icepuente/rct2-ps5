#!/usr/bin/env bash
# Upload your RollerCoaster Tycoon 2 files into the installed title, where
# OpenRCT2 finds them at /app0/assets/rct2.
#
# Usage: PS5_HOST=192.168.0.134 scripts/install-game-data.sh /path/to/rct2
#   The folder is the game's install directory (the one containing Data/g1.dat),
#   e.g. a Steam depot download of app 285330. Nothing from it is ever added
#   to this repository.
set -euo pipefail

SRC="${1:?usage: $0 RCT2_DIR}"
TITLE_ID=PPSA99702
[[ -f "${SRC}/Data/g1.dat" || -f "${SRC}/data/g1.dat" ]] || {
    echo "${SRC} does not look like an RCT2 install (no Data/g1.dat)" >&2
    exit 1
}

# Stage the folders OpenRCT2 reads as symlinks; the rest of the install
# (installer cabinets, the Windows executable and the manual) is not used.
# OpenRCT2 looks legacy objects up by their upper-case names, and the console's
# filesystem is case-sensitive, so ObjData files are staged upper-case (some
# copies ship e.g. wallsign.dat and ssig4.dat).
SRC="$(cd "${SRC}" && pwd)"
STAGE="$(mktemp -d)"
trap 'rm -rf "${STAGE}"' EXIT
mkdir -p "${STAGE}/rct2"
for dir in "${SRC}"/*/; do
    name="$(basename "${dir}")"
    case "${name}" in
        Install) continue ;;
        ObjData)
            mkdir -p "${STAGE}/rct2/ObjData"
            for f in "${dir}"*; do
                ln -s "${f}" "${STAGE}/rct2/ObjData/$(basename "${f}" | tr '[:lower:]' '[:upper:]')"
            done
            ;;
        *) ln -s "${dir%/}" "${STAGE}/rct2/${name}" ;;
    esac
done
"$(dirname "$0")/upload.py" "${STAGE}/rct2" "/data/homebrew/${TITLE_ID}/assets"
