#!/usr/bin/env bash
#SBATCH --job-name=maxwell_ml
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem-per-cpu=4G
#SBATCH --gpus=1
#SBATCH --time=04:00:00
#SBATCH --tmp=30G
#SBATCH --output=logs/train_%j.out
#SBATCH --error=logs/train_%j.err

set -euo pipefail
cd "${SLURM_SUBMIT_DIR}"
mkdir -p logs output/models output/datasets

if [[ -n "${EULER_MODULES_GPU:-${EULER_MODULES:-}}" ]]; then
  module purge || true
  module load ${EULER_MODULES_GPU:-${EULER_MODULES}}
fi

# Optional: activate a venv/conda env before running, e.g.
#   sbatch --export=ALL,PY_ENV=/cluster/home/$USER/venvs/maxwell/bin/activate euler/train_gpu.sbatch
if [[ -n "${PY_ENV:-}" ]]; then
  # shellcheck source=/dev/null
  source "${PY_ENV}"
fi

python3 - <<'PY'
import torch
print('torch', torch.__version__)
print('cuda_available', torch.cuda.is_available())
if torch.cuda.is_available():
    print('gpu', torch.cuda.get_device_name(0))
PY

DATA="${DATASET:-output/datasets/maxwell_dataset.parquet}"
OUT="${MODEL_OUT:-output/models/surrogate.pt}"
if [[ ! -f "${DATA}" ]]; then
  echo "Dataset not found: ${DATA}"
  exit 2
fi

srun python3 ml/train_surrogate.py \
  --data "${DATA}" \
  --out "${OUT}" \
  --epochs "${EPOCHS:-300}" \
  --batch-size "${BATCH_SIZE:-512}" \
  --hidden "${HIDDEN:-128}" \
  --depth "${DEPTH:-3}"

echo "Model ready: ${OUT}"
