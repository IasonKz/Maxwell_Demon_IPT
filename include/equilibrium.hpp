#pragma once

#include <deque>

#include "config.hpp"
#include "io.hpp"

struct EquilibriumStatus {
    double temp_error = 0.0;
    double density_error = 0.0;
    double flux_error = 0.0;
    double slope_error = 0.0;
    double equilibrium_quality = 0.0;
    bool thermal_candidate = false;
    bool steady_candidate = false;
    bool separated_steady_candidate = false;
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

class EquilibriumDetector {
public:
    explicit EquilibriumDetector(const Config& cfg);
    EquilibriumStatus observe(const ObservableRow& row);
    const EquilibriumStatus& status() const noexcept;
    SummaryRow summary() const;

private:
    Config cfg_;
    std::deque<ObservableRow> window_;
    EquilibriumStatus status_{};
    int thermal_streak_ = 0;
    int steady_streak_ = 0;
    int separated_streak_ = 0;
    EquilibriumStatus compute_window_status(const ObservableRow& row) const;
};
