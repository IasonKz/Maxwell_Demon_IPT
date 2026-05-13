#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace {

std::string trim(const std::string& input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
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
    if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    throw std::runtime_error("Could not parse boolean value: " + value);
}

std::vector<double> parse_double_list(const std::string& value) {
    std::vector<double> numbers;
    std::stringstream ss(value);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (!token.empty()) numbers.push_back(std::stod(token));
    }
    return numbers;
}

bool is_species_key(const std::string& key) {
    static const std::regex pattern(R"(^species[0-9]+_(count|radius|mass|friction_gamma|drive_strength)$)");
    return std::regex_match(key, pattern);
}

int int_value(const std::unordered_map<std::string, std::string>& values, const std::string& key, int current) {
    const auto it = values.find(key);
    return it == values.end() ? current : std::stoi(it->second);
}

double double_value(const std::unordered_map<std::string, std::string>& values, const std::string& key, double current) {
    const auto it = values.find(key);
    return it == values.end() ? current : std::stod(it->second);
}

bool bool_value(const std::unordered_map<std::string, std::string>& values, const std::string& key, bool current) {
    const auto it = values.find(key);
    return it == values.end() ? current : to_bool(it->second);
}

std::string string_value(const std::unordered_map<std::string, std::string>& values, const std::string& key, std::string current) {
    const auto it = values.find(key);
    return it == values.end() ? current : it->second;
}

bool has_key(const std::unordered_map<std::string, std::string>& values, const std::string& key) {
    return values.find(key) != values.end();
}

} // namespace

Config Config::from_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Could not open config file: " + path);

    std::unordered_map<std::string, std::string> values;
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        line = trim(line);
        if (line.empty()) continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("Invalid config line " + std::to_string(line_number) + ": " + line);
        }
        values[trim(line.substr(0, equals))] = trim(line.substr(equals + 1));
    }

    static const std::vector<std::string> known_keys{
        "num_particles", "lx", "ly", "radius", "mass", "friction_gamma", "drive_strength", "restitution",
        "dt", "steps", "snapshot_interval", "observable_interval", "live_log_interval", "collision_iterations", "seed",
        "velocity_std", "initial_left_fraction", "middle_wall_x", "opening_centers", "opening_height",
        "demon_threshold", "demon_mode", "output_dir", "overwrite_output", "species_count",
        "detect_thermal_equilibrium", "stop_at_thermal_equilibrium", "stop_at_steady_state",
        "equilibrium_window", "equilibrium_required_windows", "equilibrium_temp_tol",
        "equilibrium_density_tol", "equilibrium_flux_tol", "equilibrium_slope_tol",
        "separated_steady_state_min_score"};

    for (const auto& item : values) {
        const auto& key = item.first;
        const bool ordinary_key = std::find(known_keys.begin(), known_keys.end(), key) != known_keys.end();
        if (!ordinary_key && !is_species_key(key)) throw std::runtime_error("Unknown config key: " + key);
    }

    Config cfg;
    cfg.num_particles = int_value(values, "num_particles", cfg.num_particles);
    cfg.lx = double_value(values, "lx", cfg.lx);
    cfg.ly = double_value(values, "ly", cfg.ly);
    cfg.radius = double_value(values, "radius", cfg.radius);
    cfg.mass = double_value(values, "mass", cfg.mass);
    cfg.friction_gamma = double_value(values, "friction_gamma", cfg.friction_gamma);
    cfg.drive_strength = double_value(values, "drive_strength", cfg.drive_strength);
    cfg.restitution = double_value(values, "restitution", cfg.restitution);
    cfg.dt = double_value(values, "dt", cfg.dt);
    cfg.steps = int_value(values, "steps", cfg.steps);
    cfg.snapshot_interval = int_value(values, "snapshot_interval", cfg.snapshot_interval);
    cfg.observable_interval = int_value(values, "observable_interval", cfg.observable_interval);
    cfg.live_log_interval = int_value(values, "live_log_interval", cfg.live_log_interval);
    cfg.collision_iterations = int_value(values, "collision_iterations", cfg.collision_iterations);
    cfg.seed = int_value(values, "seed", cfg.seed);
    cfg.velocity_std = double_value(values, "velocity_std", cfg.velocity_std);
    cfg.initial_left_fraction = double_value(values, "initial_left_fraction", cfg.initial_left_fraction);
    cfg.middle_wall_x = double_value(values, "middle_wall_x", cfg.middle_wall_x);
    if (has_key(values, "opening_centers")) cfg.opening_centers = parse_double_list(values.at("opening_centers"));
    cfg.opening_height = double_value(values, "opening_height", cfg.opening_height);
    cfg.demon_threshold = double_value(values, "demon_threshold", cfg.demon_threshold);
    cfg.demon_mode = string_value(values, "demon_mode", cfg.demon_mode);
    cfg.output_dir = string_value(values, "output_dir", cfg.output_dir);
    cfg.overwrite_output = bool_value(values, "overwrite_output", cfg.overwrite_output);

    cfg.detect_thermal_equilibrium = bool_value(values, "detect_thermal_equilibrium", cfg.detect_thermal_equilibrium);
    cfg.stop_at_thermal_equilibrium = bool_value(values, "stop_at_thermal_equilibrium", cfg.stop_at_thermal_equilibrium);
    cfg.stop_at_steady_state = bool_value(values, "stop_at_steady_state", cfg.stop_at_steady_state);
    cfg.equilibrium_window = int_value(values, "equilibrium_window", cfg.equilibrium_window);
    cfg.equilibrium_required_windows = int_value(values, "equilibrium_required_windows", cfg.equilibrium_required_windows);
    cfg.equilibrium_temp_tol = double_value(values, "equilibrium_temp_tol", cfg.equilibrium_temp_tol);
    cfg.equilibrium_density_tol = double_value(values, "equilibrium_density_tol", cfg.equilibrium_density_tol);
    cfg.equilibrium_flux_tol = double_value(values, "equilibrium_flux_tol", cfg.equilibrium_flux_tol);
    cfg.equilibrium_slope_tol = double_value(values, "equilibrium_slope_tol", cfg.equilibrium_slope_tol);
    cfg.separated_steady_state_min_score = double_value(values, "separated_steady_state_min_score", cfg.separated_steady_state_min_score);

    const int species_count = int_value(values, "species_count", 0);
    cfg.species.clear();
    if (species_count > 0) {
        cfg.species.reserve(static_cast<std::size_t>(species_count));
        int total = 0;
        for (int id = 0; id < species_count; ++id) {
            const std::string prefix = "species" + std::to_string(id) + "_";
            SpeciesConfig sp;
            sp.id = id;
            sp.count = int_value(values, prefix + "count", -1);
            sp.radius = double_value(values, prefix + "radius", cfg.radius);
            sp.mass = double_value(values, prefix + "mass", cfg.mass);
            sp.friction_gamma = double_value(values, prefix + "friction_gamma", cfg.friction_gamma);
            sp.drive_strength = double_value(values, prefix + "drive_strength", cfg.drive_strength);
            if (sp.count < 0) throw std::runtime_error(prefix + "count is required when species_count is set.");
            total += sp.count;
            cfg.species.push_back(sp);
        }
        cfg.num_particles = total;
        if (!cfg.species.empty()) {
            cfg.radius = cfg.species.front().radius;
            cfg.mass = cfg.species.front().mass;
        }
    } else {
        SpeciesConfig sp;
        sp.id = 0;
        sp.count = cfg.num_particles;
        sp.radius = cfg.radius;
        sp.mass = cfg.mass;
        sp.friction_gamma = cfg.friction_gamma;
        sp.drive_strength = cfg.drive_strength;
        cfg.species.push_back(sp);
    }

    if (cfg.middle_wall_x <= 0.0) cfg.middle_wall_x = 0.5 * cfg.lx;
    cfg.validate();
    return cfg;
}

double Config::max_radius() const {
    double result = radius;
    for (const auto& sp : species) result = std::max(result, sp.radius);
    return result;
}

double Config::max_collision_diameter() const {
    double first = 0.0;
    double second = 0.0;
    for (const auto& sp : species) {
        if (sp.radius >= first) {
            second = first;
            first = sp.radius;
        } else if (sp.radius > second) {
            second = sp.radius;
        }
    }
    if (second <= 0.0) second = first;
    return first + second;
}

void Config::validate() const {
    if (species.empty()) throw std::runtime_error("At least one species is required.");
    if (num_particles <= 0) throw std::runtime_error("num_particles must be positive.");
    if (dt <= 0.0) throw std::runtime_error("dt must be positive.");
    if (steps <= 0) throw std::runtime_error("steps must be positive.");
    if (snapshot_interval < 0) throw std::runtime_error("snapshot_interval must be non-negative. Use 0 to disable snapshots.");
    if (observable_interval <= 0) throw std::runtime_error("observable_interval must be positive.");
    if (live_log_interval < 0) throw std::runtime_error("live_log_interval must be non-negative. Use 0 to disable live console telemetry.");
    if (collision_iterations <= 0) throw std::runtime_error("collision_iterations must be positive.");
    if (velocity_std < 0.0) throw std::runtime_error("velocity_std must be non-negative.");
    if (friction_gamma < 0.0) throw std::runtime_error("friction_gamma must be non-negative.");
    if (drive_strength < 0.0) throw std::runtime_error("drive_strength must be non-negative.");
    if (restitution < 0.0 || restitution > 1.0) throw std::runtime_error("restitution must be in [0, 1].");
    if (initial_left_fraction < 0.0 || initial_left_fraction > 1.0) {
        throw std::runtime_error("initial_left_fraction must be in [0, 1].");
    }

    int total = 0;
    for (const auto& sp : species) {
        if (sp.count <= 0) throw std::runtime_error("All species counts must be positive.");
        if (sp.radius <= 0.0) throw std::runtime_error("All species radii must be positive.");
        if (sp.mass <= 0.0) throw std::runtime_error("All species masses must be positive.");
        if (sp.friction_gamma < 0.0) throw std::runtime_error("Species friction_gamma must be non-negative.");
        if (sp.drive_strength < 0.0) throw std::runtime_error("Species drive_strength must be non-negative.");
        total += sp.count;
    }
    if (total != num_particles) throw std::runtime_error("Internal error: sum(species counts) does not match num_particles.");

    const double rmax = max_radius();
    if (lx <= 2.0 * rmax || ly <= 2.0 * rmax) throw std::runtime_error("Box too small for chosen particle radii.");
    if (middle_wall_x <= rmax || middle_wall_x >= lx - rmax) {
        throw std::runtime_error("middle_wall_x must lie strictly inside the box with enough clearance for the largest particle.");
    }
    if (opening_centers.empty()) throw std::runtime_error("At least one opening center is required.");
    if (opening_height <= 2.0 * rmax) throw std::runtime_error("opening_height must be larger than 2*max_radius.");
    for (const double center : opening_centers) {
        if (center - 0.5 * opening_height < rmax || center + 0.5 * opening_height > ly - rmax) {
            throw std::runtime_error("An opening falls outside the box or too close to the boundary for the largest particle.");
        }
    }
    if (equilibrium_window < 3) throw std::runtime_error("equilibrium_window must be at least 3 observable rows.");
    if (equilibrium_required_windows <= 0) throw std::runtime_error("equilibrium_required_windows must be positive.");
    if (equilibrium_temp_tol < 0.0 || equilibrium_density_tol < 0.0 || equilibrium_flux_tol < 0.0 || equilibrium_slope_tol < 0.0) {
        throw std::runtime_error("Equilibrium tolerances must be non-negative.");
    }
    if (separated_steady_state_min_score < 0.0) {
        throw std::runtime_error("separated_steady_state_min_score must be non-negative.");
    }
}
