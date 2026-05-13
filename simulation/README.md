# Local simulation folder

This folder contains ready-to-run local configurations and helper scripts.

The C++ simulation core lives in `../src` and `../include`; build from the repository root.

## Quick local run with GIF

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
bash simulation/run_local_demo.sh
```

Main outputs:

```text
output/local_demo_separation/separation.gif
output/local_demo_separation/observables.csv
output/local_demo_separation/summary.json
output/local_demo_separation/diagnostics_temperature.png
output/local_demo_separation/diagnostics_final_snapshot.png
```

The GIF overlay includes live chamber statistics:

```text
T_left, T_right, temperature contrast, N_left, N_right, separation score
```

## Larger local run

```bash
bash simulation/run_big_local.sh
```

The default large run has `snapshot_interval = 0`, so it does not write a huge `snapshots.csv`. It still writes:

```text
observables.csv
summary.json
final_snapshot.csv
```

## Live temperature statistic

The simulator writes `temperature_left` and `temperature_right` every `observable_interval` steps in `observables.csv`.

The 2D kinetic temperature uses `k_B = 1`:

```text
T_chamber = thermal_kinetic_energy_chamber / N_chamber
```

The center-of-mass chamber velocity is removed before calculating thermal kinetic energy, so bulk motion is not misread as heat.

Set this in a config to print live telemetry to the console:

```text
live_log_interval = 400
```

Use this for a live/refreshing temperature plot while the simulation is running:

```bash
python3 scripts/live_temperature_plot.py output/local_demo_separation --poll 1.0
```

## Changing species / ball populations

One population:

```text
species_count = 1
species0_count = 120
species0_radius = 0.12
species0_mass = 1.0
species0_friction_gamma = 0.0
species0_drive_strength = 0.0
```

Two populations:

```text
species_count = 2
species0_count = 700
species0_radius = 0.08
species0_mass = 1.0
species0_friction_gamma = 0.0
species0_drive_strength = 0.0
species1_count = 300
species1_radius = 0.13
species1_mass = 2.5
species1_friction_gamma = 0.0
species1_drive_strength = 0.0
```

Important: the total particle count is the sum of the species counts.
