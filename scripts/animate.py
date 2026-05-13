#!/usr/bin/env python3
from __future__ import annotations

import argparse
import bisect
import json
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.animation import FFMpegWriter, FuncAnimation, PillowWriter
import pandas as pd


def draw_wall(ax, wall_x: float, openings: list[float], height: float, ly: float) -> None:
    start = 0.0
    for center in sorted(openings):
        low = center - 0.5 * height
        high = center + 0.5 * height
        if low > start:
            ax.plot([wall_x, wall_x], [start, low], linewidth=2)
        start = high
    if start < ly:
        ax.plot([wall_x, wall_x], [start, ly], linewidth=2)


def _thin_frames(frames: list[int], max_frames: int | None) -> list[int]:
    if max_frames is None or max_frames <= 0 or len(frames) <= max_frames:
        return frames
    stride = max(1, round(len(frames) / max_frames))
    thinned = frames[::stride]
    if thinned[-1] != frames[-1]:
        thinned.append(frames[-1])
    return thinned


def main() -> None:
    parser = argparse.ArgumentParser(description="Animate a Maxwell demon run from CSV output.")
    parser.add_argument("run_dir", type=Path, help="Directory containing snapshots.csv and metadata.json")
    parser.add_argument("--save-gif", type=Path, default=None, help="Optional GIF output path")
    parser.add_argument("--save-mp4", type=Path, default=None, help="Optional MP4 output path; requires ffmpeg")
    parser.add_argument("--fps", type=int, default=20)
    parser.add_argument("--max-frames", type=int, default=800, help="Limit rendered frames to avoid huge videos")
    args = parser.parse_args()

    run_dir = args.run_dir
    snapshots_path = run_dir / "snapshots.csv"
    if not snapshots_path.exists():
        raise FileNotFoundError(f"{snapshots_path} not found. Set snapshot_interval > 0 for video runs.")
    snapshots = pd.read_csv(snapshots_path)
    metadata = json.loads((run_dir / "metadata.json").read_text())

    observables_path = run_dir / "observables.csv"
    observables = pd.read_csv(observables_path) if observables_path.exists() else None
    obs_steps: list[int] = []
    obs_by_step: dict[int, pd.Series] = {}
    if observables is not None and not observables.empty:
        obs_steps = [int(x) for x in sorted(observables["step"].unique())]
        obs_by_step = {int(row.step): row for row in observables.itertuples(index=False)}

    frames = sorted(snapshots["step"].unique())
    frames = _thin_frames(frames, args.max_frames)
    grouped_all = snapshots.groupby("step")
    grouped = {step: grouped_all.get_group(step) for step in frames}

    fig, ax = plt.subplots(figsize=(10, 5))
    ax.set_xlim(0, metadata["lx"])
    ax.set_ylim(0, metadata["ly"])
    ax.set_aspect("equal", adjustable="box")
    ax.set_title("Maxwell Demon Simulation")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    draw_wall(ax, metadata["middle_wall_x"], metadata["opening_centers"], metadata["opening_height"], metadata["ly"])

    first = grouped[frames[0]]
    radius_scale = first["radius"].max() if "radius" in first.columns else 1.0
    first_sizes = (first.get("radius", pd.Series(1.0, index=first.index)) / radius_scale).pow(2) * 70
    scatter = ax.scatter(first["x"], first["y"], c=first["speed"], s=first_sizes)
    cbar = fig.colorbar(scatter, ax=ax)
    cbar.set_label("speed")
    text = ax.text(0.02, 0.98, "", transform=ax.transAxes, va="top")

    def update(step: int):
        frame = grouped[step]
        scatter.set_offsets(frame[["x", "y"]].to_numpy())
        scatter.set_array(frame["speed"].to_numpy())
        if "radius" in frame.columns:
            sizes = (frame["radius"] / max(frame["radius"].max(), 1e-12)).pow(2) * 70
            scatter.set_sizes(sizes.to_numpy())
        live = f"step={step}  t={frame['time'].iloc[0]:.3f}"
        if obs_steps:
            pos = bisect.bisect_right(obs_steps, int(step)) - 1
            if pos >= 0:
                obs = obs_by_step[obs_steps[pos]]
                live += (
                    f"\nT_left={obs.temperature_left:.3f}  T_right={obs.temperature_right:.3f}"
                    f"  contrast={obs.temperature_contrast:.3f}"
                    f"\nN_left={obs.n_left}  N_right={obs.n_right}"
                    f"  separation={obs.separation_score:.3f}"
                )
        text.set_text(live)
        return scatter, text

    animation = FuncAnimation(fig, update, frames=frames, interval=1000 / args.fps, blit=False)

    if args.save_mp4 is not None:
        args.save_mp4.parent.mkdir(parents=True, exist_ok=True)
        animation.save(args.save_mp4, writer=FFMpegWriter(fps=args.fps))
    elif args.save_gif is not None:
        args.save_gif.parent.mkdir(parents=True, exist_ok=True)
        animation.save(args.save_gif, writer=PillowWriter(fps=args.fps))
    else:
        plt.show()


if __name__ == "__main__":
    main()
