#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter
import pandas as pd


def draw_wall(ax, wall_x: float, openings: list[float], height: float, ly: float) -> None:
    segments = []
    start = 0.0
    for center in sorted(openings):
        low = center - 0.5 * height
        high = center + 0.5 * height
        segments.append((start, low))
        start = high
    segments.append((start, ly))
    for y0, y1 in segments:
        if y1 > y0:
            ax.plot([wall_x, wall_x], [y0, y1], linewidth=2)


def main() -> None:
    parser = argparse.ArgumentParser(description="Animate a Maxwell demon run from CSV output.")
    parser.add_argument("run_dir", type=Path, help="Directory containing snapshots.csv and metadata.json")
    parser.add_argument("--save-gif", type=Path, default=None, help="Optional GIF output path")
    parser.add_argument("--fps", type=int, default=20)
    args = parser.parse_args()

    run_dir = args.run_dir
    snapshots = pd.read_csv(run_dir / "snapshots.csv")
    metadata = json.loads((run_dir / "metadata.json").read_text())

    frames = sorted(snapshots["step"].unique())
    grouped = {step: frame for step, frame in snapshots.groupby("step")}

    fig, ax = plt.subplots(figsize=(10, 5))
    ax.set_xlim(0, metadata["lx"])
    ax.set_ylim(0, metadata["ly"])
    ax.set_aspect("equal", adjustable="box")
    ax.set_title("Maxwell Demon Simulation")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    draw_wall(ax, metadata["middle_wall_x"], metadata["opening_centers"], metadata["opening_height"], metadata["ly"])

    first = grouped[frames[0]]
    scatter = ax.scatter(first["x"], first["y"], c=first["speed"], cmap="coolwarm", s=18)
    cbar = fig.colorbar(scatter, ax=ax)
    cbar.set_label("speed")
    text = ax.text(0.02, 0.98, "", transform=ax.transAxes, va="top")

    def update(step: int):
        frame = grouped[step]
        scatter.set_offsets(frame[["x", "y"]].to_numpy())
        scatter.set_array(frame["speed"].to_numpy())
        text.set_text(f"step={step}  t={frame['time'].iloc[0]:.3f}")
        return scatter, text

    animation = FuncAnimation(fig, update, frames=frames, interval=1000 / args.fps, blit=False)

    if args.save_gif is not None:
        animation.save(args.save_gif, writer=PillowWriter(fps=args.fps))
    else:
        plt.show()


if __name__ == "__main__":
    main()
