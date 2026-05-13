# Phase 2 additions

Phase 2 adds the data-collection and machine-learning pipeline:

- `scripts/generate_design.py`: creates parameter sweep tables.
- `scripts/generate_config_from_design.py`: converts one design row to a C++ simulator config.
- `scripts/check_sweep_status.py`: checks completed/missing runs.
- `ml/build_dataset.py`: collects `summary.json` and `metadata.json` into a CSV/Parquet dataset.
- `ml/train_surrogate.py`: trains a multi-output PyTorch MLP surrogate model.
- `ml/predict.py`: predicts final metrics from design parameters.
- `ml/optimize_layout.py`: proposes candidate layouts by random search through the surrogate.
- `euler/*.sbatch`: safe Slurm scripts for build, CPU arrays, dataset collection, GPU training, and probes.

Recommended order:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
python3 scripts/generate_design.py --out designs/design.csv --n-runs 8 --preset local_small --snapshot-interval 0
python3 scripts/generate_config_from_design.py --design designs/design.csv --index 0 --out output/test_run.cfg --output-dir output/test_run
./build/maxwell_demon_sim output/configs/run_000000.cfg
python3 ml/build_dataset.py --summaries output --design designs/design.csv --out output/datasets/local_dataset.csv
```

For large runs, keep snapshots disabled. Make videos only for selected runs by setting `snapshot_interval > 0` in a small design table or manual config.
