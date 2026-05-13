#!/usr/bin/env bash
#SBATCH --job-name=maxwell_collect
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=2
#SBATCH --mem-per-cpu=4G
#SBATCH --time=00:30:00
#SBATCH --tmp=5G
#SBATCH --output=logs/collect_%j.out
#SBATCH --error=logs/collect_%j.err

set -euo pipefail
cd "${SLURM_SUBMIT_DIR}"
mkdir -p logs output/datasets

if [[ -n "${EULER_MODULES:-}" ]]; then
  module purge || true
  module load ${EULER_MODULES}
fi

RUNS_DIR="${RUNS_DIR:-output/euler_runs/${AFTER_ARRAY_JOB_ID:-latest}}"
if [[ "${RUNS_DIR}" == *"/latest" ]]; then
  RUNS_DIR=$(ls -dt output/euler_runs/* 2>/dev/null | head -1 || true)
fi
if [[ -z "${RUNS_DIR}" || ! -d "${RUNS_DIR}" ]]; then
  echo "Could not find RUNS_DIR. Submit with: sbatch --export=ALL,RUNS_DIR=output/euler_runs/<array_job_id> euler/collect_dataset.sbatch"
  exit 2
fi

DESIGN_ARG=()
if [[ -f "${DESIGN_CSV:-designs/design.csv}" ]]; then
  DESIGN_ARG=(--design "${DESIGN_CSV:-designs/design.csv}")
fi

OUT="${DATASET_OUT:-output/datasets/maxwell_dataset.parquet}"
python3 ml/build_dataset.py --summaries "${RUNS_DIR}" --out "${OUT}" "${DESIGN_ARG[@]}"
python3 scripts/check_sweep_status.py --runs "${RUNS_DIR}" --design "${DESIGN_CSV:-designs/design.csv}" --out "${OUT%.parquet}_status.csv" || true

echo "Dataset ready: ${OUT}"
