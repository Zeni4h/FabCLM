#pragma once

#include "metrics_model.hpp"
#include <string>

namespace fabclm {

class TerminalUI {
public:
    static void InitializeConsole();

    // Render single capacity status card
    static void RenderStatus(const CapacityMetricsSummary& summary);

    // Render diagnostic config check
    static void RenderConfigCheck(bool authOk, bool fabricOk, bool azureOk, const std::string& details);

    // Render top CU consuming operations/items
    static void RenderTopConsumers(const std::vector<TopConsumerItem>& items);

    // Clear screen for watch mode
    static void ClearScreen();

    // Helper to generate a colored utilization bar [████████░░░░] 65.0%
    static std::string GetProgressBar(double pct, int width = 25);
};

} // namespace fabclm
