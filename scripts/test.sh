#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$ROOT/build"
ctest --test-dir "$ROOT/build" --output-on-failure
python -m pytest "$ROOT/tests/python" -q
