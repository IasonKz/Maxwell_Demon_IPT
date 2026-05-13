#!/usr/bin/env bash
#SBATCH --job-name=maxwell_gpu_probe
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=2
#SBATCH --mem-per-cpu=2G
#SBATCH --gpus=1
#SBATCH --time=00:10:00
#SBATCH --output=logs/gpu_probe_%j.out
#SBATCH --error=logs/gpu_probe_%j.err

set -euo pipefail
mkdir -p logs
hostname
nvidia-smi -L
nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv
