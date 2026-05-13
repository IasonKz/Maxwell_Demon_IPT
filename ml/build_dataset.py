#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

import pandas as pd


def flatten_summary(summary_path: Path) -> dict:
    data = json.loads(summary_path.read_text())
    data["run_dir"] = str(summary_path.parent)
    data["run_id"] = summary_path.parent.name

    metadata_path = summary_path.parent / "metadata.json"
    if metadata_path.exists():
        metadata = json.loads(metadata_path.read_text())
        for key in [
            "lx", "ly", "max_radius", "dt", "steps", "snapshot_interval", "observable_interval",
            "collision_iterations", "seed", "velocity_std", "initial_left_fraction", "middle_wall_x",
            "opening_height", "demon_threshold", "demon_mode", "restitution",
        ]:
            if key in metadata:
                data[key] = metadata[key]
        species = metadata.get("species", [])
        for sp in species:
            sid = int(sp.get("id", 0))
            for key, value in sp.items():
                if key != "id":
                    data[f"species{sid}_{key}"] = value
    return data


def main() -> None:
    parser = argparse.ArgumentParser(description="Collect per-run summary.json files into a ML dataset.")
    parser.add_argument("--summaries", type=Path, required=True, help="Directory containing run folders or summary_*.json files")
    parser.add_argument("--out", type=Path, required=True, help="Output .csv or .parquet")
    parser.add_argument("--design", type=Path, default=None, help="Optional design CSV; merged by run_id to restore planned input features")
    args = parser.parse_args()

    paths = sorted(args.summaries.rglob("summary.json"))
    paths += sorted(args.summaries.glob("summary_*.json"))
    # Avoid duplicate rows when glob patterns overlap.
    paths = sorted(set(paths))
    if not paths:
        raise FileNotFoundError(f"No summary.json files found under {args.summaries}")

    rows = [flatten_summary(path) for path in paths]
    df = pd.DataFrame(rows)

    if args.design is not None:
        design = pd.read_csv(args.design)
        if "run_id" not in design.columns:
            raise ValueError("--design CSV must contain a run_id column")
        # Prefix conflicting design columns so output/leakage columns from summaries remain authoritative.
        rename = {c: f"design_{c}" for c in design.columns if c in df.columns and c != "run_id"}
        design = design.rename(columns=rename)
        df = df.merge(design, on="run_id", how="left")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    if args.out.suffix.lower() == ".parquet":
        df.to_parquet(args.out, index=False)
    else:
        df.to_csv(args.out, index=False)
    print(f"Wrote {len(df)} rows to {args.out}")


if __name__ == "__main__":
    main()
