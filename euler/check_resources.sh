#!/usr/bin/env bash
set -euo pipefail

echo "== Identity =="
date
hostname
whoami
pwd

echo
if command -v my_share_info >/dev/null 2>&1; then
  echo "== Euler shareholder groups =="
  my_share_info || true
else
  echo "my_share_info not found"
fi

echo
if command -v lquota >/dev/null 2>&1; then
  echo "== Storage quota =="
  lquota || true
else
  echo "lquota not found"
fi

echo
if command -v sinfo >/dev/null 2>&1; then
  echo "== Slurm overview: partitions/nodes/resources =="
  sinfo -o "%P %D %c %m %G %t" || true
  echo
  echo "== GPU node summary, if visible to your account =="
  sinfo -o "%10D %80G %80N" | head -40 || true
else
  echo "sinfo not found"
fi

echo
if command -v squeue >/dev/null 2>&1; then
  echo "== Your current jobs =="
  squeue -u "${USER}" || true
fi

echo
if command -v sacctmgr >/dev/null 2>&1; then
  echo "== Slurm associations for user, if permitted =="
  sacctmgr -n show assoc user="${USER}" format=Account,Partition,QOS,GrpTRES,MaxTRES,MaxJobs,MaxSubmitJobs 2>/dev/null | head -80 || true
fi

echo
if command -v module >/dev/null 2>&1; then
  echo "== Module hints =="
  module avail cmake 2>&1 | head -40 || true
  module avail gcc 2>&1 | head -40 || true
  module avail python 2>&1 | head -40 || true
fi

echo
if [[ "${1:-}" == "--cpu-probe" ]]; then
  echo "== Tiny CPU allocation probe =="
  srun --time=00:05:00 --ntasks=1 --cpus-per-task=1 --mem-per-cpu=1G bash -lc 'hostname; nproc; free -h | head -2'
fi

if [[ "${1:-}" == "--gpu-probe" ]]; then
  echo "== Tiny GPU allocation probe =="
  srun --time=00:05:00 --gpus=1 --mem-per-cpu=2G nvidia-smi -L
fi
