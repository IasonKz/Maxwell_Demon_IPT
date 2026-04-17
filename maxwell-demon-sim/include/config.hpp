#pragma once

#include <string>
#include <vector>

struct Config {
    int num_particles = 140;
    double lx = 20.0;
    double ly = 10.0;
    double radius = 0.14;
    double mass = 1.0;
    double dt = 0.004;
    int steps = 6000;
    int snapshot_interval = 10;
    int observable_interval = 5;
    int collision_iterations = 2;
    int seed = 42;
    double velocity_std = 1.0;
    double middle_wall_x = -1.0;
    std::vector<double> opening_centers{3.0, 7.0};
    double opening_height = 1.2;
    double demon_threshold = 1.0;
    std::string demon_mode = "hot_right_cold_left";
    std::string output_dir = "output/default";
    bool overwrite_output = true;

    static Config from_file(const std::string& path);
    void validate() const;
};
