#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

# Allow importing scripts/generate_design.py when executed from repo root or ml/.
REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "scripts"))

from generate_design import generate  # type: ignore  # noqa: E402
from predict import load_checkpoint, predict_dataframe  # type: ignore  # noqa: E402


def score_predictions(df: pd.DataFrame, energy_weight: float, time_weight: float, equilibrium_weight: float) -> pd.Series:
    separation = df.get("pred_separation_score_final", pd.Series(np.zeros(len(df))))
    energy = df.get("pred_energy_injected_total", pd.Series(np.zeros(len(df)))) + df.get("pred_energy_dissipated_total", pd.Series(np.zeros(len(df))))
    equilibrium_quality = df.get("pred_equilibrium_quality_final", pd.Series(np.zeros(len(df))))
    steady_prob = df.get("pred_separated_steady_state_reached", pd.Series(np.zeros(len(df))))
    final_time = df.get("final_time", df.get("steps", pd.Series(np.zeros(len(df)))) * df.get("dt", 1.0))

    energy_norm = energy / max(float(energy.quantile(0.9)), 1e-9)
    time_norm = final_time / max(float(final_time.quantile(0.9)), 1e-9)
    return separation + 0.25 * steady_prob + equilibrium_weight * equilibrium_quality - energy_weight * energy_norm - time_weight * time_norm


def main() -> None:
    parser = argparse.ArgumentParser(description="Use the trained surrogate to propose high-performing experimental layouts by random search.")
    parser.add_argument("--model", type=Path, required=True, help="Trained surrogate .pt")
    parser.add_argument("--out", type=Path, required=True, help="Output CSV with top candidates")
    parser.add_argument("--trials", type=int, default=5000, help="Random candidate count")
    parser.add_argument("--top-k", type=int, default=50)
    parser.add_argument("--seed", type=int, default=2026)
    parser.add_argument("--preset", choices=["local_small", "euler_medium", "euler_large"], default="euler_medium")
    parser.add_argument("--energy-weight", type=float, default=0.15)
    parser.add_argument("--time-weight", type=float, default=0.05)
    parser.add_argument("--equilibrium-weight", type=float, default=0.05, help="Positive weight; keep small for demon separation, larger for equilibrium control")
    parser.add_argument("--max-particles", type=int, default=None)
    parser.add_argument("--species-count", type=int, choices=[1, 2], default=None)
    parser.add_argument("--device", type=str, default=None)
    args = parser.parse_args()

    rows = generate(args.trials, args.seed, args.preset, snapshot_interval=0)
    candidates = pd.DataFrame(rows)
    if args.max_particles is not None:
        candidates = candidates[candidates["num_particles"] <= args.max_particles].reset_index(drop=True)
    if args.species_count is not None:
        candidates = candidates[candidates["species_count"] == args.species_count].reset_index(drop=True)
    if candidates.empty:
        raise SystemExit("No candidates remained after constraints")

    model, preprocessing, device = load_checkpoint(args.model, args.device)
    predicted = predict_dataframe(candidates, model, preprocessing, device)
    predicted["objective_score"] = score_predictions(predicted, args.energy_weight, args.time_weight, args.equilibrium_weight)
    top = predicted.sort_values("objective_score", ascending=False).head(args.top_k)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    top.to_csv(args.out, index=False)
    print(f"Wrote top {len(top)} candidates to {args.out}")
    cols = [c for c in ["run_id", "scenario", "num_particles", "species_count", "opening_height", "demon_threshold", "pred_separation_score_final", "pred_energy_injected_total", "objective_score"] if c in top]
    print(top[cols].head(10).to_string(index=False))


if __name__ == "__main__":
    main()
