#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build
cmake --build build -j
./build/maxwell_demon_sim config/default.cfg
python3 scripts/plots.py output/default --save-prefix output/default/diagnostics
