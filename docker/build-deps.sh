#!/usr/bin/env bash
# Build and install only the pacbrew packages OpenRCT2 needs.
set -euo pipefail

REPO="$(realpath "$1")"

PKGS=(sdk openlibm libcxx
      bzip2 zlib xz zstd libzip
      libpng
      libsamplerate libogg libvorbis flac
      SDL2)

for PKG in "${PKGS[@]}"; do
    echo "==> Building ${PKG}"
    pushd "${REPO}/${PKG}" >/dev/null
    rm -rf src pkg ./*.pkg.tar.*
    makepkg -c -f -C --nocheck
    sudo pacman --config "${REPO}/pacman.conf" --noconfirm -U ./ps5-payload-*.pkg.tar.*
    popd >/dev/null
done
