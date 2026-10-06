#!/usr/bin/env bash
# Package the cross-compiled OpenRCT2 (scripts/build-openrct2.sh) as the
# native title PPSA99702. Runs inside the toolchain container.
#
# OPENRCT2_PS5_DEBUG=1 adds OpenRCT2's verbose log and the SDL display trace
# to the kernel log. OPENRCT2_PS5_AUTOTEST=1 installs the scenario tour plugin
# (ps5/autotest/ps5-autotest.js), which runs on the title screen.
set -euo pipefail

DEBUG="${OPENRCT2_PS5_DEBUG:-0}"
AUTOTEST="${OPENRCT2_PS5_AUTOTEST:-0}"
DEBUG_CFLAGS=()
[[ "${DEBUG}" == 1 ]] && DEBUG_CFLAGS+=(-DOPENRCT2_PS5_DEBUG)
[[ "${AUTOTEST}" == 1 ]] && DEBUG_CFLAGS+=(-DOPENRCT2_PS5_AUTOTEST)

TITLE_ID=PPSA99702
BUILD=build/openrct2
DATA=build/openrct2-data
WORK=build/openrct2-native
STAGE="${WORK}/assets"
HB="${PS5_SYSROOT}${PS5_HBROOT}/lib"

[[ -f "${DATA}/g2.dat" ]] || { echo "run scripts/fetch-openrct2-data.sh first" >&2; exit 1; }

mkdir -p "${WORK}"

# OpenRCT2's main() becomes openrct2_main(), called by ps5/main.c.
UI_MAIN="${BUILD}/CMakeFiles/openrct2.dir/src/openrct2-ui/Ui.cpp.o"
"${OBJCOPY}" --redefine-sym main=openrct2_main "${UI_MAIN}" "${WORK}/Ui.cpp.o"
mapfile -t UI_OBJS < <(find "${BUILD}/CMakeFiles/openrct2.dir" -name '*.o' ! -path "${UI_MAIN}" | sort)

"${CC}" -std=gnu11 -O2 -Wall "${DEBUG_CFLAGS[@]}" -I"${PS5_SYSROOT}${PS5_HBROOT}/include" \
    -c ps5/main.c -o "${WORK}/main.o"

rm -rf "${STAGE}"
mkdir -p "${STAGE}"
cp -a "${DATA}" "${STAGE}/openrct2"
rm -rf "${STAGE}/openrct2/shaders" # OpenGL only
cp ps5/assets/config.ini ps5/assets/loading.bmp "${STAGE}/"
if [[ "${AUTOTEST}" == 1 ]]; then
    mkdir -p "${STAGE}/autotest"
    cp ps5/autotest/ps5-autotest.js "${STAGE}/autotest/"
fi

PS5_SDL_APP=1 PS5_SDL_TRACE="${DEBUG}" scripts/package-native.sh "build/dist/${TITLE_ID}" ps5/sce_sys "${STAGE}" \
    "${WORK}/main.o" "${WORK}/Ui.cpp.o" "${UI_OBJS[@]}" \
    --start-group \
    "${BUILD}/libopenrct2.a" \
    "${HB}/libSDL2.a" "${HB}/libsamplerate.a" \
    "${HB}/libpng16.a" "${HB}/libzip.a" "${HB}/libbz2.a" "${HB}/liblzma.a" \
    "${HB}/libzstd.a" "${HB}/libz.a" \
    "${HB}/libFLAC.a" "${HB}/libvorbisfile.a" "${HB}/libvorbis.a" "${HB}/libogg.a" \
    "${HB}/libicuuc.a" "${HB}/libicudata.a" "${HB}/libm.a" \
    --end-group

scripts/check-imports.sh "build/native-link/${TITLE_ID}/llvm-pie.elf"
