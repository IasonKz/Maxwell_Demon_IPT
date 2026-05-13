#!/usr/bin/env bash
set -euo pipefail

JOB_ID="${1:?Usage: bash euler/collect_for_job.sh <array_job_id> [design_csv]}"
DESIGN_CSV="${2:-designs/design.csv}"
RUNS_DIR="output/euler_runs/${JOB_ID}"

sbatch --export=ALL,RUNS_DIR="${RUNS_DIR}",DESIGN_CSV="${DESIGN_CSV}" euler/collect_dataset.sbatch
