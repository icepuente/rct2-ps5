#!/usr/bin/env bash
# Cross-compile OpenRCT2 as a PS5 payload. Runs inside the toolchain container
# (see scripts/build.sh), with prospero.sh already sourced.
set -euo pipefail

SRC=openrct2
BUILD=build/openrct2

# Apply this repository's fixes to the pinned OpenRCT2 source (idempotent).
for patch in patches/openrct2/*.patch; do
    if patch -d "${SRC}" -p1 -R --dry-run -s -f < "${patch}" > /dev/null 2>&1; then
        continue # already applied
    fi
    patch -d "${SRC}" -p1 -s -N < "${patch}" || { echo "cannot apply ${patch}" >&2; exit 1; }
done

# DOCDIR (OpenRCT2's licence and changelog) becomes /app0/assets/doc: the
# install prefix is only used for that, since nothing is installed.
#
# OpenRCT2 builds with -fno-char8_t, but nlohmann/json's C++20 std::filesystem
# conversions need char8_t. OpenRCT2 doesn't serialise paths, so turn them off.
CXX_FLAGS="-DJSON_HAS_FILESYSTEM=0 -DJSON_HAS_EXPERIMENTAL_FILESYSTEM=0"

# OpenRCT2 only links ICU's uc component; the static libicuuc also needs the
# (filtered) data library, which has to come last on the link line.
EXTRA_LIBS="${PS5_SYSROOT}${PS5_HBROOT}/lib/libicudata.a"

${CMAKE} -S "${SRC}" -B "${BUILD}" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_PREFIX_PATH="${PS5_SYSROOT}${PS5_HBROOT}" \
    -DCMAKE_CXX_FLAGS="${CXX_FLAGS}" \
    -DSTATIC=OFF \
    -DDISABLE_IPO=ON \
    -DCMAKE_CXX_STANDARD_LIBRARIES="${EXTRA_LIBS}" \
    -DCMAKE_INSTALL_PREFIX=/app0 \
    -DCMAKE_INSTALL_DOCDIR=assets/doc \
    -DDISABLE_OPENGL=ON \
    -DDISABLE_NETWORK=ON \
    -DDISABLE_HTTP=ON \
    -DDISABLE_DISCORD_RPC=ON \
    -DDISABLE_TTF=ON \
    -DDISABLE_VERSION_CHECKER=ON \
    -DENABLE_SCRIPTING=ON \
    -DWITH_TESTS=OFF \
    -DOPENRCT2_USE_CCACHE=OFF \
    -DDOWNLOAD_TITLE_SEQUENCES=OFF \
    -DDOWNLOAD_OBJECTS=OFF \
    -DDOWNLOAD_OPENSFX=OFF \
    -DDOWNLOAD_OPENMUSIC=OFF

ninja -C "${BUILD}" "$@"
