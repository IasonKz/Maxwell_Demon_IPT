#!/usr/bin/env bash
#SBATCH --job-name=maxwell_build
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem-per-cpu=2G
#SBATCH --time=00:30:00
#SBATCH --tmp=5G
#SBATCH --output=logs/build_%j.out
#SBATCH --error=logs/build_%j.err

set -euo pipefail
mkdir -p logs output
cd "${SLURM_SUBMIT_DIR}"

echo "Started build on $(hostname) at $(date)"
echo "SLURM_JOB_ID=${SLURM_JOB_ID}"

# Optional: submit with EULER_MODULES="gcc/.. cmake/.. python/.." sbatch euler/build_cpu.sbatch
if [[ -n "${EULER_MODULES:-}" ]]; then
  module purge || true
  module load ${EULER_MODULES}
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "${SLURM_CPUS_PER_TASK}"

./build/maxwell_demon_sim config/smoke.cfg

echo "Build + smoke test finished at $(date)"
