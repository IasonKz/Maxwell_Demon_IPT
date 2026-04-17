#pragma once

#include <string>

class Demon {
public:
    Demon(double threshold, std::string mode);
    bool allow_passage(double vx, int from_chamber) const;
    const std::string& mode() const noexcept;
    double threshold() const noexcept;

private:
    double threshold_;
    std::string mode_;
};
