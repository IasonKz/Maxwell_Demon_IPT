#!/usr/bin/env bash
set -euo pipefail

# Usage from repo root on Euler:
#   bash euler/start_cpu_sweep.sh 256 euler_medium 64
# args: n_runs preset concurrency snapshot_interval

N_RUNS="${1:-256}"
PRESET="${2:-euler_medium}"
CONCURRENCY="${3:-64}"
SNAPSHOT_INTERVAL="${4:-0}"
DESIGN_CSV="${DESIGN_CSV:-designs/design.csv}"

mkdir -p designs logs output/euler_runs
python3 scripts/generate_design.py \
  --out "${DESIGN_CSV}" \
  --n-runs "${N_RUNS}" \
  --preset "${PRESET}" \
  --seed "${SEED:-42}" \
  --snapshot-interval "${SNAPSHOT_INTERVAL}"

LAST=$((N_RUNS - 1))
JOB_ID=$(sbatch --parsable --array="0-${LAST}%${CONCURRENCY}" --export=ALL,DESIGN_CSV="${DESIGN_CSV}" euler/run_array_cpu.sbatch)

echo "Submitted CPU sweep array job: ${JOB_ID}"
echo "Monitor: squeue -u $USER"
echo "Status:  python3 scripts/check_sweep_status.py --runs output/euler_runs/${JOB_ID} --design ${DESIGN_CSV}"
echo "Collect: bash euler/collect_for_job.sh ${JOB_ID} ${DESIGN_CSV}"
