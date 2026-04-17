#include "io.hpp"

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <stdexcept>

namespace fs = std::filesystem;

CsvWriters::CsvWriters(const Config& cfg) : output_dir_(cfg.output_dir) {
    if (cfg.overwrite_output && fs::exists(output_dir_)) {
        fs::remove_all(output_dir_);
    }
    fs::create_directories(output_dir_);

    snapshots_.open(fs::path(output_dir_) / "snapshots.csv");
    observables_.open(fs::path(output_dir_) / "observables.csv");

    if (!snapshots_ || !observables_) {
        throw std::runtime_error("Failed to open output CSV files in " + output_dir_);
    }

    snapshots_ << "step,time,id,x,y,vx,vy,speed,chamber\n";
    observables_ << "step,time,n_left,n_right,temperature_left,temperature_right,mean_speed_left,mean_speed_right,accepted_lr,accepted_rl,rejected_lr,rejected_rl\n";
}

void CsvWriters::write_metadata(const Config& cfg) {
    std::ofstream meta(fs::path(output_dir_) / "metadata.json");
    if (!meta) {
        throw std::runtime_error("Could not write metadata.json");
    }

    meta << std::fixed << std::setprecision(8);
    meta << "{\n";
    meta << "  \"num_particles\": " << cfg.num_particles << ",\n";
    meta << "  \"lx\": " << cfg.lx << ",\n";
    meta << "  \"ly\": " << cfg.ly << ",\n";
    meta << "  \"radius\": " << cfg.radius << ",\n";
    meta << "  \"mass\": " << cfg.mass << ",\n";
    meta << "  \"dt\": " << cfg.dt << ",\n";
    meta << "  \"steps\": " << cfg.steps << ",\n";
    meta << "  \"snapshot_interval\": " << cfg.snapshot_interval << ",\n";
    meta << "  \"observable_interval\": " << cfg.observable_interval << ",\n";
    meta << "  \"collision_iterations\": " << cfg.collision_iterations << ",\n";
    meta << "  \"seed\": " << cfg.seed << ",\n";
    meta << "  \"velocity_std\": " << cfg.velocity_std << ",\n";
    meta << "  \"middle_wall_x\": " << cfg.middle_wall_x << ",\n";
    meta << "  \"opening_height\": " << cfg.opening_height << ",\n";
    meta << "  \"demon_threshold\": " << cfg.demon_threshold << ",\n";
    meta << "  \"demon_mode\": \"" << cfg.demon_mode << "\",\n";
    meta << "  \"opening_centers\": [";
    for (std::size_t i = 0; i < cfg.opening_centers.size(); ++i) {
        if (i != 0) meta << ", ";
        meta << cfg.opening_centers[i];
    }
    meta << "]\n";
    meta << "}\n";

    std::ofstream cfg_copy(fs::path(output_dir_) / "config_used.cfg");
    if (cfg_copy) {
        cfg_copy << "num_particles = " << cfg.num_particles << "\n";
        cfg_copy << "lx = " << cfg.lx << "\n";
        cfg_copy << "ly = " << cfg.ly << "\n";
        cfg_copy << "radius = " << cfg.radius << "\n";
        cfg_copy << "mass = " << cfg.mass << "\n";
        cfg_copy << "dt = " << cfg.dt << "\n";
        cfg_copy << "steps = " << cfg.steps << "\n";
        cfg_copy << "snapshot_interval = " << cfg.snapshot_interval << "\n";
        cfg_copy << "observable_interval = " << cfg.observable_interval << "\n";
        cfg_copy << "collision_iterations = " << cfg.collision_iterations << "\n";
        cfg_copy << "seed = " << cfg.seed << "\n";
        cfg_copy << "velocity_std = " << cfg.velocity_std << "\n";
        cfg_copy << "middle_wall_x = " << cfg.middle_wall_x << "\n";
        cfg_copy << "opening_centers = ";
        for (std::size_t i = 0; i < cfg.opening_centers.size(); ++i) {
            if (i != 0) cfg_copy << ", ";
            cfg_copy << cfg.opening_centers[i];
        }
        cfg_copy << "\n";
        cfg_copy << "opening_height = " << cfg.opening_height << "\n";
        cfg_copy << "demon_threshold = " << cfg.demon_threshold << "\n";
        cfg_copy << "demon_mode = " << cfg.demon_mode << "\n";
        cfg_copy << "output_dir = " << cfg.output_dir << "\n";
        cfg_copy << "overwrite_output = true\n";
    }
}

void CsvWriters::write_snapshot(int step, double time, const std::vector<Particle>& particles) {
    snapshots_ << std::fixed << std::setprecision(8);
    for (const auto& p : particles) {
        const double speed = std::sqrt(p.vx * p.vx + p.vy * p.vy);
        snapshots_ << step << ',' << time << ',' << p.id << ',' << p.x << ',' << p.y << ','
                   << p.vx << ',' << p.vy << ',' << speed << ',' << p.chamber << '\n';
    }
}

void CsvWriters::write_observable(const ObservableRow& row) {
    observables_ << std::fixed << std::setprecision(8)
                 << row.step << ',' << row.time << ',' << row.n_left << ',' << row.n_right << ','
                 << row.temperature_left << ',' << row.temperature_right << ','
                 << row.mean_speed_left << ',' << row.mean_speed_right << ','
                 << row.gate_stats.accepted_lr << ',' << row.gate_stats.accepted_rl << ','
                 << row.gate_stats.rejected_lr << ',' << row.gate_stats.rejected_rl << '\n';
}

const std::string& CsvWriters::output_dir() const noexcept { return output_dir_; }
