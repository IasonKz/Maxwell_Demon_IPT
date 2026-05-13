# Euler workflow

This folder is for running many C++ simulations on CPU nodes, then training the ML surrogate on an Euler GPU node.

Do not run large sweeps on login nodes. Use Slurm jobs.

## 1. Resource check

```bash
bash euler/check_resources.sh | tee euler_resource_report.txt
bash euler/check_resources.sh --cpu-probe | tee euler_cpu_probe.txt
bash euler/check_resources.sh --gpu-probe | tee euler_gpu_probe.txt
```

## 2. Build C++ on a compute node

```bash
sbatch euler/build_cpu.sbatch
squeue -u $USER
```

## 3. Small pilot sweep

```bash
bash euler/start_cpu_sweep.sh 64 euler_medium 16 0
```

This generates `designs/design.csv` and submits a Slurm job array. The last `0` means `snapshot_interval=0`, so no huge videos/snapshots are created in the data-collection phase.

## 4. Larger sweep

After the pilot is clean:

```bash
bash euler/start_cpu_sweep.sh 2000 euler_medium 128 0
```

Use conservative concurrency first (`16`, then `64`, then `128`). Even if many nodes are available, filesystem I/O and account limits can be the real bottleneck.

## 5. Collect dataset

```bash
bash euler/collect_for_job.sh <array_job_id> designs/design.csv
```

Output:

```text
output/datasets/maxwell_dataset.parquet
```

## 6. GPU training

```bash
bash euler/setup_python_venv.sh $HOME/venvs/maxwell_demon
source $HOME/venvs/maxwell_demon/bin/activate
pip install -r requirements-ml-gpu.txt
bash euler/start_gpu_training.sh output/datasets/maxwell_dataset.parquet
```

The GPU training script requests one GPU with Slurm.
