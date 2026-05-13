#!/usr/bin/env bash
set -euo pipefail

VENV_DIR="${1:-$HOME/venvs/maxwell_demon}"
python3 -m venv "${VENV_DIR}"
# shellcheck source=/dev/null
source "${VENV_DIR}/bin/activate"
python -m pip install --upgrade pip wheel setuptools
python -m pip install -r requirements.txt
# Install torch separately only if your Euler/CUDA environment is ready.
# For CPU-only local testing you can run: python -m pip install -r requirements-ml-gpu.txt
python - <<'PY'
import sys
print('python', sys.version)
import numpy, pandas, matplotlib
print('basic packages OK')
PY

echo "Activate with: source ${VENV_DIR}/bin/activate"
echo "For GPU training submit with: sbatch --export=ALL,PY_ENV=${VENV_DIR}/bin/activate euler/train_gpu.sbatch"
