#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace {

std::string trim(const std::string& input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, last - first + 1);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool to_bool(const std::string& value) {
    const auto v = lower(trim(value));
    if (v == "1" || v == "true" || v == "yes" || v == "on") {
        return true;
    }
    if (v == "0" || v == "false" || v == "no" || v == "off") {
        return false;
    }
    throw std::runtime_error("Could not parse boolean value: " + value);
}

std::vector<double> parse_double_list(const std::string& value) {
    std::vector<double> numbers;
    std::stringstream ss(value);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (!token.empty()) {
            numbers.push_back(std::stod(token));
        }
    }
    return numbers;
}

} // namespace

Config Config::from_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Could not open config file: " + path);
    }

    Config cfg;
    std::string line;
    int line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("Invalid config line " + std::to_string(line_number) + ": " + line);
        }

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        if (key == "num_particles") cfg.num_particles = std::stoi(value);
        else if (key == "lx") cfg.lx = std::stod(value);
        else if (key == "ly") cfg.ly = std::stod(value);
        else if (key == "radius") cfg.radius = std::stod(value);
        else if (key == "mass") cfg.mass = std::stod(value);
        else if (key == "dt") cfg.dt = std::stod(value);
        else if (key == "steps") cfg.steps = std::stoi(value);
        else if (key == "snapshot_interval") cfg.snapshot_interval = std::stoi(value);
        else if (key == "observable_interval") cfg.observable_interval = std::stoi(value);
        else if (key == "collision_iterations") cfg.collision_iterations = std::stoi(value);
        else if (key == "seed") cfg.seed = std::stoi(value);
        else if (key == "velocity_std") cfg.velocity_std = std::stod(value);
        else if (key == "middle_wall_x") cfg.middle_wall_x = std::stod(value);
        else if (key == "opening_centers") cfg.opening_centers = parse_double_list(value);
        else if (key == "opening_height") cfg.opening_height = std::stod(value);
        else if (key == "demon_threshold") cfg.demon_threshold = std::stod(value);
        else if (key == "demon_mode") cfg.demon_mode = value;
        else if (key == "output_dir") cfg.output_dir = value;
        else if (key == "overwrite_output") cfg.overwrite_output = to_bool(value);
        else {
            throw std::runtime_error("Unknown config key on line " + std::to_string(line_number) + ": " + key);
        }
    }

    if (cfg.middle_wall_x <= 0.0) {
        cfg.middle_wall_x = 0.5 * cfg.lx;
    }

    cfg.validate();
    return cfg;
}

void Config::validate() const {
    if (num_particles <= 0) throw std::runtime_error("num_particles must be positive.");
    if (lx <= 2.0 * radius || ly <= 2.0 * radius) throw std::runtime_error("Box too small for chosen radius.");
    if (radius <= 0.0) throw std::runtime_error("radius must be positive.");
    if (mass <= 0.0) throw std::runtime_error("mass must be positive.");
    if (dt <= 0.0) throw std::runtime_error("dt must be positive.");
    if (steps <= 0) throw std::runtime_error("steps must be positive.");
    if (snapshot_interval <= 0) throw std::runtime_error("snapshot_interval must be positive.");
    if (observable_interval <= 0) throw std::runtime_error("observable_interval must be positive.");
    if (collision_iterations <= 0) throw std::runtime_error("collision_iterations must be positive.");
    if (velocity_std < 0.0) throw std::runtime_error("velocity_std must be non-negative.");
    if (middle_wall_x <= radius || middle_wall_x >= lx - radius) {
        throw std::runtime_error("middle_wall_x must lie strictly inside the box.");
    }
    if (opening_centers.empty()) throw std::runtime_error("At least one opening center is required.");
    if (opening_height <= 2.0 * radius) {
        throw std::runtime_error("opening_height must be larger than 2*radius so a disk can pass.");
    }
    for (const double center : opening_centers) {
        if (center - 0.5 * opening_height < radius || center + 0.5 * opening_height > ly - radius) {
            throw std::runtime_error("An opening falls outside the box or too close to the boundary.");
        }
    }
}
