#!/usr/bin/env bash
#SBATCH --job-name=maxwell_bench
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem-per-cpu=2G
#SBATCH --time=00:45:00
#SBATCH --tmp=10G
#SBATCH --output=logs/bench_%j.out
#SBATCH --error=logs/bench_%j.err

set -euo pipefail
cd "${SLURM_SUBMIT_DIR}"
mkdir -p logs output/bench

if [[ -n "${EULER_MODULES:-}" ]]; then
  module purge || true
  module load ${EULER_MODULES}
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "${SLURM_CPUS_PER_TASK}"

CFG="${TMPDIR:-/tmp}/maxwell_bench.cfg"
python3 scripts/generate_design.py --out "${TMPDIR:-/tmp}/bench_design.csv" --n-runs 1 --preset euler_medium --seed 99 --snapshot-interval 0
python3 scripts/generate_config_from_design.py --design "${TMPDIR:-/tmp}/bench_design.csv" --index 0 --out "${CFG}" --output-dir "${TMPDIR:-/tmp}/bench_output"

for t in 1 2 4 8; do
  export OMP_NUM_THREADS="${t}"
  echo "===== OMP_NUM_THREADS=${t} ====="
  /usr/bin/time -v ./build/maxwell_demon_sim "${CFG}" || true
  rm -rf "${TMPDIR:-/tmp}/bench_output"
done
