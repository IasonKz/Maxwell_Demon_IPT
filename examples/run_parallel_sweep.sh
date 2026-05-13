#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build
cmake --build build -j
python3 scripts/run_sweep.py \
  --binary build/maxwell_demon_sim \
  --base-config config/default.cfg \
  --thresholds 0.8 1.0 1.2 \
  --seeds 1 2 3 \
  --workers 4
