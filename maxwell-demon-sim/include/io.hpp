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
    double temperature_left = 0.0;
    double temperature_right = 0.0;
    double mean_speed_left = 0.0;
    double mean_speed_right = 0.0;
    GateStats gate_stats{};
};

class CsvWriters {
public:
    explicit CsvWriters(const Config& cfg);
    void write_metadata(const Config& cfg);
    void write_snapshot(int step, double time, const std::vector<Particle>& particles);
    void write_observable(const ObservableRow& row);
    const std::string& output_dir() const noexcept;

private:
    std::string output_dir_;
    std::ofstream snapshots_;
    std::ofstream observables_;
};
