#include <exception>
#include <iostream>
#include <string>

#include "config.hpp"
#include "simulation.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config.cfg>\n";
        return 1;
    }

    try {
        Config cfg = Config::from_file(argv[1]);
        Simulation simulation(std::move(cfg));
        simulation.run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 2;
    }
}
