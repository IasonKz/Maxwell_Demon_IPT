#pragma once

struct Particle {
    int id = 0;
    int species_id = 0;
    double x = 0.0;
    double y = 0.0;
    double vx = 0.0;
    double vy = 0.0;
    double radius = 0.0;
    double mass = 1.0;
    double friction_gamma = 0.0;
    double drive_strength = 0.0;
    int chamber = 0; // 0 = left, 1 = right
};
