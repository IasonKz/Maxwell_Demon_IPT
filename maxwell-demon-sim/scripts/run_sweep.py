#!/usr/bin/env python3
from __future__ import annotations

import argparse
import itertools
import subprocess
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path


def read_cfg(path: Path) -> dict[str, str]:
    config: dict[str, str] = {}
    for raw_line in path.read_text().splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        key, value = [part.strip() for part in line.split("=", 1)]
        config[key] = value
    return config


def write_cfg(path: Path, config: dict[str, str]) -> None:
    lines = [f"{key} = {value}" for key, value in config.items()]
    path.write_text("\n".join(lines) + "\n")


def run_one(binary: Path, cfg_path: Path) -> tuple[str, int, str, str]:
    proc = subprocess.run([str(binary), str(cfg_path)], capture_output=True, text=True)
    return str(cfg_path), proc.returncode, proc.stdout, proc.stderr


def main() -> None:
    parser = argparse.ArgumentParser(description="Launch multiple Maxwell demon runs in parallel.")
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--base-config", type=Path, required=True)
    parser.add_argument("--out-root", type=Path, default=Path("output/sweeps"))
    parser.add_argument("--thresholds", nargs="+", type=float, default=[0.8, 1.0, 1.2])
    parser.add_argument("--seeds", nargs="+", type=int, default=[1, 2, 3])
    parser.add_argument("--workers", type=int, default=4)
    args = parser.parse_args()

    base = read_cfg(args.base_config)
    args.out_root.mkdir(parents=True, exist_ok=True)

    configs: list[Path] = []
    for threshold, seed in itertools.product(args.thresholds, args.seeds):
        label = f"thr_{threshold:.3f}_seed_{seed}"
        run_dir = args.out_root / label
        run_dir.mkdir(parents=True, exist_ok=True)
        cfg = dict(base)
        cfg["demon_threshold"] = f"{threshold}"
        cfg["seed"] = f"{seed}"
        cfg["output_dir"] = str(run_dir)
        cfg["overwrite_output"] = "true"
        cfg_path = run_dir / "run.cfg"
        write_cfg(cfg_path, cfg)
        configs.append(cfg_path)

    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(run_one, args.binary, cfg_path) for cfg_path in configs]
        failures = 0
        for future in as_completed(futures):
            cfg_path, returncode, stdout, stderr = future.result()
            if returncode != 0:
                failures += 1
                print(f"[FAIL] {cfg_path}\n{stdout}\n{stderr}")
            else:
                print(f"[OK] {cfg_path}\n{stdout.strip()}")

    if failures:
        raise SystemExit(f"Completed with {failures} failed runs.")


if __name__ == "__main__":
    main()
