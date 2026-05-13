#include "simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#if defined(MAXWELL_DEMON_USE_OPENMP)
#include <omp.h>
#endif

namespace {
constexpr double kEpsilon = 1e-9;

double squared_distance(const Particle& a, const Particle& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    return dx * dx + dy * dy;
}

double kinetic_energy(const Particle& p) {
    return 0.5 * p.mass * (p.vx * p.vx + p.vy * p.vy);
}
} // namespace

Simulation::Simulation(Config cfg)
    : cfg_(std::move(cfg)),
      demon_(cfg_.demon_threshold, cfg_.demon_mode),
      equilibrium_detector_(cfg_),
      rng_(static_cast<std::mt19937::result_type>(cfg_.seed)),
      writers_(cfg_) {
    initialize_particles();
    writers_.write_metadata(cfg_);
}

bool Simulation::is_in_opening(double y, double particle_radius) const {
    const double margin = particle_radius;
    for (const double center : cfg_.opening_centers) {
        const double low = center - 0.5 * cfg_.opening_height + margin;
        const double high = center + 0.5 * cfg_.opening_height - margin;
        if (y >= low && y <= high) return true;
    }
    return false;
}

bool Simulation::wall_blocks_line_of_sight(const Particle& a, const Particle& b) const {
    const bool opposite_sides = (a.x < cfg_.middle_wall_x && b.x > cfg_.middle_wall_x) ||
                                (a.x > cfg_.middle_wall_x && b.x < cfg_.middle_wall_x);
    if (!opposite_sides) return false;

    const double dx = b.x - a.x;
    if (std::abs(dx) < kEpsilon) return true;

    const double alpha = (cfg_.middle_wall_x - a.x) / dx;
    if (alpha < 0.0 || alpha > 1.0) return false;
    const double y_cross = a.y + alpha * (b.y - a.y);
    return !is_in_opening(y_cross, std::max(a.radius, b.radius));
}

bool Simulation::overlaps_any_existing(const Particle& candidate) const {
    for (const auto& existing : particles_) {
        if (wall_blocks_line_of_sight(candidate, existing)) continue;
        const double min_dist = candidate.radius + existing.radius;
        if (squared_distance(candidate, existing) < min_dist * min_dist) return true;
    }
    return false;
}

void Simulation::initialize_particles() {
    std::normal_distribution<double> velocity_dist(0.0, cfg_.velocity_std);
    std::uniform_real_distribution<double> side_dist(0.0, 1.0);

    particles_.clear();
    particles_.reserve(static_cast<std::size_t>(cfg_.num_particles));

    const int max_attempts = 500000;
    int id = 0;
    for (const auto& sp : cfg_.species) {
        for (int local = 0; local < sp.count; ++local) {
            bool placed = false;
            for (int attempt = 0; attempt < max_attempts; ++attempt) {
                Particle p;
                p.id = id;
                p.species_id = sp.id;
                p.radius = sp.radius;
                p.mass = sp.mass;
                p.friction_gamma = sp.friction_gamma;
                p.drive_strength = sp.drive_strength;
                p.chamber = side_dist(rng_) < cfg_.initial_left_fraction ? 0 : 1;

                const double x_min = p.chamber == 0 ? p.radius : cfg_.middle_wall_x + p.radius;
                const double x_max = p.chamber == 0 ? cfg_.middle_wall_x - p.radius : cfg_.lx - p.radius;
                if (x_max <= x_min) throw std::runtime_error("A chamber is too narrow for the chosen particle radius.");
                std::uniform_real_distribution<double> x_dist(x_min, x_max);
                std::uniform_real_distribution<double> y_dist(p.radius, cfg_.ly - p.radius);
                p.x = x_dist(rng_);
                p.y = y_dist(rng_);
                p.vx = velocity_dist(rng_);
                p.vy = velocity_dist(rng_);

                if (!overlaps_any_existing(p)) {
                    particles_.push_back(p);
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                throw std::runtime_error("Could not place all particles without overlap. Reduce num_particles/radii or enlarge the box.");
            }
            ++id;
        }
    }

    double total_mass = 0.0;
    double momentum_x = 0.0;
    double momentum_y = 0.0;
    for (const auto& p : particles_) {
        total_mass += p.mass;
        momentum_x += p.mass * p.vx;
        momentum_y += p.mass * p.vy;
    }
    const double mean_vx = momentum_x / std::max(total_mass, kEpsilon);
    const double mean_vy = momentum_y / std::max(total_mass, kEpsilon);
    for (auto& p : particles_) {
        p.vx -= mean_vx;
        p.vy -= mean_vy;
    }
}

void Simulation::apply_forces_and_energy_accounting() {
    std::normal_distribution<double> kick_dist(0.0, 1.0);
    for (auto& p : particles_) {
        if (p.friction_gamma > 0.0) {
            const double before = kinetic_energy(p);
            const double damping = std::exp(-p.friction_gamma * cfg_.dt);
            p.vx *= damping;
            p.vy *= damping;
            const double after = kinetic_energy(p);
            if (before > after) energy_dissipated_total_ += before - after;
        }
        if (p.drive_strength > 0.0) {
            const double before = kinetic_energy(p);
            const double sigma = p.drive_strength * std::sqrt(cfg_.dt);
            p.vx += sigma * kick_dist(rng_);
            p.vy += sigma * kick_dist(rng_);
            const double after = kinetic_energy(p);
            if (after >= before) energy_injected_total_ += after - before;
            else energy_dissipated_total_ += before - after;
        }
    }
}

void Simulation::enforce_outer_bounds(Particle& p) const {
    if (p.x < p.radius) {
        p.x = 2.0 * p.radius - p.x;
        p.vx = -p.vx;
    }
    if (p.x > cfg_.lx - p.radius) {
        p.x = 2.0 * (cfg_.lx - p.radius) - p.x;
        p.vx = -p.vx;
    }
    if (p.y < p.radius) {
        p.y = 2.0 * p.radius - p.y;
        p.vy = -p.vy;
    }
    if (p.y > cfg_.ly - p.radius) {
        p.y = 2.0 * (cfg_.ly - p.radius) - p.y;
        p.vy = -p.vy;
    }
}

void Simulation::enforce_middle_wall_consistency(Particle& p) const {
    if (is_in_opening(p.y, p.radius)) return;
    if (p.chamber == 0 && p.x > cfg_.middle_wall_x - p.radius) {
        p.x = cfg_.middle_wall_x - p.radius;
        if (p.vx > 0.0) p.vx = -p.vx;
    }
    if (p.chamber == 1 && p.x < cfg_.middle_wall_x + p.radius) {
        p.x = cfg_.middle_wall_x + p.radius;
        if (p.vx < 0.0) p.vx = -p.vx;
    }
}

void Simulation::advance_particles_and_handle_walls(std::vector<OldState>& old_states) {
    old_states.resize(particles_.size());

    long long accepted_lr = 0;
    long long accepted_rl = 0;
    long long rejected_lr = 0;
    long long rejected_rl = 0;

#if defined(MAXWELL_DEMON_USE_OPENMP)
#pragma omp parallel for if(particles_.size() > 512) reduction(+ : accepted_lr, accepted_rl, rejected_lr, rejected_rl) schedule(static)
#endif
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(particles_.size()); ++i) {
        auto& p = particles_[static_cast<std::size_t>(i)];
        old_states[static_cast<std::size_t>(i)] = {p.x, p.y, p.vx, p.vy, p.chamber};

        p.x += p.vx * cfg_.dt;
        p.y += p.vy * cfg_.dt;

        enforce_outer_bounds(p);

        const auto& old = old_states[static_cast<std::size_t>(i)];
        const double old_x = old.x;
        const double new_x = p.x;
        const bool crossed = (old_x - cfg_.middle_wall_x) * (new_x - cfg_.middle_wall_x) <= 0.0 &&
                             std::abs(new_x - old_x) > kEpsilon &&
                             ((old.chamber == 0 && new_x >= cfg_.middle_wall_x) ||
                              (old.chamber == 1 && new_x <= cfg_.middle_wall_x));

        if (crossed) {
            const double alpha = (cfg_.middle_wall_x - old_x) / (new_x - old_x);
            const double y_cross = old.y + alpha * (p.y - old.y);
            const bool opening = is_in_opening(y_cross, p.radius);
            const bool allow = opening && demon_.allow_passage(old.vx, old.chamber);
            if (allow) {
                if (old.chamber == 0) {
                    ++accepted_lr;
                    p.chamber = 1;
                } else {
                    ++accepted_rl;
                    p.chamber = 0;
                }
            } else {
                if (old.chamber == 0) {
                    ++rejected_lr;
                    p.x = 2.0 * (cfg_.middle_wall_x - p.radius) - p.x;
                } else {
                    ++rejected_rl;
                    p.x = 2.0 * (cfg_.middle_wall_x + p.radius) - p.x;
                }
                p.vx = -p.vx;
                p.chamber = old.chamber;
            }
        }

        enforce_middle_wall_consistency(p);
        enforce_outer_bounds(p);
    }

    gate_stats_.accepted_lr += accepted_lr;
    gate_stats_.accepted_rl += accepted_rl;
    gate_stats_.rejected_lr += rejected_lr;
    gate_stats_.rejected_rl += rejected_rl;
}

void Simulation::resolve_particle_collision(Particle& a, Particle& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double min_dist = a.radius + b.radius;
    const double dist2 = dx * dx + dy * dy;
    if (dist2 >= min_dist * min_dist) return;
    if (wall_blocks_line_of_sight(a, b)) return;

    double dist = std::sqrt(std::max(dist2, kEpsilon));
    double nx = dx / dist;
    double ny = dy / dist;
    if (dist < kEpsilon) {
        nx = 1.0;
        ny = 0.0;
        dist = min_dist;
    }

    const double overlap = min_dist - dist;
    if (overlap > 0.0) {
        const double inv_ma = 1.0 / a.mass;
        const double inv_mb = 1.0 / b.mass;
        const double denom = inv_ma + inv_mb;
        const double correction = overlap + 1e-6;
        a.x -= correction * (inv_ma / denom) * nx;
        a.y -= correction * (inv_ma / denom) * ny;
        b.x += correction * (inv_mb / denom) * nx;
        b.y += correction * (inv_mb / denom) * ny;
    }

    const double dvx = b.vx - a.vx;
    const double dvy = b.vy - a.vy;
    const double rel_normal = dvx * nx + dvy * ny;
    if (rel_normal >= 0.0) return;

    const double impulse = -(1.0 + cfg_.restitution) * rel_normal / (1.0 / a.mass + 1.0 / b.mass);
    a.vx -= (impulse / a.mass) * nx;
    a.vy -= (impulse / a.mass) * ny;
    b.vx += (impulse / b.mass) * nx;
    b.vy += (impulse / b.mass) * ny;
}

void Simulation::resolve_particle_collisions() {
    const double cell_size = std::max(cfg_.max_collision_diameter() * 1.05, cfg_.max_collision_diameter() + 1e-6);
    const int nx = std::max(1, static_cast<int>(std::ceil(cfg_.lx / cell_size)));
    const int ny = std::max(1, static_cast<int>(std::ceil(cfg_.ly / cell_size)));

    auto cell_index = [ny](int cx, int cy) { return cx * ny + cy; };

    for (int iteration = 0; iteration < cfg_.collision_iterations; ++iteration) {
        std::vector<std::vector<int>> cells(static_cast<std::size_t>(nx * ny));
        for (std::size_t i = 0; i < particles_.size(); ++i) {
            const int cx = std::clamp(static_cast<int>(particles_[i].x / cell_size), 0, nx - 1);
            const int cy = std::clamp(static_cast<int>(particles_[i].y / cell_size), 0, ny - 1);
            cells[static_cast<std::size_t>(cell_index(cx, cy))].push_back(static_cast<int>(i));
        }

        for (int cx = 0; cx < nx; ++cx) {
            for (int cy = 0; cy < ny; ++cy) {
                auto& current = cells[static_cast<std::size_t>(cell_index(cx, cy))];
                for (std::size_t a = 0; a < current.size(); ++a) {
                    for (std::size_t b = a + 1; b < current.size(); ++b) {
                        resolve_particle_collision(particles_[static_cast<std::size_t>(current[a])],
                                                   particles_[static_cast<std::size_t>(current[b])]);
                    }
                }
                for (int dx = 0; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        if (dx == 0 && dy <= 0) continue;
                        const int nx_cell = cx + dx;
                        const int ny_cell = cy + dy;
                        if (nx_cell < 0 || nx_cell >= nx || ny_cell < 0 || ny_cell >= ny) continue;
                        auto& other = cells[static_cast<std::size_t>(cell_index(nx_cell, ny_cell))];
                        for (int ia : current) {
                            for (int ib : other) {
                                resolve_particle_collision(particles_[static_cast<std::size_t>(ia)],
                                                           particles_[static_cast<std::size_t>(ib)]);
                            }
                        }
                    }
                }
            }
        }

#if defined(MAXWELL_DEMON_USE_OPENMP)
#pragma omp parallel for if(particles_.size() > 512) schedule(static)
#endif
        for (std::int64_t i = 0; i < static_cast<std::int64_t>(particles_.size()); ++i) {
            enforce_outer_bounds(particles_[static_cast<std::size_t>(i)]);
            enforce_middle_wall_consistency(particles_[static_cast<std::size_t>(i)]);
        }
    }
}

ObservableRow Simulation::measure(int step, double time) const {
    ObservableRow row;
    row.step = step;
    row.time = time;
    row.gate_stats = gate_stats_;
    row.energy_injected_total = energy_injected_total_;
    row.energy_dissipated_total = energy_dissipated_total_;

    double mass_left = 0.0;
    double mass_right = 0.0;
    double momentum_left_x = 0.0;
    double momentum_left_y = 0.0;
    double momentum_right_x = 0.0;
    double momentum_right_y = 0.0;
    double speed_left = 0.0;
    double speed_right = 0.0;
    int n_left = 0;
    int n_right = 0;

#if defined(MAXWELL_DEMON_USE_OPENMP)
#pragma omp parallel for if(particles_.size() > 512) reduction(+ : mass_left, mass_right, momentum_left_x, momentum_left_y, momentum_right_x, momentum_right_y, speed_left, speed_right, n_left, n_right) schedule(static)
#endif
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(particles_.size()); ++i) {
        const auto& p = particles_[static_cast<std::size_t>(i)];
        const double speed = std::sqrt(p.vx * p.vx + p.vy * p.vy);
        if (p.x < cfg_.middle_wall_x) {
            ++n_left;
            mass_left += p.mass;
            momentum_left_x += p.mass * p.vx;
            momentum_left_y += p.mass * p.vy;
            speed_left += speed;
        } else {
            ++n_right;
            mass_right += p.mass;
            momentum_right_x += p.mass * p.vx;
            momentum_right_y += p.mass * p.vy;
            speed_right += speed;
        }
    }

    const double ux_left = mass_left > 0.0 ? momentum_left_x / mass_left : 0.0;
    const double uy_left = mass_left > 0.0 ? momentum_left_y / mass_left : 0.0;
    const double ux_right = mass_right > 0.0 ? momentum_right_x / mass_right : 0.0;
    const double uy_right = mass_right > 0.0 ? momentum_right_y / mass_right : 0.0;

    double thermal_ke_left = 0.0;
    double thermal_ke_right = 0.0;
    double total_ke_left = 0.0;
    double total_ke_right = 0.0;

#if defined(MAXWELL_DEMON_USE_OPENMP)
#pragma omp parallel for if(particles_.size() > 512) reduction(+ : thermal_ke_left, thermal_ke_right, total_ke_left, total_ke_right) schedule(static)
#endif
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(particles_.size()); ++i) {
        const auto& p = particles_[static_cast<std::size_t>(i)];
        const double speed2 = p.vx * p.vx + p.vy * p.vy;
        const double total_ke = 0.5 * p.mass * speed2;
        if (p.x < cfg_.middle_wall_x) {
            const double dvx = p.vx - ux_left;
            const double dvy = p.vy - uy_left;
            thermal_ke_left += 0.5 * p.mass * (dvx * dvx + dvy * dvy);
            total_ke_left += total_ke;
        } else {
            const double dvx = p.vx - ux_right;
            const double dvy = p.vy - uy_right;
            thermal_ke_right += 0.5 * p.mass * (dvx * dvx + dvy * dvy);
            total_ke_right += total_ke;
        }
    }

    const double area_left = cfg_.middle_wall_x * cfg_.ly;
    const double area_right = (cfg_.lx - cfg_.middle_wall_x) * cfg_.ly;
    const double total_n = static_cast<double>(std::max(1, cfg_.num_particles));

    row.n_left = n_left;
    row.n_right = n_right;
    row.density_left = n_left / std::max(area_left, kEpsilon);
    row.density_right = n_right / std::max(area_right, kEpsilon);
    row.temperature_left = (n_left > 0) ? (thermal_ke_left / static_cast<double>(n_left)) : 0.0;
    row.temperature_right = (n_right > 0) ? (thermal_ke_right / static_cast<double>(n_right)) : 0.0;
    const double t_mean = std::max(0.5 * (std::abs(row.temperature_left) + std::abs(row.temperature_right)), kEpsilon);
    row.temperature_contrast = std::abs(row.temperature_right - row.temperature_left) / t_mean;
    row.population_imbalance = std::abs(static_cast<double>(n_right - n_left)) / total_n;
    row.separation_score = 0.5 * row.temperature_contrast + 0.5 * row.population_imbalance;
    row.mean_speed_left = (n_left > 0) ? (speed_left / static_cast<double>(n_left)) : 0.0;
    row.mean_speed_right = (n_right > 0) ? (speed_right / static_cast<double>(n_right)) : 0.0;
    row.kinetic_energy_left = total_ke_left;
    row.kinetic_energy_right = total_ke_right;
    row.kinetic_energy_total = total_ke_left + total_ke_right;
    return row;
}

void Simulation::apply_equilibrium_status(ObservableRow& row, const EquilibriumStatus& status) {
    row.temp_error = status.temp_error;
    row.density_error = status.density_error;
    row.flux_error = status.flux_error;
    row.slope_error = status.slope_error;
    row.equilibrium_quality = status.equilibrium_quality;
    row.thermal_equilibrium_candidate = status.thermal_candidate ? 1 : 0;
    row.thermal_equilibrium_reached = status.thermal_equilibrium_reached ? 1 : 0;
    row.steady_state_reached = status.steady_state_reached ? 1 : 0;
    row.separated_steady_state_reached = status.separated_steady_state_reached ? 1 : 0;
}

void Simulation::run() {
    std::vector<OldState> old_states;
    bool wrote_thermal_snapshot = false;
    bool wrote_steady_snapshot = false;
    bool wrote_separated_snapshot = false;

    if (cfg_.snapshot_interval > 0) writers_.write_snapshot(0, 0.0, particles_);
    ObservableRow final_row = measure(0, 0.0);
    apply_equilibrium_status(final_row, equilibrium_detector_.observe(final_row));
    writers_.write_observable(final_row);

    int final_step = 0;
    double final_time = 0.0;

    for (int step = 1; step <= cfg_.steps; ++step) {
        apply_forces_and_energy_accounting();
        advance_particles_and_handle_walls(old_states);
        resolve_particle_collisions();

        const double time = static_cast<double>(step) * cfg_.dt;
        final_step = step;
        final_time = time;

        if (cfg_.snapshot_interval > 0 && step % cfg_.snapshot_interval == 0) {
            writers_.write_snapshot(step, time, particles_);
        }
        if (step % cfg_.observable_interval == 0) {
            final_row = measure(step, time);
            const auto status = equilibrium_detector_.observe(final_row);
            apply_equilibrium_status(final_row, status);
            writers_.write_observable(final_row);

            if (cfg_.live_log_interval > 0 && step % cfg_.live_log_interval == 0) {
                std::cout << "[live] step=" << step
                          << " time=" << time
                          << " T_left=" << final_row.temperature_left
                          << " T_right=" << final_row.temperature_right
                          << " temp_contrast=" << final_row.temperature_contrast
                          << " N_left=" << final_row.n_left
                          << " N_right=" << final_row.n_right
                          << " separation_score=" << final_row.separation_score
                          << " thermal_eq=" << (status.thermal_equilibrium_reached ? "yes" : "no")
                          << " separated_ss=" << (status.separated_steady_state_reached ? "yes" : "no")
                          << std::endl;
            }

            if (status.thermal_equilibrium_reached && !wrote_thermal_snapshot) {
                writers_.write_named_snapshot("thermal_equilibrium_snapshot.csv", step, time, particles_);
                wrote_thermal_snapshot = true;
            }
            if (status.steady_state_reached && !wrote_steady_snapshot) {
                writers_.write_named_snapshot("steady_state_snapshot.csv", step, time, particles_);
                wrote_steady_snapshot = true;
            }
            if (status.separated_steady_state_reached && !wrote_separated_snapshot) {
                writers_.write_named_snapshot("separated_steady_state_snapshot.csv", step, time, particles_);
                wrote_separated_snapshot = true;
            }
            if (cfg_.stop_at_thermal_equilibrium && status.thermal_equilibrium_reached) break;
            if (cfg_.stop_at_steady_state && status.steady_state_reached) break;
        }
    }

    final_row = measure(final_step, final_time);
    apply_equilibrium_status(final_row, equilibrium_detector_.observe(final_row));
    writers_.write_named_snapshot("final_snapshot.csv", final_step, final_time, particles_);
    writers_.write_summary(cfg_, final_row, equilibrium_detector_.summary());

    const auto summary = equilibrium_detector_.summary();
    std::cout << "Simulation finished. Output written to: " << writers_.output_dir() << '\n';
    std::cout << "Final step=" << final_step << ", time=" << final_time << '\n';
    std::cout << "Thermal equilibrium reached: " << (summary.thermal_equilibrium_reached ? "yes" : "no") << '\n';
    std::cout << "Steady state reached: " << (summary.steady_state_reached ? "yes" : "no") << '\n';
    std::cout << "Separated steady state reached: " << (summary.separated_steady_state_reached ? "yes" : "no") << '\n';
    std::cout << "Gate stats: accepted LR=" << gate_stats_.accepted_lr
              << ", accepted RL=" << gate_stats_.accepted_rl
              << ", rejected LR=" << gate_stats_.rejected_lr
              << ", rejected RL=" << gate_stats_.rejected_rl << '\n';
}
