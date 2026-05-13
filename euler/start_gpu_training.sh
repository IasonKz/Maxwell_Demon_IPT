#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   bash euler/start_gpu_training.sh output/datasets/maxwell_dataset.parquet

DATASET="${1:-output/datasets/maxwell_dataset.parquet}"
MODEL_OUT="${MODEL_OUT:-output/models/surrogate.pt}"

sbatch --export=ALL,DATASET="${DATASET}",MODEL_OUT="${MODEL_OUT}",PY_ENV="${PY_ENV:-}" euler/train_gpu.sbatch
