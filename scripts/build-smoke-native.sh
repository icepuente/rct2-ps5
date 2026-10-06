#!/usr/bin/env bash
# Build the SDL2 smoke test as a native title (PPSA99998). Runs inside the
# toolchain container (see scripts/build.sh).
set -euo pipefail

OBJ=build/sdl-smoke-native
HB="${PS5_SYSROOT}${PS5_HBROOT}"
mkdir -p "${OBJ}"

${CC} -O2 -Wall -I"${HB}/include" -I"${HB}/include/SDL2" -D_REENTRANT \
    -c tools/sdl-smoke/main.c -o "${OBJ}/main.o"

scripts/package-native.sh build/dist/PPSA99998 tools/sdl-smoke/sce_sys - \
    "${OBJ}/main.o" "${HB}/lib/libSDL2.a" "${HB}/lib/libsamplerate.a"
scripts/check-imports.sh build/native-link/PPSA99998/llvm-pie.elf
