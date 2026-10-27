#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
exec renode --console scripts/litex_vexriscv_accel.resc -e s --hide-log
