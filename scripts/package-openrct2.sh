#!/usr/bin/env bash
# Package the cross-compiled OpenRCT2 (scripts/build-openrct2.sh) as the
# native title PPSA99702. Runs inside the toolchain container.
#
# OPENRCT2_PS5_DEBUG=1 adds OpenRCT2's verbose log and the SDL display trace
# to the kernel log. OPENRCT2_PS5_AUTOTEST=tour (or 1) installs the autotest
# plugin (ps5/autotest/ps5-autotest.js) in scenario tour mode, which runs on
# the title screen; OPENRCT2_PS5_AUTOTEST=play opens the next scenario on each
# launch and plays it.
set -euo pipefail

DEBUG="${OPENRCT2_PS5_DEBUG:-0}"
AUTOTEST="${OPENRCT2_PS5_AUTOTEST:-0}"
DEBUG_CFLAGS=()
[[ "${DEBUG}" == 1 ]] && DEBUG_CFLAGS+=(-DOPENRCT2_PS5_DEBUG)
case "${AUTOTEST}" in
    0) ;;
    1 | tour) DEBUG_CFLAGS+=(-DOPENRCT2_PS5_AUTOTEST) ;;
    play) DEBUG_CFLAGS+=(-DOPENRCT2_PS5_AUTOTEST -DOPENRCT2_PS5_AUTOTEST_PLAY) ;;
    *) echo "OPENRCT2_PS5_AUTOTEST must be tour or play" >&2; exit 1 ;;
esac

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

# ps5/ui_snap.cpp reads OpenRCT2's window structures, so it is compiled with
# the same defines, include paths and layout-affecting flags as OpenRCT2.
ninja_var() {
    awk -v var="$1" '/^build CMakeFiles\/openrct2.dir\/src\/openrct2-ui\/Ui.cpp.o:/{f=1}
        f && $1 == var {sub(/^ *[A-Z]+ = /, ""); print; exit}' "${BUILD}/build.ninja"
}
eval "UI_DEFINES=($(ninja_var DEFINES))"
eval "UI_INCLUDES=($(ninja_var INCLUDES))"
"${CXX}" -std=gnu++20 -fno-char8_t -O2 -DNDEBUG -DDEBUG=0 \
    -DJSON_HAS_FILESYSTEM=0 -DJSON_HAS_EXPERIMENTAL_FILESYSTEM=0 \
    "${UI_DEFINES[@]}" "${UI_INCLUDES[@]}" -Wall -c ps5/ui_snap.cpp -o "${WORK}/ui_snap.o"

rm -rf "${STAGE}"
mkdir -p "${STAGE}"
cp -a "${DATA}" "${STAGE}/openrct2"
rm -rf "${STAGE}/openrct2/shaders" # OpenGL only
cp ps5/assets/config.ini ps5/assets/loading.bmp "${STAGE}/"
if [[ "${AUTOTEST}" != 0 ]]; then
    mkdir -p "${STAGE}/autotest"
    cp ps5/autotest/ps5-autotest.js "${STAGE}/autotest/"
fi

PS5_SDL_APP=1 PS5_SDL_TRACE="${DEBUG}" scripts/package-native.sh "build/dist/${TITLE_ID}" ps5/sce_sys "${STAGE}" \
    "${WORK}/main.o" "${WORK}/ui_snap.o" "${WORK}/Ui.cpp.o" "${UI_OBJS[@]}" \
    --start-group \
    "${BUILD}/libopenrct2.a" \
    "${HB}/libSDL2.a" "${HB}/libsamplerate.a" \
    "${HB}/libpng16.a" "${HB}/libzip.a" "${HB}/libbz2.a" "${HB}/liblzma.a" \
    "${HB}/libzstd.a" "${HB}/libz.a" \
    "${HB}/libFLAC.a" "${HB}/libvorbisfile.a" "${HB}/libvorbis.a" "${HB}/libogg.a" \
    "${HB}/libicuuc.a" "${HB}/libicudata.a" "${HB}/libm.a" \
    --end-group

scripts/check-imports.sh "build/native-link/${TITLE_ID}/llvm-pie.elf"
