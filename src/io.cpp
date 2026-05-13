#include "io.hpp"

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <stdexcept>

namespace fs = std::filesystem;

CsvWriters::CsvWriters(const Config& cfg) : output_dir_(cfg.output_dir) {
    if (cfg.overwrite_output && fs::exists(output_dir_)) fs::remove_all(output_dir_);
    fs::create_directories(output_dir_);

    if (cfg.snapshot_interval > 0) {
        snapshots_.open(fs::path(output_dir_) / "snapshots.csv");
        if (!snapshots_) throw std::runtime_error("Failed to open snapshots.csv in " + output_dir_);
        write_snapshot_header(snapshots_);
    }

    observables_.open(fs::path(output_dir_) / "observables.csv");
    if (!observables_) throw std::runtime_error("Failed to open observables.csv in " + output_dir_);
    observables_ << "step,time,n_left,n_right,density_left,density_right,"
                 << "temperature_left,temperature_right,temperature_contrast,population_imbalance,separation_score,"
                 << "mean_speed_left,mean_speed_right,kinetic_energy_left,kinetic_energy_right,kinetic_energy_total,"
                 << "energy_injected_total,energy_dissipated_total,"
                 << "temp_error,density_error,flux_error,slope_error,equilibrium_quality,"
                 << "thermal_equilibrium_candidate,thermal_equilibrium_reached,steady_state_reached,separated_steady_state_reached,"
                 << "accepted_lr,accepted_rl,rejected_lr,rejected_rl\n";
}

void CsvWriters::write_metadata(const Config& cfg) {
    std::ofstream meta(fs::path(output_dir_) / "metadata.json");
    if (!meta) throw std::runtime_error("Could not write metadata.json");

    meta << std::fixed << std::setprecision(8);
    meta << "{\n";
    meta << "  \"num_particles\": " << cfg.num_particles << ",\n";
    meta << "  \"lx\": " << cfg.lx << ",\n";
    meta << "  \"ly\": " << cfg.ly << ",\n";
    meta << "  \"radius\": " << cfg.radius << ",\n";
    meta << "  \"mass\": " << cfg.mass << ",\n";
    meta << "  \"max_radius\": " << cfg.max_radius() << ",\n";
    meta << "  \"friction_gamma\": " << cfg.friction_gamma << ",\n";
    meta << "  \"drive_strength\": " << cfg.drive_strength << ",\n";
    meta << "  \"restitution\": " << cfg.restitution << ",\n";
    meta << "  \"dt\": " << cfg.dt << ",\n";
    meta << "  \"steps\": " << cfg.steps << ",\n";
    meta << "  \"snapshot_interval\": " << cfg.snapshot_interval << ",\n";
    meta << "  \"observable_interval\": " << cfg.observable_interval << ",\n";
    meta << "  \"live_log_interval\": " << cfg.live_log_interval << ",\n";
    meta << "  \"collision_iterations\": " << cfg.collision_iterations << ",\n";
    meta << "  \"seed\": " << cfg.seed << ",\n";
    meta << "  \"velocity_std\": " << cfg.velocity_std << ",\n";
    meta << "  \"initial_left_fraction\": " << cfg.initial_left_fraction << ",\n";
    meta << "  \"middle_wall_x\": " << cfg.middle_wall_x << ",\n";
    meta << "  \"opening_height\": " << cfg.opening_height << ",\n";
    meta << "  \"demon_threshold\": " << cfg.demon_threshold << ",\n";
    meta << "  \"demon_mode\": \"" << cfg.demon_mode << "\",\n";
    meta << "  \"detect_thermal_equilibrium\": " << (cfg.detect_thermal_equilibrium ? "true" : "false") << ",\n";
    meta << "  \"stop_at_thermal_equilibrium\": " << (cfg.stop_at_thermal_equilibrium ? "true" : "false") << ",\n";
    meta << "  \"stop_at_steady_state\": " << (cfg.stop_at_steady_state ? "true" : "false") << ",\n";
    meta << "  \"equilibrium_window\": " << cfg.equilibrium_window << ",\n";
    meta << "  \"equilibrium_required_windows\": " << cfg.equilibrium_required_windows << ",\n";
    meta << "  \"equilibrium_temp_tol\": " << cfg.equilibrium_temp_tol << ",\n";
    meta << "  \"equilibrium_density_tol\": " << cfg.equilibrium_density_tol << ",\n";
    meta << "  \"equilibrium_flux_tol\": " << cfg.equilibrium_flux_tol << ",\n";
    meta << "  \"equilibrium_slope_tol\": " << cfg.equilibrium_slope_tol << ",\n";
    meta << "  \"separated_steady_state_min_score\": " << cfg.separated_steady_state_min_score << ",\n";
    meta << "  \"opening_centers\": [";
    for (std::size_t i = 0; i < cfg.opening_centers.size(); ++i) {
        if (i != 0) meta << ", ";
        meta << cfg.opening_centers[i];
    }
    meta << "],\n";
    meta << "  \"species\": [\n";
    for (std::size_t i = 0; i < cfg.species.size(); ++i) {
        const auto& sp = cfg.species[i];
        meta << "    {\"id\": " << sp.id << ", \"count\": " << sp.count
             << ", \"radius\": " << sp.radius << ", \"mass\": " << sp.mass
             << ", \"friction_gamma\": " << sp.friction_gamma
             << ", \"drive_strength\": " << sp.drive_strength << "}";
        if (i + 1 != cfg.species.size()) meta << ',';
        meta << "\n";
    }
    meta << "  ]\n";
    meta << "}\n";

    std::ofstream cfg_copy(fs::path(output_dir_) / "config_used.cfg");
    if (cfg_copy) {
        cfg_copy << "num_particles = " << cfg.num_particles << "\n";
        cfg_copy << "lx = " << cfg.lx << "\nly = " << cfg.ly << "\n";
        cfg_copy << "radius = " << cfg.radius << "\nmass = " << cfg.mass << "\n";
        cfg_copy << "friction_gamma = " << cfg.friction_gamma << "\ndrive_strength = " << cfg.drive_strength << "\n";
        cfg_copy << "restitution = " << cfg.restitution << "\n";
        cfg_copy << "dt = " << cfg.dt << "\nsteps = " << cfg.steps << "\n";
        cfg_copy << "snapshot_interval = " << cfg.snapshot_interval << "\nobservable_interval = " << cfg.observable_interval << "\n";
        cfg_copy << "live_log_interval = " << cfg.live_log_interval << "\n";
        cfg_copy << "collision_iterations = " << cfg.collision_iterations << "\nseed = " << cfg.seed << "\n";
        cfg_copy << "velocity_std = " << cfg.velocity_std << "\ninitial_left_fraction = " << cfg.initial_left_fraction << "\n";
        cfg_copy << "middle_wall_x = " << cfg.middle_wall_x << "\nopening_centers = ";
        for (std::size_t i = 0; i < cfg.opening_centers.size(); ++i) {
            if (i != 0) cfg_copy << ", ";
            cfg_copy << cfg.opening_centers[i];
        }
        cfg_copy << "\nopening_height = " << cfg.opening_height << "\n";
        cfg_copy << "demon_threshold = " << cfg.demon_threshold << "\ndemon_mode = " << cfg.demon_mode << "\n";
        cfg_copy << "output_dir = " << cfg.output_dir << "\noverwrite_output = true\n";
        cfg_copy << "detect_thermal_equilibrium = " << (cfg.detect_thermal_equilibrium ? "true" : "false") << "\n";
        cfg_copy << "stop_at_thermal_equilibrium = " << (cfg.stop_at_thermal_equilibrium ? "true" : "false") << "\n";
        cfg_copy << "stop_at_steady_state = " << (cfg.stop_at_steady_state ? "true" : "false") << "\n";
        cfg_copy << "equilibrium_window = " << cfg.equilibrium_window << "\n";
        cfg_copy << "equilibrium_required_windows = " << cfg.equilibrium_required_windows << "\n";
        cfg_copy << "equilibrium_temp_tol = " << cfg.equilibrium_temp_tol << "\n";
        cfg_copy << "equilibrium_density_tol = " << cfg.equilibrium_density_tol << "\n";
        cfg_copy << "equilibrium_flux_tol = " << cfg.equilibrium_flux_tol << "\n";
        cfg_copy << "equilibrium_slope_tol = " << cfg.equilibrium_slope_tol << "\n";
        cfg_copy << "separated_steady_state_min_score = " << cfg.separated_steady_state_min_score << "\n";
        cfg_copy << "species_count = " << cfg.species.size() << "\n";
        for (const auto& sp : cfg.species) {
            cfg_copy << "species" << sp.id << "_count = " << sp.count << "\n";
            cfg_copy << "species" << sp.id << "_radius = " << sp.radius << "\n";
            cfg_copy << "species" << sp.id << "_mass = " << sp.mass << "\n";
            cfg_copy << "species" << sp.id << "_friction_gamma = " << sp.friction_gamma << "\n";
            cfg_copy << "species" << sp.id << "_drive_strength = " << sp.drive_strength << "\n";
        }
    }
}

void CsvWriters::write_snapshot_header(std::ostream& out) {
    out << "step,time,id,species_id,x,y,vx,vy,speed,radius,mass,chamber\n";
}

void CsvWriters::write_snapshot_rows(std::ostream& out, int step, double time, const std::vector<Particle>& particles) {
    out << std::fixed << std::setprecision(8);
    for (const auto& p : particles) {
        const double speed = std::sqrt(p.vx * p.vx + p.vy * p.vy);
        out << step << ',' << time << ',' << p.id << ',' << p.species_id << ',' << p.x << ',' << p.y << ','
            << p.vx << ',' << p.vy << ',' << speed << ',' << p.radius << ',' << p.mass << ',' << p.chamber << '\n';
    }
}

void CsvWriters::write_snapshot(int step, double time, const std::vector<Particle>& particles) {
    if (!snapshots_) return;
    write_snapshot_rows(snapshots_, step, time, particles);
}

void CsvWriters::write_named_snapshot(const std::string& filename, int step, double time, const std::vector<Particle>& particles) {
    std::ofstream out(fs::path(output_dir_) / filename);
    if (!out) throw std::runtime_error("Could not write " + filename);
    write_snapshot_header(out);
    write_snapshot_rows(out, step, time, particles);
}

void CsvWriters::write_observable(const ObservableRow& row) {
    observables_ << std::fixed << std::setprecision(8)
                 << row.step << ',' << row.time << ',' << row.n_left << ',' << row.n_right << ','
                 << row.density_left << ',' << row.density_right << ','
                 << row.temperature_left << ',' << row.temperature_right << ','
                 << row.temperature_contrast << ',' << row.population_imbalance << ',' << row.separation_score << ','
                 << row.mean_speed_left << ',' << row.mean_speed_right << ','
                 << row.kinetic_energy_left << ',' << row.kinetic_energy_right << ',' << row.kinetic_energy_total << ','
                 << row.energy_injected_total << ',' << row.energy_dissipated_total << ','
                 << row.temp_error << ',' << row.density_error << ',' << row.flux_error << ',' << row.slope_error << ','
                 << row.equilibrium_quality << ',' << row.thermal_equilibrium_candidate << ','
                 << row.thermal_equilibrium_reached << ',' << row.steady_state_reached << ','
                 << row.separated_steady_state_reached << ','
                 << row.gate_stats.accepted_lr << ',' << row.gate_stats.accepted_rl << ','
                 << row.gate_stats.rejected_lr << ',' << row.gate_stats.rejected_rl << '\n';
    observables_.flush();
}

void CsvWriters::write_summary(const Config& cfg, const ObservableRow& final_row, const SummaryRow& summary) {
    std::ofstream out(fs::path(output_dir_) / "summary.json");
    if (!out) throw std::runtime_error("Could not write summary.json");
    out << std::fixed << std::setprecision(8);
    out << "{\n";
    out << "  \"num_particles\": " << cfg.num_particles << ",\n";
    out << "  \"species_count\": " << cfg.species.size() << ",\n";
    out << "  \"steps_configured\": " << cfg.steps << ",\n";
    out << "  \"final_step\": " << final_row.step << ",\n";
    out << "  \"final_time\": " << final_row.time << ",\n";
    out << "  \"thermal_equilibrium_reached\": " << (summary.thermal_equilibrium_reached ? "true" : "false") << ",\n";
    out << "  \"thermal_equilibrium_step\": " << summary.thermal_equilibrium_step << ",\n";
    out << "  \"thermal_equilibrium_time\": " << summary.thermal_equilibrium_time << ",\n";
    out << "  \"steady_state_reached\": " << (summary.steady_state_reached ? "true" : "false") << ",\n";
    out << "  \"steady_state_step\": " << summary.steady_state_step << ",\n";
    out << "  \"steady_state_time\": " << summary.steady_state_time << ",\n";
    out << "  \"separated_steady_state_reached\": " << (summary.separated_steady_state_reached ? "true" : "false") << ",\n";
    out << "  \"separated_steady_state_step\": " << summary.separated_steady_state_step << ",\n";
    out << "  \"separated_steady_state_time\": " << summary.separated_steady_state_time << ",\n";
    out << "  \"n_left_final\": " << final_row.n_left << ",\n";
    out << "  \"n_right_final\": " << final_row.n_right << ",\n";
    out << "  \"temperature_left_final\": " << final_row.temperature_left << ",\n";
    out << "  \"temperature_right_final\": " << final_row.temperature_right << ",\n";
    out << "  \"temperature_contrast_final\": " << final_row.temperature_contrast << ",\n";
    out << "  \"population_imbalance_final\": " << final_row.population_imbalance << ",\n";
    out << "  \"separation_score_final\": " << final_row.separation_score << ",\n";
    out << "  \"equilibrium_quality_final\": " << final_row.equilibrium_quality << ",\n";
    out << "  \"temp_error_final\": " << final_row.temp_error << ",\n";
    out << "  \"density_error_final\": " << final_row.density_error << ",\n";
    out << "  \"flux_error_final\": " << final_row.flux_error << ",\n";
    out << "  \"slope_error_final\": " << final_row.slope_error << ",\n";
    out << "  \"kinetic_energy_total_final\": " << final_row.kinetic_energy_total << ",\n";
    out << "  \"energy_injected_total\": " << final_row.energy_injected_total << ",\n";
    out << "  \"energy_dissipated_total\": " << final_row.energy_dissipated_total << ",\n";
    out << "  \"accepted_lr\": " << final_row.gate_stats.accepted_lr << ",\n";
    out << "  \"accepted_rl\": " << final_row.gate_stats.accepted_rl << ",\n";
    out << "  \"rejected_lr\": " << final_row.gate_stats.rejected_lr << ",\n";
    out << "  \"rejected_rl\": " << final_row.gate_stats.rejected_rl << "\n";
    out << "}\n";
}

const std::string& CsvWriters::output_dir() const noexcept { return output_dir_; }
