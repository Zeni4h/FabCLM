#pragma once

#include "config.hpp"
#include <vector>
#include <string>

namespace fabclm {

class Cli {
public:
    explicit Cli(const AppConfig& config);

    int Run(int argc, char* argv[]);

private:
    int HandleStatus();
    int HandleWatch(int intervalSeconds);
    int HandleList();
    int HandleTop(int limit);
    int HandleDiscover();
    int HandleConfig();
    void PrintHelp();

    AppConfig config_;
};

} // namespace fabclm
