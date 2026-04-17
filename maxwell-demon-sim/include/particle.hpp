#pragma once

struct Particle {
    int id = 0;
    double x = 0.0;
    double y = 0.0;
    double vx = 0.0;
    double vy = 0.0;
    int chamber = 0; // 0 = left, 1 = right
};
