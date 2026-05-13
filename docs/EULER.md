# Euler workflow for Maxwell demon sweeps

This project is embarrassingly parallel for data generation: every simulation run is independent. Use Slurm job arrays for CPU simulations and reserve GPU only for training the surrogate model.

## 0. Resource discovery

Run on the login node:

```bash
bash euler/check_resources.sh
bash euler/check_resources.sh --cpu-probe
```

If you expect GPU access:

```bash
bash euler/check_resources.sh --gpu-probe
# or as a batch job:
sbatch euler/probe_gpu.sbatch
```

If `my_share_info` shows a shareholder group, you can submit with `-A <share_name>` or set a default account in `~/.slurm/defaults`.

## 1. Build on a compute node

```bash
sbatch euler/build_cpu.sbatch
squeue -u $USER
```

If your environment needs modules, submit for example:

```bash
sbatch --export=ALL,EULER_MODULES="gcc cmake python" euler/build_cpu.sbatch
```

Use exact module names from `module avail` on Euler.

## 2. Generate a design table

For a small pilot:

```bash
python3 scripts/generate_design.py --out designs/design.csv --n-runs 32 --preset local_small --seed 7 --snapshot-interval 0
```

For production data collection:

```bash
python3 scripts/generate_design.py --out designs/design.csv --n-runs 2000 --preset euler_medium --seed 42 --snapshot-interval 0
```

`snapshot_interval = 0` is intentional for large sweeps. Each run still writes `observables.csv`, `summary.json`, `metadata.json`, and `final_snapshot.csv`.

## 3. Pilot array

Start with a small array and low concurrency:

```bash
sbatch --array=0-31%8 euler/run_array_cpu.sbatch
```

Check logs:

```bash
squeue -u $USER
ls logs
python3 scripts/check_sweep_status.py --runs output/euler_runs/<array_job_id> --design designs/design.csv
```

## 4. Larger array

After the pilot is clean, increase concurrency. For example:

```bash
sbatch --array=0-1999%128 euler/run_array_cpu.sbatch
```

Do not start at `%1000`. Increase gradually based on `jeff`, `myjobs`, filesystem pressure, and queue behavior.

## 5. Build dataset

```bash
sbatch --export=ALL,RUNS_DIR=output/euler_runs/<array_job_id>,DESIGN_CSV=designs/design.csv euler/collect_dataset.sbatch
```

or locally after the jobs finish:

```bash
python3 ml/build_dataset.py --summaries output/euler_runs/<array_job_id> --design designs/design.csv --out output/datasets/maxwell_dataset.parquet
```

## 6. GPU training

First create/activate a Python environment that has `torch`, `numpy`, `pandas`, `pyarrow`, and `matplotlib`. Then submit:

```bash
sbatch --export=ALL,PY_ENV=$HOME/venvs/maxwell_demon/bin/activate,DATASET=output/datasets/maxwell_dataset.parquet euler/train_gpu.sbatch
```

If you need a specific GPU model, add the GPU constraint at submission time according to Euler policy. Example:

```bash
sbatch --gpus=a100:1 --export=ALL,PY_ENV=$HOME/venvs/maxwell_demon/bin/activate euler/train_gpu.sbatch
```

## 7. Use the model

Predict outcomes for a candidate design:

```bash
python3 ml/predict.py --model output/models/surrogate.pt --input designs/design.csv --out output/predictions/design_predictions.csv
```

Propose optimized candidates:

```bash
python3 ml/optimize_layout.py --model output/models/surrogate.pt --out output/predictions/top_candidates.csv --trials 10000 --top-k 50
```

Then simulate the top candidates to validate the surrogate predictions.
