#!/usr/bin/env bash
# Fetch OpenRCT2's data directory (g2.dat, languages, objects, title
# sequences, sound packs) from the official release matching the openrct2
# submodule. Generating it from source needs a host build of OpenRCT2.
#
# Usage: scripts/fetch-openrct2-data.sh [OUT_DIR]   (default: build/openrct2-data)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(git -C "${ROOT}/openrct2" describe --tags --exact-match)"
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

DATA_PATH="$(tar -tzf "${ASSET}" | grep -m1 -E '(^|/)data/g2\.dat$' | sed 's|/g2\.dat$||')"
STRIP="$(awk -F/ '{print NF}' <<< "${DATA_PATH}")"
rm -rf "${OUT}"
mkdir -p "${OUT}"
tar -xzf "${ASSET}" -C "${OUT}" --strip-components="${STRIP}" "${DATA_PATH}"
echo "OpenRCT2 ${VERSION} data -> ${OUT} ($(du -sh "${OUT}" | cut -f1))"
