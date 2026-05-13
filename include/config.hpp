#pragma once

#include <string>
#include <vector>

#include "species.hpp"

struct Config {
    int num_particles = 140;
    double lx = 20.0;
    double ly = 10.0;
    double radius = 0.14;
    double mass = 1.0;
    double friction_gamma = 0.0;
    double drive_strength = 0.0;
    double restitution = 1.0;
    double dt = 0.004;
    int steps = 6000;
    int snapshot_interval = 10; // use 0 to disable snapshots.csv
    int observable_interval = 5;
    int live_log_interval = 0; // 0 disables console live telemetry; otherwise print every N steps
    int collision_iterations = 2;
    int seed = 42;
    double velocity_std = 1.0;
    double initial_left_fraction = 0.5;
    double middle_wall_x = -1.0;
    std::vector<double> opening_centers{3.0, 7.0};
    double opening_height = 1.2;
    double demon_threshold = 1.0;
    std::string demon_mode = "hot_right_cold_left";
    std::string output_dir = "output/default";
    bool overwrite_output = true;

    bool detect_thermal_equilibrium = true;
    bool stop_at_thermal_equilibrium = false;
    bool stop_at_steady_state = false;
    int equilibrium_window = 120;
    int equilibrium_required_windows = 3;
    double equilibrium_temp_tol = 0.03;
    double equilibrium_density_tol = 0.03;
    double equilibrium_flux_tol = 0.05;
    double equilibrium_slope_tol = 0.02;
    double separated_steady_state_min_score = 0.05;

    std::vector<SpeciesConfig> species{};

    static Config from_file(const std::string& path);
    void validate() const;
    double max_radius() const;
    double max_collision_diameter() const;
};
