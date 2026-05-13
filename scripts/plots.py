#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def _save_or_show(figures: list[plt.Figure], save_prefix: Path | None) -> None:
    if save_prefix is None:
        plt.show()
        return
    save_prefix.parent.mkdir(parents=True, exist_ok=True)
    for name, fig in figures:
        fig.savefig(f"{save_prefix}_{name}.png", dpi=160, bbox_inches="tight")
    plt.close("all")


def _draw_wall(ax, metadata: dict) -> None:
    wall_x = float(metadata["middle_wall_x"])
    ly = float(metadata["ly"])
    openings = metadata.get("opening_centers", [])
    height = float(metadata.get("opening_height", 0.0))
    start = 0.0
    for center in sorted(openings):
        low = center - 0.5 * height
        high = center + 0.5 * height
        if low > start:
            ax.plot([wall_x, wall_x], [start, low], linewidth=2)
        start = high
    if start < ly:
        ax.plot([wall_x, wall_x], [start, ly], linewidth=2)


def _plot_final_snapshot(run_dir: Path) -> tuple[str, plt.Figure] | None:
    path = run_dir / "final_snapshot.csv"
    metadata_path = run_dir / "metadata.json"
    if not path.exists() or not metadata_path.exists():
        return None
    snap = pd.read_csv(path)
    metadata = json.loads(metadata_path.read_text())
    fig, ax = plt.subplots(figsize=(10, 5))
    size = (snap["radius"] / snap["radius"].max()).pow(2) * 80 if "radius" in snap else 20
    marker_data = snap.groupby("species_id") if "species_id" in snap else [(0, snap)]
    for species_id, frame in marker_data:
        sizes = size.loc[frame.index] if hasattr(size, "loc") else size
        ax.scatter(frame["x"], frame["y"], c=frame["speed"], s=sizes, label=f"species {species_id}", alpha=0.85)
    _draw_wall(ax, metadata)
    ax.set_xlim(0, metadata["lx"])
    ax.set_ylim(0, metadata["ly"])
    ax.set_aspect("equal", adjustable="box")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_title("Final snapshot: color = speed, size = radius")
    ax.legend(loc="upper right")
    return "final_snapshot", fig


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot observables from a Maxwell demon simulation.")
    parser.add_argument("run_dir", type=Path, help="Directory containing observables.csv")
    parser.add_argument("--save-prefix", type=Path, default=None, help="Optional prefix for PNG output")
    args = parser.parse_args()

    obs = pd.read_csv(args.run_dir / "observables.csv")
    figures: list[tuple[str, plt.Figure]] = []

    fig1, ax1 = plt.subplots(figsize=(8, 4.5))
    ax1.plot(obs["time"], obs["n_left"], label="N left")
    ax1.plot(obs["time"], obs["n_right"], label="N right")
    ax1.set_xlabel("time")
    ax1.set_ylabel("particle count")
    ax1.legend()
    ax1.set_title("Population by chamber")
    figures.append(("counts", fig1))

    fig2, ax2 = plt.subplots(figsize=(8, 4.5))
    ax2.plot(obs["time"], obs["temperature_left"], label="T left")
    ax2.plot(obs["time"], obs["temperature_right"], label="T right")
    ax2.set_xlabel("time")
    ax2.set_ylabel("2D kinetic temperature, kB=1")
    ax2.legend()
    ax2.set_title("Temperature by chamber")
    figures.append(("temperature", fig2))

    if {"temperature_contrast", "population_imbalance", "separation_score"}.issubset(obs.columns):
        fig3, ax3 = plt.subplots(figsize=(8, 4.5))
        ax3.plot(obs["time"], obs["temperature_contrast"], label="temperature contrast")
        ax3.plot(obs["time"], obs["population_imbalance"], label="population imbalance")
        ax3.plot(obs["time"], obs["separation_score"], label="separation score")
        ax3.set_xlabel("time")
        ax3.set_ylabel("dimensionless score")
        ax3.legend()
        ax3.set_title("Separation metrics")
        figures.append(("separation", fig3))

    gate_cols = ["accepted_lr", "accepted_rl", "rejected_lr", "rejected_rl"]
    if set(gate_cols).issubset(obs.columns):
        fig4, ax4 = plt.subplots(figsize=(8, 4.5))
        labels = {"accepted_lr": "accepted L→R", "accepted_rl": "accepted R→L", "rejected_lr": "rejected L→R", "rejected_rl": "rejected R→L"}
        for col in gate_cols:
            ax4.plot(obs["time"], obs[col], label=labels[col])
        ax4.set_xlabel("time")
        ax4.set_ylabel("cumulative gate events")
        ax4.legend(ncol=2)
        ax4.set_title("Gate statistics")
        figures.append(("gates", fig4))

    energy_cols = ["kinetic_energy_total", "energy_injected_total", "energy_dissipated_total"]
    if set(energy_cols).issubset(obs.columns):
        fig5, ax5 = plt.subplots(figsize=(8, 4.5))
        ax5.plot(obs["time"], obs["kinetic_energy_total"], label="kinetic energy")
        ax5.plot(obs["time"], obs["energy_injected_total"], label="energy injected")
        ax5.plot(obs["time"], obs["energy_dissipated_total"], label="energy dissipated")
        ax5.set_xlabel("time")
        ax5.set_ylabel("energy")
        ax5.legend()
        ax5.set_title("Energy ledger")
        figures.append(("energy", fig5))

    error_cols = ["temp_error", "density_error", "flux_error", "slope_error", "equilibrium_quality"]
    if set(error_cols).issubset(obs.columns):
        fig6, ax6 = plt.subplots(figsize=(8, 4.5))
        for col in error_cols:
            ax6.plot(obs["time"], obs[col], label=col)
        ax6.set_xlabel("time")
        ax6.set_ylabel("error / quality")
        ax6.legend(ncol=2)
        ax6.set_title("Equilibrium detector diagnostics")
        figures.append(("equilibrium_errors", fig6))

    final_snapshot = _plot_final_snapshot(args.run_dir)
    if final_snapshot is not None:
        figures.append(final_snapshot)

    _save_or_show(figures, args.save_prefix)


if __name__ == "__main__":
    main()
