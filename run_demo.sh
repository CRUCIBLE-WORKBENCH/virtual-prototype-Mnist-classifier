#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

args=(--console)
case "${1:-}" in
    "") ;;
    --headless) args+=(--disable-xwt --hide-analyzers) ;;
    *) echo "Usage: bash run_demo.sh [--headless]" >&2; exit 2 ;;
esac

igny workspace sync
exec igny run renode "${args[@]}" scripts/litex_vexriscv_accel.resc -e s --hide-log
