#pragma once

#include <random>
#include <string>
#include <vector>

#include "config.hpp"
#include "demon.hpp"
#include "equilibrium.hpp"
#include "io.hpp"
#include "particle.hpp"

class Simulation {
public:
    explicit Simulation(Config cfg);
    void run();

private:
    struct OldState {
        double x = 0.0;
        double y = 0.0;
        double vx = 0.0;
        double vy = 0.0;
        int chamber = 0;
    };

    Config cfg_;
    Demon demon_;
    EquilibriumDetector equilibrium_detector_;
    std::mt19937 rng_;
    std::vector<Particle> particles_;
    CsvWriters writers_;
    GateStats gate_stats_{};
    double energy_injected_total_ = 0.0;
    double energy_dissipated_total_ = 0.0;

    void initialize_particles();
    bool is_in_opening(double y, double particle_radius) const;
    bool wall_blocks_line_of_sight(const Particle& a, const Particle& b) const;
    bool overlaps_any_existing(const Particle& candidate) const;
    void apply_forces_and_energy_accounting();
    void advance_particles_and_handle_walls(std::vector<OldState>& old_states);
    void resolve_particle_collisions();
    void resolve_particle_collision(Particle& a, Particle& b);
    void enforce_outer_bounds(Particle& p) const;
    void enforce_middle_wall_consistency(Particle& p) const;
    ObservableRow measure(int step, double time) const;
    static void apply_equilibrium_status(ObservableRow& row, const EquilibriumStatus& status);
};
