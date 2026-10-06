#!/usr/bin/env bash
# Link objects and static archives built with the payload toolchain into a
# native PS5 title folder, using ps5-native-app-boilerplate's startup code,
# linker script, runtime shim, converter and FSELF signer. Runs inside the
# toolchain container.
#
# Usage: scripts/package-native.sh TITLE_DIR SCE_SYS_DIR ASSETS_DIR|- INPUT...
#   TITLE_DIR    output folder, e.g. build/dist/PPSA01325 (its name is the title ID)
#   SCE_SYS_DIR  folder with param.json, icon0.png and optional pic0/pic1/snd0
#   ASSETS_DIR   folder copied to /app0/assets, or - for none
#   INPUT...     objects, static archives and linker flags, in link order
#
# Set PS5_SDL_APP=1 for mouse-driven SDL2 renderer apps: the controller then
# drives a cursor (platform/sdl/ps5_virtual_mouse.c) and the software renderer
# offers only 32-bit texture formats (platform/sdl/ps5_sdl_render.c).
# PS5_SDL_TRACE=1 logs display setup and the first frames
# (platform/sdl/ps5_sdl_trace.c).
set -euo pipefail

TITLE_DIR="$1"
SCE_SYS="$2"
ASSETS="$3"
shift 3

NATIVE="$(cd "$(dirname "$0")/../native" && pwd)"
SDK="${NATIVE}/.deps/native/ps5-payload-sdk"
TOOL="${NATIVE}/build/host/ps5-native-tool"
LIBDIR="${PS5_PAYLOAD_SDK}/target/lib"
HBLIB="${PS5_PAYLOAD_SDK}/target/user/homebrew/lib"
WORK="$(dirname "${TITLE_DIR}")/../native-link/$(basename "${TITLE_DIR}")"

# Same constants as the boilerplate's tools/build.sh.
MODULE_SDK=0x02000009
COMPANION_SDK=0x08050001
FSELF_MAGIC=0x1D3D154F

# The host tool, pinned SDK and runtime shim come from the boilerplate's build.
if [[ ! -x "${TOOL}" || ! -f "${NATIVE}/runtime/libc.prx" || ! -d "${SDK}" ]]; then
    make -C "${NATIVE}" deps libc
    bash "${NATIVE}/tools/build-host-tools.sh"
fi

PLATFORM="$(cd "$(dirname "$0")/../platform" && pwd)"
mkdir -p "${WORK}"
"${CXX}" -std=c++20 -O2 -fno-exceptions -fno-rtti -c \
    "${NATIVE}/tooling/native/app_crt.cpp" -o "${WORK}/app_crt.o"
PLATFORM_OBJS=()
for src in "${PLATFORM}"/*.c; do
    obj="${WORK}/$(basename "${src}" .c).o"
    "${CC}" -std=gnu11 -O2 -Wall -c "${src}" -o "${obj}"
    PLATFORM_OBJS+=("${obj}")
done

SDL_SOURCES=()
[[ "${PS5_SDL_APP:-0}" == 1 ]] && SDL_SOURCES+=("${PLATFORM}/sdl/ps5_virtual_mouse.c" "${PLATFORM}/sdl/ps5_sdl_render.c")
[[ "${PS5_SDL_TRACE:-0}" == 1 ]] && SDL_SOURCES+=("${PLATFORM}/sdl/ps5_sdl_trace.c")
if (( ${#SDL_SOURCES[@]} > 0 )); then
    for src in "${SDL_SOURCES[@]}"; do
        obj="${WORK}/$(basename "${src}" .c).o"
        "${CC}" -std=gnu11 -O2 -Wall -I"${PS5_SYSROOT}${PS5_HBROOT}/include/SDL2" \
            -D_REENTRANT -c "${src}" -o "${obj}"
        PLATFORM_OBJS+=("${obj}")
    done
fi

# The boilerplate's linker script plus the __eh_frame_* bounds that the payload
# SDK's libunwind looks up (its own linker script defines them the same way).
LDSCRIPT="${WORK}/ps5-pie.ld"
sed -e 's|KEEP(\*(\.eh_frame_hdr))|PROVIDE_HIDDEN(__eh_frame_hdr_start = .); KEEP(*(.eh_frame_hdr)) PROVIDE_HIDDEN(__eh_frame_hdr_end = .);|' \
    -e 's|{ KEEP(\*(\.eh_frame)) }|{ PROVIDE_HIDDEN(__eh_frame_start = .); KEEP(*(.eh_frame)) PROVIDE_HIDDEN(__eh_frame_end = .); }|' \
    "${NATIVE}/tooling/native/ps5-pie.ld" > "${LDSCRIPT}"
[[ $(grep -c '__eh_frame' "${LDSCRIPT}") == 2 ]] || { echo "ps5-pie.ld changed; update the eh_frame patch" >&2; exit 1; }

# Route allocations to the app heap in platform/ps5_heap.c, and exit(),
# sysctl() and console stdio to platform/ps5_native_shims.c.
WRAPS=(--wrap=exit --wrap=sysctl --wrap=vfprintf --wrap=fputs --wrap=fputc --wrap=fwrite --wrap=fflush)
for sym in malloc calloc realloc free memalign aligned_alloc posix_memalign malloc_usable_size; do
    WRAPS+=("--wrap=${sym}")
done
if [[ "${PS5_SDL_APP:-0}" == 1 ]]; then
    for sym in SDL_PollEvent SDL_GetMouseState SDL_WarpMouseInWindow SDL_ShowCursor \
               SDL_RenderPresent SDL_GameControllerGetAxis SDL_GetRendererInfo; do
        WRAPS+=("--wrap=${sym}")
    done
fi
if [[ "${PS5_SDL_TRACE:-0}" == 1 ]]; then
    for sym in SDL_CreateWindow SDL_CreateRenderer SDL_CreateTexture SDL_SetWindowSize \
               SDL_SetWindowFullscreen; do
        WRAPS+=("--wrap=${sym}")
    done
fi

# The payload toolchain's libc++ (with exceptions and RTTI) replaces the
# boilerplate's minimal app_cpp_runtime. Its objects ask for -lpthread, which
# libkernel's stubs already provide.
"${SDK}/bin/prospero-lld" -T "${LDSCRIPT}" --eh-frame-hdr \
    --no-dependent-libraries --error-limit=0 \
    --version-script "${NATIVE}/tooling/native/app-symbols.map" \
    "${WRAPS[@]}" -e _start -o "${WORK}/llvm-pie.elf" \
    "${WORK}/app_crt.o" "${PLATFORM_OBJS[@]}" "$@" \
    --start-group "${LIBDIR}/libc++.a" "${LIBDIR}/libc++abi.a" "${LIBDIR}/libunwind.a" --end-group \
    --as-needed "${SDK}"/target/lib/*.so

"${TOOL}" link --in "${WORK}/llvm-pie.elf" --out "${WORK}/eboot.elf" \
    --stub-dir "${SDK}/target/lib" --module-sdk "${MODULE_SDK}" \
    --companion-sdk "${COMPANION_SDK}" --file-name eboot.elf

rm -rf "${TITLE_DIR}"
mkdir -p "${TITLE_DIR}/sce_sys" "${TITLE_DIR}/sce_module"
"${TOOL}" self --sign --in "${WORK}/eboot.elf" --out "${TITLE_DIR}/eboot.bin" \
    --magic "${FSELF_MAGIC}"
cp "${NATIVE}/runtime/libc.prx" "${TITLE_DIR}/sce_module/libc.prx"
for f in param.json icon0.png pic0.dds pic1.dds snd0.at9; do
    [[ -f "${SCE_SYS}/${f}" ]] && cp "${SCE_SYS}/${f}" "${TITLE_DIR}/sce_sys/${f}"
done
[[ "${ASSETS}" == - ]] || cp -a "${ASSETS}" "${TITLE_DIR}/assets"

echo "Packaged $(basename "${TITLE_DIR}") -> ${TITLE_DIR} ($(du -sh "${TITLE_DIR}" | cut -f1))"
