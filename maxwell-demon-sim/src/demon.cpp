#include "demon.hpp"

#include <cmath>
#include <stdexcept>

Demon::Demon(double threshold, std::string mode)
    : threshold_(threshold), mode_(std::move(mode)) {}

bool Demon::allow_passage(double vx, int from_chamber) const {
    if (mode_ == "always_open") {
        return true;
    }

    if (mode_ == "fast_both") {
        if (from_chamber == 0) {
            return vx > threshold_;
        }
        return -vx > threshold_;
    }

    if (mode_ == "hot_right_cold_left") {
        if (from_chamber == 0) {
            return vx > threshold_;
        }
        return (-vx) < threshold_;
    }

    if (mode_ == "slow_both") {
        if (from_chamber == 0) {
            return vx > 0.0 && std::abs(vx) < threshold_;
        }
        return vx < 0.0 && std::abs(vx) < threshold_;
    }

    throw std::runtime_error("Unknown demon mode: " + mode_);
}

const std::string& Demon::mode() const noexcept { return mode_; }

double Demon::threshold() const noexcept { return threshold_; }
