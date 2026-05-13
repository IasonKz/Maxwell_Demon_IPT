#!/usr/bin/env python3
from __future__ import annotations

import argparse
import time
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def read_observables(path: Path) -> pd.DataFrame:
    if not path.exists() or path.stat().st_size == 0:
        return pd.DataFrame()
    try:
        return pd.read_csv(path)
    except Exception:
        # File may be half-written while the simulator is running. Try again later.
        return pd.DataFrame()


def main() -> None:
    parser = argparse.ArgumentParser(description="Live temperature monitor for a running Maxwell demon output directory.")
    parser.add_argument("run_dir", type=Path, help="Directory containing observables.csv")
    parser.add_argument("--poll", type=float, default=1.0, help="Seconds between refreshes")
    parser.add_argument("--once", action="store_true", help="Draw once and exit; useful for scripts")
    parser.add_argument("--save", type=Path, default=None, help="Optional PNG output path")
    args = parser.parse_args()

    obs_path = args.run_dir / "observables.csv"
    fig, ax = plt.subplots(figsize=(9, 5))

    while True:
        df = read_observables(obs_path)
        ax.clear()
        ax.set_title("Live chamber temperature, kB=1")
        ax.set_xlabel("time")
        ax.set_ylabel("2D kinetic temperature")
        if not df.empty and {"time", "temperature_left", "temperature_right"}.issubset(df.columns):
            ax.plot(df["time"], df["temperature_left"], label="T left")
            ax.plot(df["time"], df["temperature_right"], label="T right")
            ax.legend()
            last = df.iloc[-1]
            ax.text(
                0.02, 0.98,
                f"last: t={last['time']:.3f}  T_L={last['temperature_left']:.3f}  T_R={last['temperature_right']:.3f}",
                transform=ax.transAxes,
                va="top",
            )
        else:
            ax.text(0.5, 0.5, f"Waiting for {obs_path}", ha="center", va="center", transform=ax.transAxes)
        fig.tight_layout()
        if args.save is not None:
            args.save.parent.mkdir(parents=True, exist_ok=True)
            fig.savefig(args.save, dpi=160)
        if args.once:
            break
        plt.pause(max(args.poll, 0.1))


if __name__ == "__main__":
    main()
