#include "equilibrium.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kTiny = 1e-12;

double safe_rel_diff(double a, double b) {
    return std::abs(a - b) / std::max(0.5 * (std::abs(a) + std::abs(b)), kTiny);
}

double bounded_quality(double temp_error, double density_error, double flux_error, double slope_error,
                       const Config& cfg) {
    const double temp_ratio = temp_error / std::max(cfg.equilibrium_temp_tol, kTiny);
    const double density_ratio = density_error / std::max(cfg.equilibrium_density_tol, kTiny);
    const double flux_ratio = flux_error / std::max(cfg.equilibrium_flux_tol, kTiny);
    const double slope_ratio = slope_error / std::max(cfg.equilibrium_slope_tol, kTiny);
    const double mean_ratio = 0.25 * (temp_ratio + density_ratio + flux_ratio + slope_ratio);
    return std::clamp(1.0 - mean_ratio, 0.0, 1.0);
}
} // namespace

EquilibriumDetector::EquilibriumDetector(const Config& cfg) : cfg_(cfg) {}

EquilibriumStatus EquilibriumDetector::observe(const ObservableRow& row) {
    window_.push_back(row);
    while (static_cast<int>(window_.size()) > cfg_.equilibrium_window) window_.pop_front();

    EquilibriumStatus current = compute_window_status(row);

    if (current.thermal_candidate) ++thermal_streak_; else thermal_streak_ = 0;
    if (current.steady_candidate) ++steady_streak_; else steady_streak_ = 0;
    if (current.separated_steady_candidate) ++separated_streak_; else separated_streak_ = 0;

    const bool thermal_now = cfg_.detect_thermal_equilibrium &&
                             thermal_streak_ >= cfg_.equilibrium_required_windows;
    const bool steady_now = steady_streak_ >= cfg_.equilibrium_required_windows;
    const bool separated_now = separated_streak_ >= cfg_.equilibrium_required_windows;

    if (thermal_now && !status_.thermal_equilibrium_reached) {
        status_.thermal_equilibrium_reached = true;
        status_.thermal_equilibrium_step = row.step;
        status_.thermal_equilibrium_time = row.time;
    }
    if (steady_now && !status_.steady_state_reached) {
        status_.steady_state_reached = true;
        status_.steady_state_step = row.step;
        status_.steady_state_time = row.time;
    }
    if (separated_now && !status_.separated_steady_state_reached) {
        status_.separated_steady_state_reached = true;
        status_.separated_steady_state_step = row.step;
        status_.separated_steady_state_time = row.time;
    }

    current.thermal_equilibrium_reached = status_.thermal_equilibrium_reached;
    current.steady_state_reached = status_.steady_state_reached;
    current.separated_steady_state_reached = status_.separated_steady_state_reached;
    current.thermal_equilibrium_step = status_.thermal_equilibrium_step;
    current.steady_state_step = status_.steady_state_step;
    current.separated_steady_state_step = status_.separated_steady_state_step;
    current.thermal_equilibrium_time = status_.thermal_equilibrium_time;
    current.steady_state_time = status_.steady_state_time;
    current.separated_steady_state_time = status_.separated_steady_state_time;

    status_ = current;
    return status_;
}

EquilibriumStatus EquilibriumDetector::compute_window_status(const ObservableRow& row) const {
    EquilibriumStatus result;
    result.temp_error = safe_rel_diff(row.temperature_left, row.temperature_right);
    result.density_error = safe_rel_diff(row.density_left, row.density_right);

    if (window_.size() < static_cast<std::size_t>(cfg_.equilibrium_window)) return result;

    const auto& first = window_.front();
    const auto& last = window_.back();
    const long long lr_delta = last.gate_stats.accepted_lr - first.gate_stats.accepted_lr;
    const long long rl_delta = last.gate_stats.accepted_rl - first.gate_stats.accepted_rl;
    result.flux_error = std::abs(static_cast<double>(lr_delta - rl_delta)) /
                        std::max(static_cast<double>(lr_delta + rl_delta), 1.0);

    const double t_left_drift = safe_rel_diff(last.temperature_left, first.temperature_left);
    const double t_right_drift = safe_rel_diff(last.temperature_right, first.temperature_right);
    const double n_left_drift = safe_rel_diff(static_cast<double>(last.n_left), static_cast<double>(first.n_left));
    const double n_right_drift = safe_rel_diff(static_cast<double>(last.n_right), static_cast<double>(first.n_right));
    const double sep_drift = safe_rel_diff(last.separation_score, first.separation_score);
    result.slope_error = std::max({t_left_drift, t_right_drift, n_left_drift, n_right_drift, sep_drift});

    result.equilibrium_quality = bounded_quality(result.temp_error, result.density_error, result.flux_error,
                                                 result.slope_error, cfg_);
    result.thermal_candidate = result.temp_error <= cfg_.equilibrium_temp_tol &&
                               result.density_error <= cfg_.equilibrium_density_tol &&
                               result.flux_error <= cfg_.equilibrium_flux_tol &&
                               result.slope_error <= cfg_.equilibrium_slope_tol;
    result.steady_candidate = result.flux_error <= cfg_.equilibrium_flux_tol &&
                              result.slope_error <= cfg_.equilibrium_slope_tol;
    result.separated_steady_candidate = result.steady_candidate &&
                                        row.separation_score >= cfg_.separated_steady_state_min_score &&
                                        !result.thermal_candidate;
    return result;
}

const EquilibriumStatus& EquilibriumDetector::status() const noexcept { return status_; }

SummaryRow EquilibriumDetector::summary() const {
    SummaryRow row;
    row.thermal_equilibrium_reached = status_.thermal_equilibrium_reached;
    row.steady_state_reached = status_.steady_state_reached;
    row.separated_steady_state_reached = status_.separated_steady_state_reached;
    row.thermal_equilibrium_step = status_.thermal_equilibrium_step;
    row.steady_state_step = status_.steady_state_step;
    row.separated_steady_state_step = status_.separated_steady_state_step;
    row.thermal_equilibrium_time = status_.thermal_equilibrium_time;
    row.steady_state_time = status_.steady_state_time;
    row.separated_steady_state_time = status_.separated_steady_state_time;
    return row;
}
