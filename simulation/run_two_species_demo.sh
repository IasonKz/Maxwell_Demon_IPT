#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-simulation/configs/local_two_species_demo.cfg}"
RUN_DIR="output/local_two_species_demo"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

./build/maxwell_demon_sim "${CONFIG}"
python3 scripts/plots.py "${RUN_DIR}" --save-prefix "${RUN_DIR}/diagnostics"
python3 scripts/animate.py "${RUN_DIR}" --save-gif "${RUN_DIR}/two_species_separation.gif" --max-frames "${MAX_FRAMES:-300}" --fps "${FPS:-18}"

echo "Two-species demo complete: ${RUN_DIR}/two_species_separation.gif"
