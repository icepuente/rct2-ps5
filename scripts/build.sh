#!/usr/bin/env bash
# Build a target inside the PS5 toolchain container.
#
# Usage: scripts/build.sh [smoke|smoke-native|openrct2]
# Set DOCKER_CONTEXT to choose the Docker daemon (e.g. colima-ps5).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${IMAGE:-ps5-openrct2-env:latest}"
TARGET="${1:-openrct2}"

case "${TARGET}" in
    smoke)    CMD='$CMAKE -S tools/sdl-smoke -B build/sdl-smoke -DCMAKE_BUILD_TYPE=Release && make -C build/sdl-smoke -j$(nproc)' ;;
    smoke-native) CMD='scripts/build-smoke-native.sh' ;;
    openrct2) CMD='scripts/build-openrct2.sh && scripts/package-openrct2.sh' ;;
    *) echo "unknown target: ${TARGET}" >&2; exit 1 ;;
esac

if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
    docker build -f "${ROOT}/docker/Dockerfile" -t "${IMAGE}" "${ROOT}"
fi

# On Linux hosts (e.g. CI) the checkout belongs to the host user, so build as
# that user; Docker Desktop and Colima on macOS map ownership themselves.
USER_ARGS=()
if [[ "$(uname -s)" == Linux ]]; then
    USER_ARGS=(--user "$(id -u):$(id -g)" -e HOME=/tmp)
fi

docker run --rm -v "${ROOT}":/work -w /work ${USER_ARGS[@]+"${USER_ARGS[@]}"} \
    -e OPENRCT2_PS5_DEBUG -e OPENRCT2_PS5_AUTOTEST "${IMAGE}" \
    bash -c "source \$PS5_PAYLOAD_SDK/toolchain/prospero.sh && ${CMD}"
