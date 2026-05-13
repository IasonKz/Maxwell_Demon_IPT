#!/usr/bin/env python3
from __future__ import annotations

import argparse
import math
from pathlib import Path
from typing import Any

import pandas as pd


CONFIG_KEYS = [
    "num_particles", "lx", "ly", "dt", "steps", "snapshot_interval", "observable_interval",
    "live_log_interval", "collision_iterations", "seed", "velocity_std", "initial_left_fraction", "middle_wall_x",
    "opening_centers", "opening_height", "demon_threshold", "demon_mode", "restitution",
    "detect_thermal_equilibrium", "stop_at_thermal_equilibrium", "stop_at_steady_state",
    "equilibrium_window", "equilibrium_required_windows", "equilibrium_temp_tol",
    "equilibrium_density_tol", "equilibrium_flux_tol", "equilibrium_slope_tol",
    "separated_steady_state_min_score",
]


def _is_missing(value: Any) -> bool:
    try:
        return bool(pd.isna(value))
    except Exception:
        return value is None


def _format_value(value: Any) -> str:
    if _is_missing(value):
        return ""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float):
        if math.isfinite(value) and abs(value - round(value)) < 1e-12:
            return str(int(round(value)))
        return f"{value:.10g}"
    return str(value)


def _boolish(value: Any) -> str:
    if isinstance(value, str):
        v = value.strip().lower()
        if v in {"1", "true", "yes", "on"}:
            return "true"
        if v in {"0", "false", "no", "off"}:
            return "false"
    if bool(value):
        return "true"
    return "false"


def write_config(row: pd.Series, out: Path, output_dir: str | None) -> None:
    lines: list[str] = []
    run_id = str(row.get("run_id", out.stem))
    scenario = str(row.get("scenario", "manual"))
    lines.append(f"# Generated from design row: {run_id}")
    lines.append(f"# scenario = {scenario}")

    for key in CONFIG_KEYS:
        if key not in row or _is_missing(row[key]):
            continue
        if key in {"detect_thermal_equilibrium", "stop_at_thermal_equilibrium", "stop_at_steady_state"}:
            value = _boolish(row[key])
        else:
            value = _format_value(row[key])
        if value != "":
            lines.append(f"{key} = {value}")

    if output_dir is not None:
        lines.append(f"output_dir = {output_dir}")
    elif "output_dir" in row and not _is_missing(row["output_dir"]):
        lines.append(f"output_dir = {_format_value(row['output_dir'])}")
    else:
        lines.append(f"output_dir = output/runs/{run_id}")
    lines.append("overwrite_output = true")

    species_count = int(row.get("species_count", 1))
    if species_count < 1:
        species_count = 1
    lines.append(f"species_count = {species_count}")
    for sid in range(species_count):
        prefix = f"species{sid}_"
        required_defaults = {
            "count": int(row.get("num_particles", 1)) if sid == 0 else 1,
            "radius": row.get("radius", row.get("species0_radius", 0.12)),
            "mass": row.get("mass", row.get("species0_mass", 1.0)),
            "friction_gamma": row.get("friction_gamma", 0.0),
            "drive_strength": row.get("drive_strength", 0.0),
        }
        for attr, default in required_defaults.items():
            key = prefix + attr
            value = row[key] if key in row and not _is_missing(row[key]) else default
            if attr == "count":
                value = int(round(float(value)))
                if value <= 0:
                    raise ValueError(f"{key} must be positive for species_count={species_count}")
            lines.append(f"{key} = {_format_value(value)}")

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Create a simulator .cfg file from one row of a design CSV.")
    parser.add_argument("--design", type=Path, required=True, help="Design CSV")
    parser.add_argument("--index", type=int, required=True, help="Array/design index")
    parser.add_argument("--index-base", type=int, choices=[0, 1], default=0, help="Whether --index is 0-based or 1-based")
    parser.add_argument("--out", type=Path, required=True, help="Output config path")
    parser.add_argument("--output-dir", type=str, default=None, help="Override output_dir in generated config")
    parser.add_argument("--output-root", type=Path, default=None, help="Use output-root/run_id as output_dir")
    args = parser.parse_args()

    df = pd.read_csv(args.design)
    idx = args.index - args.index_base
    if idx < 0 or idx >= len(df):
        raise IndexError(f"Design index {args.index} with base {args.index_base} maps to {idx}, but design has {len(df)} rows")
    row = df.iloc[idx]

    output_dir = args.output_dir
    if output_dir is None and args.output_root is not None:
        run_id = str(row.get("run_id", f"run_{idx:06d}"))
        output_dir = str(args.output_root / run_id)

    write_config(row, args.out, output_dir)
    print(f"Wrote config for design row {idx} to {args.out}")


if __name__ == "__main__":
    main()
