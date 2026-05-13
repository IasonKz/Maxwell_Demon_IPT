#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "particle.hpp"

struct GateStats {
    long long accepted_lr = 0;
    long long accepted_rl = 0;
    long long rejected_lr = 0;
    long long rejected_rl = 0;
};

struct ObservableRow {
    int step = 0;
    double time = 0.0;
    int n_left = 0;
    int n_right = 0;
    double density_left = 0.0;
    double density_right = 0.0;
    double temperature_left = 0.0;
    double temperature_right = 0.0;
    double temperature_contrast = 0.0;
    double population_imbalance = 0.0;
    double separation_score = 0.0;
    double mean_speed_left = 0.0;
    double mean_speed_right = 0.0;
    double kinetic_energy_left = 0.0;
    double kinetic_energy_right = 0.0;
    double kinetic_energy_total = 0.0;
    double energy_injected_total = 0.0;
    double energy_dissipated_total = 0.0;
    double temp_error = 0.0;
    double density_error = 0.0;
    double flux_error = 0.0;
    double slope_error = 0.0;
    double equilibrium_quality = 0.0;
    int thermal_equilibrium_candidate = 0;
    int thermal_equilibrium_reached = 0;
    int steady_state_reached = 0;
    int separated_steady_state_reached = 0;
    GateStats gate_stats{};
};

struct SummaryRow {
    bool thermal_equilibrium_reached = false;
    bool steady_state_reached = false;
    bool separated_steady_state_reached = false;
    int thermal_equilibrium_step = -1;
    int steady_state_step = -1;
    int separated_steady_state_step = -1;
    double thermal_equilibrium_time = -1.0;
    double steady_state_time = -1.0;
    double separated_steady_state_time = -1.0;
};

class CsvWriters {
public:
    explicit CsvWriters(const Config& cfg);
    void write_metadata(const Config& cfg);
    void write_snapshot(int step, double time, const std::vector<Particle>& particles);
    void write_named_snapshot(const std::string& filename, int step, double time, const std::vector<Particle>& particles);
    void write_observable(const ObservableRow& row);
    void write_summary(const Config& cfg, const ObservableRow& final_row, const SummaryRow& summary);
    const std::string& output_dir() const noexcept;

private:
    std::string output_dir_;
    std::ofstream snapshots_;
    std::ofstream observables_;
    static void write_snapshot_header(std::ostream& out);
    static void write_snapshot_rows(std::ostream& out, int step, double time, const std::vector<Particle>& particles);
};
