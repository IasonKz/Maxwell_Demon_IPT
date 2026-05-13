#!/usr/bin/env bash
set -euo pipefail

# Larger local run. snapshots.csv is disabled in the default config.
CONFIG="${1:-simulation/configs/local_big_run.cfg}"
RUN_DIR="output/local_big_run"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

./build/maxwell_demon_sim "${CONFIG}"
python3 scripts/plots.py "${RUN_DIR}" --save-prefix "${RUN_DIR}/diagnostics"
python3 scripts/live_temperature_plot.py "${RUN_DIR}" --once --save "${RUN_DIR}/temperature_final.png"

echo "Big local run complete. No GIF is created by default because snapshot_interval=0."
echo "Outputs: ${RUN_DIR}/summary.json and ${RUN_DIR}/observables.csv"
