# Maxwell Demon Simulation

A GitHub-ready Maxwell demon project with:

- **C++17 simulation core** for a 2D hard-disk gas in a rectangular box.
- **Particle-particle elastic collisions**.
- A **vertical middle wall** with **specific gate openings**.
- A **demon decision rule based on the normal velocity component** `vx` at the wall.
- **Python post-processing** for animation and diagnostics.
- **Parallel execution in two ways**:
  - OpenMP for independent per-particle stages and observable reductions when available.
  - Parallel parameter sweeps across many runs with `scripts/run_sweep.py`.

## Physics model

The domain is a rectangle `[0, Lx] x [0, Ly]` split by a vertical wall at `x = middle_wall_x`.
Only particles that hit the wall inside one of the configured opening regions are evaluated by the demon.
The demon uses the **normal velocity component** to the wall, which for a vertical wall is `vx`.

Implemented demon modes:

- `always_open`: baseline reference.
- `fast_both`: particles pass only if the normal component is above the threshold in the direction of travel.
- `hot_right_cold_left`: Maxwell-demon-like mode.
  - left -> right: pass if `vx > threshold`
  - right -> left: pass if `|vx| < threshold`
- `slow_both`: only slow particles pass in either direction.

Particles are modeled as identical hard disks with elastic collisions.
The middle wall also blocks inter-particle collisions across closed sections of the wall.
Only line-of-sight interactions through an opening are allowed.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Run a default simulation:

```bash
./build/maxwell_demon_sim config/default.cfg
```

## Python environment

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Visualize a run

Plot diagnostics:

```bash
python3 scripts/plots.py output/default --save-prefix output/default/diagnostics
```

Animate the particles interactively:

```bash
python3 scripts/animate.py output/default
```

Save a GIF instead:

```bash
python3 scripts/animate.py output/default --save-gif output/default/animation.gif
```

## Parallel sweeps

Run many thresholds and seeds in parallel:

```bash
python3 scripts/run_sweep.py \
  --binary build/maxwell_demon_sim \
  --base-config config/default.cfg \
  --thresholds 0.8 1.0 1.2 \
  --seeds 1 2 3 \
  --workers 4
```

Each run gets its own directory under `output/sweeps/` with:

- `snapshots.csv`
- `observables.csv`
- `metadata.json`
- `config_used.cfg`

## Config format

The simulation uses a lightweight `key = value` config file.
See `config/default.cfg`.

Key parameters:

- `num_particles`
- `lx`, `ly`
- `radius`
- `dt`, `steps`
- `collision_iterations`
- `middle_wall_x`
- `opening_centers`
- `opening_height`
- `demon_threshold`
- `demon_mode`
- `output_dir`

## Output format

`snapshots.csv` columns:

- `step,time,id,x,y,vx,vy,speed,chamber`

`observables.csv` columns:

- `step,time,n_left,n_right,temperature_left,temperature_right,mean_speed_left,mean_speed_right,accepted_lr,accepted_rl,rejected_lr,rejected_rl`

The reported temperature is a **2D kinetic temperature proxy** with `k_B = 1` and identical particle mass.

## Notes on parallelism

The numerically delicate part is the hard-disk collision resolution. For robustness, collision resolution is kept deterministic and local in the main loop.
Parallelism is applied where it is safe and helpful:

- particle advection and wall handling
- observable reductions
- many-run sweeps across thresholds, seeds, and geometry choices

For small test cases the code keeps these loops effectively serial to avoid OpenMP overhead, and automatically uses threaded loops for larger runs.
That makes the project practical both on a laptop and on a cluster.

## Quick start

```bash
./examples/run_local.sh
```

Parallel example:

```bash
./examples/run_parallel_sweep.sh
```

## Suggested next extensions

- event-driven collisions instead of fixed-`dt` overlap correction
- entropy estimates from velocity histograms
- asymmetric openings or different thresholds per opening
- MPI/SLURM job arrays for large parameter scans
