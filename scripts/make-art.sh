#!/usr/bin/env bash
# Regenerate the title's original artwork: the loading screen
# (tools/make-loading-screen.py -> ps5/assets/loading.bmp) and the home-screen
# icon (tools/make-icon.py -> ps5/sce_sys/icon0.png).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
docker run --rm -v "${ROOT}":/work -w /work python:3.12-slim bash -c '
    apt-get update -qq && apt-get install -y -qq fonts-dejavu-core >/dev/null &&
    pip install -q pillow &&
    python3 tools/make-loading-screen.py ps5/assets/loading.bmp &&
    python3 tools/make-icon.py ps5/sce_sys/icon0.png'
