#!/usr/bin/env bash
#SBATCH --job-name=maxwell_sim
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=2
#SBATCH --mem-per-cpu=2G
#SBATCH --time=02:00:00
#SBATCH --tmp=10G
#SBATCH --array=0-9%5
#SBATCH --output=logs/sim_%A_%a.out
#SBATCH --error=logs/sim_%A_%a.err

set -euo pipefail
cd "${SLURM_SUBMIT_DIR}"
mkdir -p logs output/euler_runs

if [[ -n "${EULER_MODULES:-}" ]]; then
  module purge || true
  module load ${EULER_MODULES}
fi

if [[ ! -x ./build/maxwell_demon_sim ]]; then
  echo "Missing ./build/maxwell_demon_sim. Run: sbatch euler/build_cpu.sbatch"
  exit 2
fi

DESIGN_CSV="${DESIGN_CSV:-designs/design.csv}"
if [[ ! -f "${DESIGN_CSV}" ]]; then
  echo "Design CSV not found: ${DESIGN_CSV}"
  exit 3
fi

IDX="${SLURM_ARRAY_TASK_ID}"
RUN_ID=$(python3 - <<PY
import pandas as pd
idx=int("${IDX}")
df=pd.read_csv("${DESIGN_CSV}")
print(str(df.iloc[idx].get("run_id", f"run_{idx:06d}")))
PY
)

RUN_TMP="${TMPDIR:-${SCRATCH:-/tmp}/maxwell_${SLURM_ARRAY_JOB_ID}_${IDX}}/${RUN_ID}"
RUN_OUT_TMP="${RUN_TMP}/output"
FINAL_ROOT="${RESULT_ROOT:-${SLURM_SUBMIT_DIR}/output/euler_runs/${SLURM_ARRAY_JOB_ID}}"
FINAL_DIR="${FINAL_ROOT}/${RUN_ID}"
mkdir -p "${RUN_TMP}" "${FINAL_DIR}"

export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK}"
export OMP_PROC_BIND=close
export OMP_PLACES=cores

echo "== Maxwell run =="
echo "date=$(date)"
echo "node=$(hostname)"
echo "array_job=${SLURM_ARRAY_JOB_ID:-none} task=${IDX} run_id=${RUN_ID}"
echo "design=${DESIGN_CSV}"
echo "tmp=${RUN_TMP}"
echo "final=${FINAL_DIR}"
echo "OMP_NUM_THREADS=${OMP_NUM_THREADS}"

python3 scripts/generate_config_from_design.py \
  --design "${DESIGN_CSV}" \
  --index "${IDX}" \
  --index-base 0 \
  --out "${RUN_TMP}/run.cfg" \
  --output-dir "${RUN_OUT_TMP}"

/usr/bin/time -v srun ./build/maxwell_demon_sim "${RUN_TMP}/run.cfg"

# Copy compact outputs only. snapshots.csv is intentionally skipped unless COPY_SNAPSHOTS=1.
for f in summary.json metadata.json config_used.cfg observables.csv final_snapshot.csv steady_state_snapshot.csv separated_steady_state_snapshot.csv thermal_equilibrium_snapshot.csv; do
  if [[ -f "${RUN_OUT_TMP}/${f}" ]]; then
    cp "${RUN_OUT_TMP}/${f}" "${FINAL_DIR}/"
  fi
done
if [[ "${COPY_SNAPSHOTS:-0}" == "1" && -f "${RUN_OUT_TMP}/snapshots.csv" ]]; then
  cp "${RUN_OUT_TMP}/snapshots.csv" "${FINAL_DIR}/"
fi
cp "${RUN_TMP}/run.cfg" "${FINAL_DIR}/run_generated.cfg"

du -sh "${FINAL_DIR}" || true
echo "Finished ${RUN_ID} at $(date)"
