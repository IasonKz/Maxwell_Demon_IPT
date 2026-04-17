#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot observables from a Maxwell demon simulation.")
    parser.add_argument("run_dir", type=Path, help="Directory containing observables.csv")
    parser.add_argument("--save-prefix", type=Path, default=None, help="Optional prefix for PNG output")
    args = parser.parse_args()

    obs = pd.read_csv(args.run_dir / "observables.csv")

    fig1, ax1 = plt.subplots(figsize=(8, 4.5))
    ax1.plot(obs["time"], obs["n_left"], label="N left")
    ax1.plot(obs["time"], obs["n_right"], label="N right")
    ax1.set_xlabel("time")
    ax1.set_ylabel("particle count")
    ax1.legend()
    ax1.set_title("Population by chamber")

    fig2, ax2 = plt.subplots(figsize=(8, 4.5))
    ax2.plot(obs["time"], obs["temperature_left"], label="T left")
    ax2.plot(obs["time"], obs["temperature_right"], label="T right")
    ax2.set_xlabel("time")
    ax2.set_ylabel("temperature proxy")
    ax2.legend()
    ax2.set_title("Temperature proxy by chamber")

    fig3, ax3 = plt.subplots(figsize=(8, 4.5))
    ax3.plot(obs["time"], obs["accepted_lr"], label="accepted L→R")
    ax3.plot(obs["time"], obs["accepted_rl"], label="accepted R→L")
    ax3.plot(obs["time"], obs["rejected_lr"], label="rejected L→R")
    ax3.plot(obs["time"], obs["rejected_rl"], label="rejected R→L")
    ax3.set_xlabel("time")
    ax3.set_ylabel("cumulative gate events")
    ax3.legend(ncol=2)
    ax3.set_title("Gate statistics")

    if args.save_prefix is not None:
        fig1.savefig(f"{args.save_prefix}_counts.png", dpi=160, bbox_inches="tight")
        fig2.savefig(f"{args.save_prefix}_temperature.png", dpi=160, bbox_inches="tight")
        fig3.savefig(f"{args.save_prefix}_gates.png", dpi=160, bbox_inches="tight")
    else:
        plt.show()


if __name__ == "__main__":
    main()
