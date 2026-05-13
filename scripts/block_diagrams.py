#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch


def add_box(ax, xy, text, width=2.6, height=0.9):
    x, y = xy
    patch = FancyBboxPatch(
        (x, y), width, height,
        boxstyle="round,pad=0.08,rounding_size=0.05",
        linewidth=1.4,
        facecolor="none",
    )
    ax.add_patch(patch)
    ax.text(x + width / 2, y + height / 2, text, ha="center", va="center", fontsize=10)
    return patch


def add_arrow(ax, start, end):
    ax.add_patch(FancyArrowPatch(start, end, arrowstyle="->", mutation_scale=14, linewidth=1.3))


def save_control_loop(out: Path) -> None:
    fig, ax = plt.subplots(figsize=(11, 6))
    ax.axis("off")
    boxes = {
        "params": add_box(ax, (0.4, 3.8), "Experimental\nparameters\nN, radii, masses"),
        "gate": add_box(ax, (3.6, 3.8), "Gate / demon\nthreshold, openings"),
        "sim": add_box(ax, (6.8, 3.8), "C++ simulation\nparticles + collisions"),
        "obs": add_box(ax, (6.8, 2.1), "Observables\nT, N, flux, energy"),
        "ml": add_box(ax, (3.6, 2.1), "ML surrogate\npredict output"),
        "opt": add_box(ax, (0.4, 2.1), "Optimizer / control\nchoose next layout"),
    }
    add_arrow(ax, (3.0, 4.25), (3.6, 4.25))
    add_arrow(ax, (6.2, 4.25), (6.8, 4.25))
    add_arrow(ax, (8.1, 3.8), (8.1, 3.0))
    add_arrow(ax, (6.8, 2.55), (6.2, 2.55))
    add_arrow(ax, (3.6, 2.55), (3.0, 2.55))
    add_arrow(ax, (1.7, 3.8), (1.7, 3.0))
    ax.text(5.5, 5.25, "Closed-loop design optimization", ha="center", fontsize=13)
    ax.set_xlim(0, 10)
    ax.set_ylim(1.4, 5.6)
    fig.savefig(out, dpi=180, bbox_inches="tight")
    plt.close(fig)


def save_euler_pipeline(out: Path) -> None:
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.axis("off")
    labels = [
        "Design CSV\nparameter sweep",
        "Slurm job array\nCPU simulations",
        "summary.json\nper run",
        "Dataset\nCSV / Parquet",
        "GPU training\nPyTorch model",
        "Predictions\n+ optimization",
    ]
    xs = [0.2, 2.2, 4.2, 6.2, 8.2, 10.2]
    for x, label in zip(xs, labels):
        add_box(ax, (x, 2.0), label, width=1.55, height=1.0)
    for x in xs[:-1]:
        add_arrow(ax, (x + 1.55, 2.5), (x + 2.0, 2.5))
    ax.text(5.8, 3.75, "Safe Euler workflow: heavy runs inside Slurm, compact outputs for ML", ha="center", fontsize=13)
    ax.set_xlim(0, 12.1)
    ax.set_ylim(1.5, 4.0)
    fig.savefig(out, dpi=180, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Create control/block diagrams for the Maxwell demon project.")
    parser.add_argument("--out-dir", type=Path, default=Path("output/figures"))
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    save_control_loop(args.out_dir / "control_loop.png")
    save_euler_pipeline(args.out_dir / "euler_pipeline.png")
    print(f"Wrote diagrams to {args.out_dir}")


if __name__ == "__main__":
    main()
