#!/usr/bin/env bash
# Build a release zip of the PPSA99702 title folder.
#
# Usage: scripts/make-release.sh VERSION      e.g. scripts/make-release.sh v0.1.0
# Output: build/release/OpenRCT2-PS5-VERSION.zip and .sha256
#
# The zip holds the PPSA99702 folder (with an empty assets/rct2 for the
# player's game files, and licences) and README.txt. No game data is included.
set -euo pipefail

VERSION="${1:?usage: $0 VERSION}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

scripts/fetch-openrct2-data.sh >/dev/null
OPENRCT2_PS5_DEBUG=0 OPENRCT2_PS5_AUTOTEST=0 scripts/build.sh openrct2

OUT="build/release"
STAGE="${OUT}/stage"
NAME="OpenRCT2-PS5-${VERSION}"
rm -rf "${STAGE}" "${OUT}/${NAME}.zip" "${OUT}/${NAME}.zip.sha256"
mkdir -p "${STAGE}"
cp -a build/dist/PPSA99702 "${STAGE}/"
rm -rf "${STAGE}/PPSA99702/assets/autotest"

mkdir -p "${STAGE}/PPSA99702/assets/rct2"
cat > "${STAGE}/PPSA99702/assets/rct2/PUT-YOUR-RCT2-FILES-HERE.txt" <<'TXT'
Copy these folders from your own RollerCoaster Tycoon 2 installation into
this folder (the one that contains Data/g1.dat):

  Data  ObjData  Scenarios  Tracks  Landscapes  Saved Games

The Steam "Triple Thrill Pack" (app 285330) and GOG versions both work.
The console's file system is case sensitive: files in ObjData must have
upper-case names (some copies ship wallsign.dat, ssig4.dat and Cerberus.dat;
rename them to WALLSIGN.DAT, SSIG4.DAT and CERBERUS.DAT).
TXT

mkdir -p "${STAGE}/PPSA99702/licenses"
cp LICENSE "${STAGE}/PPSA99702/licenses/GPL-3.0.txt"
cp THIRD-PARTY-NOTICES.md "${STAGE}/PPSA99702/licenses/"
[[ -f build/openrct2-doc/licence.txt ]] && cp build/openrct2-doc/licence.txt "${STAGE}/PPSA99702/licenses/OpenRCT2-licence.txt"

cat > "${STAGE}/README.txt" <<TXT
OpenRCT2 for PS5 ${VERSION}
https://github.com/icepuente/rct2-ps5

1. Copy your RollerCoaster Tycoon 2 files into PPSA99702/assets/rct2
   (see the note in that folder).
2. Upload the whole PPSA99702 folder to your console's homebrew folder,
   e.g. /data/homebrew/PPSA99702, with ps5upload, FTP or a file manager.
3. Let ShadowMountPlus (or your loader) register it, then start
   "OpenRCT2" from the home screen. The first start indexes about 2,500
   objects and takes about 20 seconds.

Saves and settings are kept in the title's own storage and survive updates.
To update, replace PPSA99702 except assets/rct2.
TXT

(cd "${STAGE}" && zip -qr -X "../${NAME}.zip" PPSA99702 README.txt)
(cd "${OUT}" && shasum -a 256 "${NAME}.zip" > "${NAME}.zip.sha256")
echo "Release: ${OUT}/${NAME}.zip ($(du -h "${OUT}/${NAME}.zip" | cut -f1))"
cat "${OUT}/${NAME}.zip.sha256"
