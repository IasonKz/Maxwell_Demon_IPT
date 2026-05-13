#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

import pandas as pd


def read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text())
    except Exception as exc:
        return {"_error": str(exc)}


def main() -> None:
    parser = argparse.ArgumentParser(description="Check which simulation runs produced summary.json and basic success flags.")
    parser.add_argument("--runs", type=Path, required=True, help="Directory containing run_* subdirectories")
    parser.add_argument("--design", type=Path, default=None, help="Optional design CSV to detect missing run_ids")
    parser.add_argument("--out", type=Path, default=None, help="Optional CSV report")
    args = parser.parse_args()

    rows: list[dict] = []
    if args.design is not None and args.design.exists():
        design = pd.read_csv(args.design)
        expected = [str(x) for x in design.get("run_id", [])]
    else:
        expected = sorted(p.name for p in args.runs.glob("run_*") if p.is_dir())

    for run_id in expected:
        run_dir = args.runs / run_id
        summary_path = run_dir / "summary.json"
        metadata_path = run_dir / "metadata.json"
        row = {"run_id": run_id, "run_dir": str(run_dir), "has_summary": summary_path.exists(), "has_metadata": metadata_path.exists()}
        if summary_path.exists():
            summary = read_json(summary_path)
            for key in [
                "final_time", "thermal_equilibrium_reached", "steady_state_reached", "separated_steady_state_reached",
                "separation_score_final", "equilibrium_quality_final", "energy_injected_total", "energy_dissipated_total",
            ]:
                row[key] = summary.get(key)
            row["error"] = summary.get("_error", "")
        else:
            row["error"] = "missing summary.json"
        rows.append(row)

    df = pd.DataFrame(rows)
    complete = int(df["has_summary"].sum()) if not df.empty else 0
    print(f"Runs with summary.json: {complete}/{len(df)}")
    if not df.empty and "separated_steady_state_reached" in df:
        print(df[["run_id", "has_summary", "thermal_equilibrium_reached", "separated_steady_state_reached", "separation_score_final", "error"]].head(20).to_string(index=False))
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        df.to_csv(args.out, index=False)
        print(f"Wrote report to {args.out}")


if __name__ == "__main__":
    main()
