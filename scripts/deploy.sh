#!/usr/bin/env bash
# Send a payload to the PS5's ELF loader and stream its stdout back.
#
# Usage: PS5_HOST=192.168.1.50 scripts/deploy.sh build/sdl-smoke/sdl-smoke.elf
set -euo pipefail

PAYLOAD="${1:?usage: $0 PAYLOAD.elf}"
HOST="${PS5_HOST:?set PS5_HOST to the IP address of the PS5}"
PORT="${PS5_PORT:-9021}"

echo "Sending ${PAYLOAD} to ${HOST}:${PORT} (Ctrl-C to disconnect)"
if command -v socat >/dev/null; then
    socat -t 9999999 - "TCP:${HOST}:${PORT}" < "${PAYLOAD}"
else
    nc "${HOST}" "${PORT}" < "${PAYLOAD}"
fi
