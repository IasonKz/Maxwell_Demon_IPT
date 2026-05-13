# Design CSV files

A design CSV is one simulation configuration per row. Generate examples with:

```bash
python3 scripts/generate_design.py --out designs/design.csv --n-runs 64 --preset local_small --snapshot-interval 0
```

Each row can change:

```text
num_particles
box dimensions
species counts/radii/masses/friction/drive
gate opening height/center
demon threshold and mode
friction and energy injection
integration steps and output intervals
```

Convert one row to a `.cfg` file:

```bash
python3 scripts/generate_config_from_design.py \
  --design designs/design.csv \
  --index 0 \
  --out output/configs/run_000000.cfg \
  --output-root output/runs
```
