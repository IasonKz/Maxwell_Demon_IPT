#!/usr/bin/env bash
set -euo pipefail

# Run from repository root:
#   bash simulation/run_local_demo.sh

CONFIG="${1:-simulation/configs/local_demo_separation.cfg}"
RUN_DIR="output/local_demo_separation"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

./build/maxwell_demon_sim "${CONFIG}"

python3 scripts/plots.py "${RUN_DIR}" --save-prefix "${RUN_DIR}/diagnostics"
python3 scripts/animate.py "${RUN_DIR}" \
  --save-gif "${RUN_DIR}/separation.gif" \
  --max-frames "${MAX_FRAMES:-320}" \
  --fps "${FPS:-20}"
python3 scripts/live_temperature_plot.py "${RUN_DIR}" --once --save "${RUN_DIR}/temperature_live_final.png"

echo "Local demo complete. Open:"
echo "  ${RUN_DIR}/separation.gif"
echo "  ${RUN_DIR}/diagnostics_temperature.png"
echo "  ${RUN_DIR}/summary.json"
