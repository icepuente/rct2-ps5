#!/usr/bin/env bash
# Fetch OpenRCT2's data directory (g2.dat, languages, objects, title
# sequences, sound packs) from the official release matching the openrct2
# submodule. Generating it from source needs a host build of OpenRCT2.
#
# Usage: scripts/fetch-openrct2-data.sh [OUT_DIR]   (default: build/openrct2-data)
# The release's doc/ folder (licence, changelog) goes to OUT_DIR/../openrct2-doc.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# The release matching the openrct2 submodule (CI checkouts have no tags).
VERSION="$(cat "${ROOT}/openrct2.version")"
if TAG="$(git -C "${ROOT}/openrct2" describe --tags --exact-match 2>/dev/null)" && [[ "${TAG}" != "${VERSION}" ]]; then
    echo "openrct2 submodule is at ${TAG}, but openrct2.version says ${VERSION}" >&2
    exit 1
fi
OUT="${1:-${ROOT}/build/openrct2-data}"
CACHE="${ROOT}/build/cache"
ASSET="OpenRCT2-${VERSION}-Linux-noble-x86_64.tar.gz"
BASE="https://github.com/OpenRCT2/OpenRCT2/releases/download/${VERSION}"

mkdir -p "${CACHE}"
cd "${CACHE}"
[[ -f "${ASSET}" ]] || curl -fL -o "${ASSET}" "${BASE}/${ASSET}"
curl -fsSL -o "sha256sums-${VERSION}.txt" "${BASE}/OpenRCT2-${VERSION}-sha256sums.txt"
awk -v f="${ASSET}" '{sub(/^\.\//, "", $2)} $2 == f {print $1 "  " $2}' \
    "sha256sums-${VERSION}.txt" | grep . | shasum -a 256 -c -

# awk reads the whole listing: stopping early (grep -m1) makes GNU tar fail.
DATA_PATH="$(tar -tzf "${ASSET}" | awk '/(^|\/)data\/g2\.dat$/ && !found { sub(/\/g2\.dat$/, ""); print; found = 1 }')"
STRIP="$(awk -F/ '{print NF}' <<< "${DATA_PATH}")"
rm -rf "${OUT}"
mkdir -p "${OUT}"
tar -xzf "${ASSET}" -C "${OUT}" --strip-components="${STRIP}" "${DATA_PATH}"

DOC_OUT="$(dirname "${OUT}")/openrct2-doc"
DOC_PATH="${DATA_PATH%/data}/doc"
rm -rf "${DOC_OUT}"
mkdir -p "${DOC_OUT}"
tar -xzf "${ASSET}" -C "${DOC_OUT}" --strip-components="${STRIP}" "${DOC_PATH}"
echo "OpenRCT2 ${VERSION} data -> ${OUT} ($(du -sh "${OUT}" | cut -f1))"
