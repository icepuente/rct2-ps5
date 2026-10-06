#!/usr/bin/env bash
# List the imports of a linked native title that only system modules outside
# the loader's preloaded set provide. Such imports link fine but stay null, so
# calling one jumps to address 0. Give each a definition in
# platform/ps5_native_shims.c. Runs inside the toolchain container.
#
# Usage: scripts/check-imports.sh build/native-link/<TITLE_ID>/llvm-pie.elf
set -euo pipefail

ELF="$1"
STUBS="$(cd "$(dirname "$0")/../native" && pwd)/.deps/native/ps5-payload-sdk/target/lib"

# Modules the loader mapped into a native title (from the
# kernel log of a launch), whether or not the title imports from them.
PRELOADED="libkernel libkernel_sys libkernel_web libSceLibcInternal libSceSysmodule
libSceAmpr libSceNet libSceIpmi libSceMbus libSceRegMgr libSceRtc libSceRazorCpu
libSceAvSetting libSceVideoOut libSceAgcDriver libSceAgc libSceAudioOut
libSceAudioIn libSceAjmi libSceAjm libScePad libSceNetCtl libSceSsl
libSceHttpCache libSceHttp libSceHttp2 libSceNpCommon libSceNpManager
libSceNpGameIntent libSceNpWebApi2 libSceSaveData libSceSystemService
libSceUserService libSceCommonDialog libSceSysUtil"

declare -A preloaded
for m in ${PRELOADED}; do preloaded[$m]=1; done

declare -A providers
for so in "${STUBS}"/*.so; do
    module="$(basename "${so}" .so)"
    [[ "${module}" == *_stub_weak ]] && continue
    while read -r sym; do
        providers[$sym]+="${module} "
    done < <(llvm-nm-18 -D --defined-only "${so}" 2>/dev/null | awk '{print $NF}')
done

status=0
while read -r sym; do
    mods="${providers[$sym]:-}"
    [[ -z "${mods}" ]] && continue
    ok=0
    for m in ${mods}; do [[ -n "${preloaded[$m]:-}" ]] && ok=1; done
    if (( ! ok )); then
        echo "null at run time: ${sym}  (only in: ${mods% })"
        status=1
    fi
done < <(llvm-readelf-18 --dyn-syms "${ELF}" | awk '$7 == "UND" && $8 != "" {sub(/@.*/, "", $8); print $8}' | sort -u)

(( status )) || echo "All imports resolve to preloaded modules."
exit ${status}
