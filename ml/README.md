# Machine learning phase

The ML phase uses compact `summary.json` outputs from many C++ simulation runs. It does not need visualization or `snapshots.csv`.

## Build dataset

```bash
python3 ml/build_dataset.py \
  --summaries output/euler_runs/<array_job_id> \
  --design designs/design.csv \
  --out output/datasets/maxwell_dataset.parquet
```

## Train surrogate

```bash
python3 ml/train_surrogate.py \
  --data output/datasets/maxwell_dataset.parquet \
  --out output/models/surrogate.pt \
  --epochs 300 \
  --batch-size 512
```

Targets include:

```text
separation_score_final
temperature_contrast_final
population_imbalance_final
equilibrium_quality_final
energy_injected_total
energy_dissipated_total
steady_state_reached
separated_steady_state_reached
thermal_equilibrium_reached
```

## Predict candidate layouts

```bash
python3 ml/predict.py \
  --model output/models/surrogate.pt \
  --input designs/design.csv \
  --out output/predictions/design_predictions.csv
```

## Optimize layout candidates

```bash
python3 ml/optimize_layout.py \
  --model output/models/surrogate.pt \
  --out output/predictions/top_candidates.csv \
  --trials 10000 \
  --top-k 50
```

Then re-run the top candidates with the C++ simulator to validate the surrogate predictions.
