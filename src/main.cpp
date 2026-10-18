#include "config.hpp"
#include "cli.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        fabclm::AppConfig config = fabclm::AppConfig::Load("config/config.json");
        fabclm::Cli cli(config);
        return cli.Run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
