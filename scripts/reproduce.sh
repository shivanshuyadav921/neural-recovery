#!/usr/bin/env bash
# NeuroParse Phase 18 - Shell Reproducibility Entrypoint
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

echo "======================================================================"
echo "  NeuroParse Phase 18 — Bash Reproducibility Orchestrator             "
echo "======================================================================"

python3 scripts/reproduce.py "$@"
