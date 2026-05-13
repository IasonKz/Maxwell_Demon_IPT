#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import math
import random
from pathlib import Path


DEMON_MODES = ["hot_right_cold_left", "always_open", "fast_both", "slow_both"]


def _float(value: float) -> float:
    return round(float(value), 8)


def _safe_box(num_particles: int, radii: list[float], counts: list[int], rng: random.Random) -> tuple[float, float, float]:
    """Return lx, ly, packing fraction target for a box that is not too dense."""
    disk_area = sum(count * math.pi * r * r for count, r in zip(counts, radii))
    packing = rng.uniform(0.055, 0.14)
    area = max(200.0, disk_area / packing)
    aspect = rng.uniform(1.8, 2.8)
    ly_raw = math.sqrt(area / aspect)
    lx_raw = aspect * ly_raw
    # Avoid boxes that are visually too tiny, but keep area large enough for placement.
    ly = max(8.0, min(24.0, ly_raw))
    lx = max(lx_raw, area / ly, 2.0 * ly)
    lx = min(500.0, lx)
    return _float(lx), _float(ly), _float(packing)


def _choose_counts(num_particles: int, species_count: int, rng: random.Random) -> list[int]:
    if species_count == 1:
        return [num_particles]
    ratio = rng.uniform(0.25, 0.75)
    n0 = max(1, min(num_particles - 1, int(round(num_particles * ratio))))
    return [n0, num_particles - n0]


def _row(run_index: int, rng: random.Random, preset: str, snapshot_interval: int) -> dict[str, object]:
    if preset == "local_small":
        num_particles = rng.choice([80, 120, 160, 220])
        steps = rng.choice([8000, 12000, 16000])
    elif preset == "euler_medium":
        num_particles = rng.choice([150, 250, 400, 650, 1000, 1500])
        steps = rng.choice([12000, 18000, 24000, 32000])
    elif preset == "euler_large":
        num_particles = rng.choice([500, 1000, 1500, 2500, 4000])
        steps = rng.choice([18000, 26000, 36000, 50000])
    else:
        raise ValueError(f"Unknown preset: {preset}")

    scenario = rng.choices(
        ["one_species_demon", "two_species_demon", "driven_friction", "equilibrium_control"],
        weights=[0.28, 0.35, 0.22, 0.15],
        k=1,
    )[0]
    species_count = 2 if scenario in {"two_species_demon", "driven_friction"} and rng.random() < 0.85 else 1
    counts = _choose_counts(num_particles, species_count, rng)

    r0 = rng.uniform(0.075, 0.17)
    radii = [r0]
    if species_count == 2:
        # Keep the second species visibly different but still easy to place.
        r1 = max(0.06, min(0.24, r0 * rng.uniform(0.65, 1.55)))
        if abs(r1 - r0) < 0.025:
            r1 = max(0.06, min(0.24, r1 + 0.04))
        radii.append(r1)

    lx, ly, packing = _safe_box(num_particles, radii, counts, rng)
    middle_wall_x = _float(0.5 * lx)
    max_radius = max(radii)
    max_opening = max(2.5 * max_radius, ly - 2.2 * max_radius)
    min_opening = max(3.0 * max_radius, 0.08 * ly)

    if scenario == "equilibrium_control":
        demon_mode = "always_open"
        opening_height = rng.uniform(0.25 * ly, 0.85 * ly)
        threshold = rng.uniform(0.1, 2.0)
        friction = rng.choice([0.0, rng.uniform(0.002, 0.025)])
        drive = 0.0 if friction == 0.0 else rng.uniform(0.02, 0.16)
        stop_at_thermal = True
        stop_at_steady = False
    else:
        demon_mode = "hot_right_cold_left"
        # Some runs should be strong separation, some weaker for model training diversity.
        threshold = rng.choice([rng.uniform(0.2, 0.6), rng.uniform(0.6, 1.4), rng.uniform(1.4, 2.4)])
        opening_height = rng.choice([rng.uniform(0.10 * ly, 0.35 * ly), rng.uniform(0.35 * ly, 0.90 * ly)])
        friction = rng.choice([0.0, rng.uniform(0.001, 0.035)])
        drive = 0.0 if scenario != "driven_friction" else rng.uniform(0.02, 0.35)
        stop_at_thermal = False
        stop_at_steady = False

    opening_height = max(min_opening, min(max_opening, opening_height))
    opening_center = _float(0.5 * ly)

    mass0 = rng.uniform(0.7, 2.0)
    masses = [mass0]
    if species_count == 2:
        # In 2D, mass proportional to area is a reasonable first model, with jitter.
        area_ratio = (radii[1] / radii[0]) ** 2
        masses.append(max(0.4, min(5.0, mass0 * area_ratio * rng.uniform(0.65, 1.35))))

    # Per-species friction/drive can differ slightly.
    gamma0 = friction * rng.uniform(0.75, 1.25)
    drive0 = drive * rng.uniform(0.75, 1.25)
    gamma1 = friction * rng.uniform(0.75, 1.25) if species_count == 2 else 0.0
    drive1 = drive * rng.uniform(0.75, 1.25) if species_count == 2 else 0.0

    seed = rng.randint(1, 2_000_000_000)
    row: dict[str, object] = {
        "run_id": f"run_{run_index:06d}",
        "scenario": scenario,
        "num_particles": sum(counts),
        "lx": lx,
        "ly": ly,
        "packing_fraction_target": packing,
        "dt": 0.004,
        "steps": steps,
        "snapshot_interval": snapshot_interval,
        "observable_interval": 40 if preset == "local_small" else 80,
        "live_log_interval": 0,
        "collision_iterations": 2,
        "seed": seed,
        "velocity_std": _float(rng.uniform(0.7, 1.6)),
        "initial_left_fraction": _float(rng.uniform(0.45, 0.55)),
        "middle_wall_x": middle_wall_x,
        "opening_centers": opening_center,
        "opening_height": _float(opening_height),
        "demon_threshold": _float(threshold),
        "demon_mode": demon_mode,
        "restitution": _float(rng.choice([1.0, 1.0, 1.0, rng.uniform(0.96, 0.995)])),
        "detect_thermal_equilibrium": "true",
        "stop_at_thermal_equilibrium": "true" if stop_at_thermal else "false",
        "stop_at_steady_state": "true" if stop_at_steady else "false",
        "equilibrium_window": 80 if preset == "local_small" else 120,
        "equilibrium_required_windows": 3,
        "equilibrium_temp_tol": 0.04,
        "equilibrium_density_tol": 0.04,
        "equilibrium_flux_tol": 0.08,
        "equilibrium_slope_tol": 0.03,
        "separated_steady_state_min_score": 0.10,
        "species_count": species_count,
        "species0_count": counts[0],
        "species0_radius": _float(radii[0]),
        "species0_mass": _float(masses[0]),
        "species0_friction_gamma": _float(gamma0),
        "species0_drive_strength": _float(drive0),
        "species1_count": counts[1] if species_count == 2 else 0,
        "species1_radius": _float(radii[1]) if species_count == 2 else 0.0,
        "species1_mass": _float(masses[1]) if species_count == 2 else 0.0,
        "species1_friction_gamma": _float(gamma1) if species_count == 2 else 0.0,
        "species1_drive_strength": _float(drive1) if species_count == 2 else 0.0,
    }
    return row


def generate(n_runs: int, seed: int, preset: str, snapshot_interval: int) -> list[dict[str, object]]:
    rng = random.Random(seed)
    return [_row(i, rng, preset, snapshot_interval) for i in range(n_runs)]


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a CSV design table for Maxwell demon parameter sweeps.")
    parser.add_argument("--out", type=Path, required=True, help="Output design CSV")
    parser.add_argument("--n-runs", type=int, default=64, help="Number of simulation rows")
    parser.add_argument("--seed", type=int, default=1234, help="Random seed for design generation")
    parser.add_argument("--preset", choices=["local_small", "euler_medium", "euler_large"], default="local_small")
    parser.add_argument("--snapshot-interval", type=int, default=0, help="0 disables snapshots.csv; use >0 only for selected videos")
    args = parser.parse_args()

    if args.n_runs <= 0:
        raise ValueError("--n-runs must be positive")
    rows = generate(args.n_runs, args.seed, args.preset, args.snapshot_interval)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0].keys())
    with args.out.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    print(f"Wrote {len(rows)} design rows to {args.out}")
    print("For Euler arrays use: sbatch --array=0-{}%<concurrency> euler/run_array_cpu.sbatch".format(len(rows) - 1))


if __name__ == "__main__":
    main()
