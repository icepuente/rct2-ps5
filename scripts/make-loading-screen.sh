#!/usr/bin/env bash
# Regenerate ps5/assets/loading.bmp with tools/make-loading-screen.py.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
docker run --rm -v "${ROOT}":/work -w /work python:3.12-slim bash -c '
    apt-get update -qq && apt-get install -y -qq fonts-dejavu-core >/dev/null &&
    pip install -q pillow && python3 tools/make-loading-screen.py ps5/assets/loading.bmp'
