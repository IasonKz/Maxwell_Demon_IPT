#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def read_table(path: Path) -> pd.DataFrame:
    if path.suffix.lower() == ".parquet":
        return pd.read_parquet(path)
    return pd.read_csv(path)


def scatter(df: pd.DataFrame, x: str, y: str, out: Path, title: str) -> None:
    if x not in df or y not in df:
        return
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.scatter(pd.to_numeric(df[x], errors="coerce"), pd.to_numeric(df[y], errors="coerce"), alpha=0.65)
    ax.set_xlabel(x)
    ax.set_ylabel(y)
    ax.set_title(title)
    fig.savefig(out, dpi=160, bbox_inches="tight")
    plt.close(fig)


def grouped_bar(df: pd.DataFrame, group: str, value: str, out: Path, title: str) -> None:
    if group not in df or value not in df:
        return
    series = df.groupby(group)[value].mean(numeric_only=True).sort_values(ascending=False)
    if series.empty:
        return
    fig, ax = plt.subplots(figsize=(7, 4))
    series.plot(kind="bar", ax=ax)
    ax.set_ylabel(f"mean {value}")
    ax.set_title(title)
    fig.savefig(out, dpi=160, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Quick plots for a collected Maxwell demon ML dataset.")
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    df = read_table(args.data)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    scatter(df, "demon_threshold", "separation_score_final", args.out_dir / "threshold_vs_separation.png", "Threshold vs separation")
    scatter(df, "opening_height", "separation_score_final", args.out_dir / "opening_vs_separation.png", "Opening height vs separation")
    scatter(df, "num_particles", "separation_score_final", args.out_dir / "particles_vs_separation.png", "Particle count vs separation")
    scatter(df, "energy_injected_total", "separation_score_final", args.out_dir / "energy_vs_separation.png", "Energy input vs separation")
    grouped_bar(df, "demon_mode", "separation_score_final", args.out_dir / "demon_mode_mean_separation.png", "Mean separation by demon mode")
    grouped_bar(df, "species_count", "separation_score_final", args.out_dir / "species_count_mean_separation.png", "Mean separation by species count")
    print(f"Wrote dataset plots to {args.out_dir}")


if __name__ == "__main__":
    main()
