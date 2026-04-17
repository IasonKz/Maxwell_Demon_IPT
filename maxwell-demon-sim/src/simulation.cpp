#include "simulation.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

#if defined(MAXWELL_DEMON_USE_OPENMP)
#include <omp.h>
#endif

namespace {

constexpr double kEpsilon = 1e-9;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

double squared_distance(const Particle& a, const Particle& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    return dx * dx + dy * dy;
}

} // namespace

Simulation::Simulation(Config cfg)
    : cfg_(std::move(cfg)),
      demon_(cfg_.demon_threshold, cfg_.demon_mode),
      rng_(static_cast<std::mt19937::result_type>(cfg_.seed)),
      writers_(cfg_) {
    initialize_particles();
    writers_.write_metadata(cfg_);
}

bool Simulation::is_in_opening(double y) const {
    const double margin = cfg_.radius;
    for (const double center : cfg_.opening_centers) {
        const double low = center - 0.5 * cfg_.opening_height + margin;
        const double high = center + 0.5 * cfg_.opening_height - margin;
        if (y >= low && y <= high) {
            return true;
        }
    }
    return false;
}

bool Simulation::wall_blocks_line_of_sight(const Particle& a, const Particle& b) const {
    const bool opposite_sides = (a.x < cfg_.middle_wall_x && b.x > cfg_.middle_wall_x) ||
                                (a.x > cfg_.middle_wall_x && b.x < cfg_.middle_wall_x);
    if (!opposite_sides) {
        return false;
    }

    const double dx = b.x - a.x;
    if (std::abs(dx) < kEpsilon) {
        return true;
    }

    const double alpha = (cfg_.middle_wall_x - a.x) / dx;
    if (alpha < 0.0 || alpha > 1.0) {
        return false;
    }
    const double y_cross = a.y + alpha * (b.y - a.y);
    return !is_in_opening(y_cross);
}

bool Simulation::overlaps_any_existing(const Particle& candidate) const {
    const double min_dist2 = std::pow(2.0 * cfg_.radius, 2);
    for (const auto& existing : particles_) {
        if (wall_blocks_line_of_sight(candidate, existing)) {
            continue;
        }
        if (squared_distance(candidate, existing) < min_dist2) {
            return true;
        }
    }
    return false;
}

void Simulation::initialize_particles() {
    std::uniform_real_distribution<double> x_dist(cfg_.radius, cfg_.lx - cfg_.radius);
    std::uniform_real_distribution<double> y_dist(cfg_.radius, cfg_.ly - cfg_.radius);
    std::normal_distribution<double> velocity_dist(0.0, cfg_.velocity_std);

    particles_.clear();
    particles_.reserve(static_cast<std::size_t>(cfg_.num_particles));

    const int max_attempts = 500000;
    for (int id = 0; id < cfg_.num_particles; ++id) {
        bool placed = false;
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            Particle p;
            p.id = id;
            p.x = x_dist(rng_);
            p.y = y_dist(rng_);
            p.vx = velocity_dist(rng_);
            p.vy = velocity_dist(rng_);
            p.chamber = (p.x < cfg_.middle_wall_x) ? 0 : 1;
            if (!overlaps_any_existing(p)) {
                particles_.push_back(p);
                placed = true;
                break;
            }
        }
        if (!placed) {
            throw std::runtime_error("Could not place all particles without overlap. Reduce num_particles or radius.");
        }
    }

    double mean_vx = 0.0;
    double mean_vy = 0.0;
    for (const auto& p : particles_) {
        mean_vx += p.vx;
        mean_vy += p.vy;
    }
    mean_vx /= static_cast<double>(particles_.size());
    mean_vy /= static_cast<double>(particles_.size());
    for (auto& p : particles_) {
        p.vx -= mean_vx;
        p.vy -= mean_vy;
    }
}

void Simulation::enforce_outer_bounds(Particle& p) const {
    if (p.x < cfg_.radius) {
        p.x = 2.0 * cfg_.radius - p.x;
        p.vx = -p.vx;
    }
    if (p.x > cfg_.lx - cfg_.radius) {
        p.x = 2.0 * (cfg_.lx - cfg_.radius) - p.x;
        p.vx = -p.vx;
    }
    if (p.y < cfg_.radius) {
        p.y = 2.0 * cfg_.radius - p.y;
        p.vy = -p.vy;
    }
    if (p.y > cfg_.ly - cfg_.radius) {
        p.y = 2.0 * (cfg_.ly - cfg_.radius) - p.y;
        p.vy = -p.vy;
    }
}

void Simulation::enforce_middle_wall_consistency(Particle& p) const {
    if (is_in_opening(p.y)) {
        return;
    }
    if (p.chamber == 0 && p.x > cfg_.middle_wall_x - kEpsilon) {
        p.x = cfg_.middle_wall_x - kEpsilon;
        if (p.vx > 0.0) {
            p.vx = -p.vx;
        }
    }
    if (p.chamber == 1 && p.x < cfg_.middle_wall_x + kEpsilon) {
        p.x = cfg_.middle_wall_x + kEpsilon;
        if (p.vx < 0.0) {
            p.vx = -p.vx;
        }
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

        const double old_x = old_states[static_cast<std::size_t>(i)].x;
        const double new_x = p.x;
        const bool crossed = (old_x - cfg_.middle_wall_x) * (new_x - cfg_.middle_wall_x) <= 0.0 &&
                             std::abs(new_x - old_x) > kEpsilon &&
                             ((old_states[static_cast<std::size_t>(i)].chamber == 0 && new_x >= cfg_.middle_wall_x) ||
                              (old_states[static_cast<std::size_t>(i)].chamber == 1 && new_x <= cfg_.middle_wall_x));

        if (crossed) {
            const double alpha = (cfg_.middle_wall_x - old_x) / (new_x - old_x);
            const double y_cross = old_states[static_cast<std::size_t>(i)].y + alpha * (p.y - old_states[static_cast<std::size_t>(i)].y);
            const bool opening = is_in_opening(y_cross);
            const bool allow = opening && demon_.allow_passage(old_states[static_cast<std::size_t>(i)].vx,
                                                               old_states[static_cast<std::size_t>(i)].chamber);
            if (allow) {
                if (old_states[static_cast<std::size_t>(i)].chamber == 0) {
                    ++accepted_lr;
                    p.chamber = 1;
                } else {
                    ++accepted_rl;
                    p.chamber = 0;
                }
            } else {
                if (old_states[static_cast<std::size_t>(i)].chamber == 0) {
                    ++rejected_lr;
                } else {
                    ++rejected_rl;
                }
                p.x = 2.0 * cfg_.middle_wall_x - p.x;
                p.vx = -p.vx;
                p.chamber = old_states[static_cast<std::size_t>(i)].chamber;
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
    const double min_dist = 2.0 * cfg_.radius;
    const double dist2 = dx * dx + dy * dy;
    if (dist2 >= min_dist * min_dist) {
        return;
    }

    if (wall_blocks_line_of_sight(a, b)) {
        return;
    }

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
        const double correction = 0.5 * (overlap + 1e-6);
        a.x -= correction * nx;
        a.y -= correction * ny;
        b.x += correction * nx;
        b.y += correction * ny;
    }

    const double dvx = b.vx - a.vx;
    const double dvy = b.vy - a.vy;
    const double rel_normal = dvx * nx + dvy * ny;
    if (rel_normal >= 0.0) {
        return;
    }

    a.vx += rel_normal * nx;
    a.vy += rel_normal * ny;
    b.vx -= rel_normal * nx;
    b.vy -= rel_normal * ny;
}

void Simulation::resolve_particle_collisions() {
    const double cell_size = std::max(2.0 * cfg_.radius * 1.05, 2.0 * cfg_.radius + 1e-6);
    const int nx = std::max(1, static_cast<int>(std::ceil(cfg_.lx / cell_size)));
    const int ny = std::max(1, static_cast<int>(std::ceil(cfg_.ly / cell_size)));

    auto cell_index = [ny](int cx, int cy) {
        return cx * ny + cy;
    };

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
                        if (dx == 0 && dy <= 0) {
                            continue;
                        }
                        const int nx_cell = cx + dx;
                        const int ny_cell = cy + dy;
                        if (nx_cell < 0 || nx_cell >= nx || ny_cell < 0 || ny_cell >= ny) {
                            continue;
                        }
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

    double ke_left = 0.0;
    double ke_right = 0.0;
    double speed_left = 0.0;
    double speed_right = 0.0;
    int n_left = 0;
    int n_right = 0;

#if defined(MAXWELL_DEMON_USE_OPENMP)
#pragma omp parallel for if(particles_.size() > 512) reduction(+ : ke_left, ke_right, speed_left, speed_right, n_left, n_right) schedule(static)
#endif
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(particles_.size()); ++i) {
        const auto& p = particles_[static_cast<std::size_t>(i)];
        const double speed2 = p.vx * p.vx + p.vy * p.vy;
        const double speed = std::sqrt(speed2);
        const double ke = 0.5 * cfg_.mass * speed2;
        if (p.x < cfg_.middle_wall_x) {
            ++n_left;
            ke_left += ke;
            speed_left += speed;
        } else {
            ++n_right;
            ke_right += ke;
            speed_right += speed;
        }
    }

    row.n_left = n_left;
    row.n_right = n_right;
    row.temperature_left = (n_left > 0) ? (ke_left / static_cast<double>(n_left)) : 0.0;
    row.temperature_right = (n_right > 0) ? (ke_right / static_cast<double>(n_right)) : 0.0;
    row.mean_speed_left = (n_left > 0) ? (speed_left / static_cast<double>(n_left)) : 0.0;
    row.mean_speed_right = (n_right > 0) ? (speed_right / static_cast<double>(n_right)) : 0.0;

    return row;
}

void Simulation::run() {
    std::vector<OldState> old_states;
    writers_.write_snapshot(0, 0.0, particles_);
    writers_.write_observable(measure(0, 0.0));

    for (int step = 1; step <= cfg_.steps; ++step) {
        advance_particles_and_handle_walls(old_states);
        resolve_particle_collisions();
        const double time = static_cast<double>(step) * cfg_.dt;
        if (step % cfg_.snapshot_interval == 0) {
            writers_.write_snapshot(step, time, particles_);
        }
        if (step % cfg_.observable_interval == 0) {
            writers_.write_observable(measure(step, time));
        }
    }

    std::cout << "Simulation finished. Output written to: " << writers_.output_dir() << '\n';
    std::cout << "Gate stats: accepted LR=" << gate_stats_.accepted_lr
              << ", accepted RL=" << gate_stats_.accepted_rl
              << ", rejected LR=" << gate_stats_.rejected_lr
              << ", rejected RL=" << gate_stats_.rejected_rl << '\n';
}
