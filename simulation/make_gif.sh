#!/usr/bin/env bash
set -euo pipefail

RUN_DIR="${1:-output/local_demo_separation}"
OUT_GIF="${2:-${RUN_DIR}/separation.gif}"
MAX_FRAMES="${MAX_FRAMES:-320}"
FPS="${FPS:-20}"

python3 scripts/animate.py "${RUN_DIR}" --save-gif "${OUT_GIF}" --max-frames "${MAX_FRAMES}" --fps "${FPS}"
echo "GIF written to ${OUT_GIF}"
