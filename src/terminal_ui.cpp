#include "terminal_ui.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fabclm {

namespace {

const char* COLOR_RESET   = "\033[0m";
const char* COLOR_BOLD    = "\033[1m";
const char* COLOR_GREEN   = "\033[1;32m";
const char* COLOR_YELLOW  = "\033[1;33m";
const char* COLOR_RED     = "\033[1;31m";
const char* COLOR_CYAN    = "\033[1;36m";
const char* COLOR_BLUE    = "\033[1;34m";
const char* COLOR_GRAY    = "\033[90m";

std::string GetColorForPct(double pct) {
    if (pct < 70.0) return COLOR_GREEN;
    if (pct < 90.0) return COLOR_YELLOW;
    return COLOR_RED;
}

} // anonymous namespace

void TerminalUI::InitializeConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

void TerminalUI::ClearScreen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

std::string TerminalUI::GetProgressBar(double pct, int width) {
    if (pct < 0.0) pct = 0.0;
    int filled = static_cast<int>((pct / 100.0) * width);
    if (filled > width) filled = width;

    std::ostringstream ss;
    ss << GetColorForPct(pct) << "[";
    for (int i = 0; i < filled; ++i) {
        ss << "#";
    }
    ss << COLOR_GRAY;
    for (int i = filled; i < width; ++i) {
        ss << "-";
    }
    ss << GetColorForPct(pct) << "] " << std::fixed << std::setprecision(1) << pct << "%" << COLOR_RESET;
    return ss.str();
}

void TerminalUI::RenderStatus(const CapacityMetricsSummary& summary) {
    std::cout << "\n" << COLOR_CYAN << COLOR_BOLD 
              << "========================================================================\n"
              << "            MICROSOFT FABRIC CAPACITY MONITOR (FabCLM)                  \n"
              << "========================================================================" 
              << COLOR_RESET << "\n\n";

    // Capacity Info Card
    std::cout << COLOR_BOLD << " CAPACITY OVERVIEW" << COLOR_RESET << "\n";
    std::cout << " ----------------------------------------------------------------------\n";
    std::cout << " Name:        " << COLOR_BOLD << (summary.capacity.displayName.empty() ? "N/A" : summary.capacity.displayName) << COLOR_RESET << "\n";
    std::cout << " Capacity ID: " << COLOR_GRAY << (summary.capacity.id.empty() ? "N/A" : summary.capacity.id) << COLOR_RESET << "\n";
    std::cout << " SKU:         " << COLOR_CYAN << (summary.capacity.sku.empty() ? "N/A" : summary.capacity.sku) << COLOR_RESET;
    
    std::string stateColor = COLOR_GRAY;
    if (summary.capacity.state == CapacityState::Active) stateColor = COLOR_GREEN;
    else if (summary.capacity.state == CapacityState::Paused) stateColor = COLOR_YELLOW;

    std::cout << "       State:  " << stateColor << CapacityStateToString(summary.capacity.state) << COLOR_RESET;
    std::cout << "       Region: " << (summary.capacity.region.empty() ? "N/A" : summary.capacity.region) << "\n\n";

    // Utilization & Throttling Card
    std::cout << COLOR_BOLD << " REAL-TIME CU UTILIZATION (Past 1 Hour)" << COLOR_RESET << "\n";
    std::cout << " ----------------------------------------------------------------------\n";
    std::cout << " Current:     " << GetProgressBar(summary.currentUtilizationPct) << "\n";
    std::cout << " Peak (1h):   " << GetColorForPct(summary.peakUtilizationPct) << std::fixed << std::setprecision(1) << summary.peakUtilizationPct << "%" << COLOR_RESET;
    std::cout << "    Average (1h): " << GetColorForPct(summary.averageUtilizationPct) << std::fixed << std::setprecision(1) << summary.averageUtilizationPct << "%" << COLOR_RESET << "\n";
    
    std::cout << " Throttling:  ";
    if (summary.isThrottled) {
        std::cout << COLOR_RED << COLOR_BOLD << "THROTTLED (Consumption exceeded limits)" << COLOR_RESET << "\n";
    } else {
        std::cout << COLOR_GREEN << "OK (No active throttling detected)" << COLOR_RESET << "\n";
    }

    if (!summary.lastUpdated.empty()) {
        std::cout << " Last Event:  " << COLOR_GRAY << summary.lastUpdated << COLOR_RESET << "\n";
    }

    // Mini Sparkline / Trend Bar
    if (!summary.recentDataPoints.empty()) {
        std::cout << "\n" << COLOR_BOLD << " RECENT TREND (Last 10 intervals)" << COLOR_RESET << "\n";
        std::cout << " ";
        size_t start = (summary.recentDataPoints.size() > 10) ? (summary.recentDataPoints.size() - 10) : 0;
        for (size_t i = start; i < summary.recentDataPoints.size(); ++i) {
            double v = summary.recentDataPoints[i].value;
            std::cout << GetColorForPct(v);
            if (v < 25.0) std::cout << ".";
            else if (v < 50.0) std::cout << ":";
            else if (v < 75.0) std::cout << "o";
            else if (v < 90.0) std::cout << "O";
            else std::cout << "#";
            std::cout << COLOR_RESET << " ";
        }
        std::cout << " (Legend: . <25%  : <50%  o <75%  O <90%  # >90%)\n";
    }

    std::cout << "\n" << COLOR_GRAY 
              << "[Tip: Run 'fabclm watch' for live refresh. No background CUs are consumed.]\n" 
              << COLOR_RESET << "\n";
}

void TerminalUI::RenderConfigCheck(bool authOk, bool fabricOk, bool azureOk, const std::string& details) {
    std::cout << "\n" << COLOR_BOLD << "================ CONFIGURATION DIAGNOSTICS ================" << COLOR_RESET << "\n\n";

    std::cout << " 1. Entra ID Authentication:  " 
              << (authOk ? (std::string(COLOR_GREEN) + "PASSED") : (std::string(COLOR_RED) + "FAILED")) 
              << COLOR_RESET << "\n";

    std::cout << " 2. Fabric Capacity API:      " 
              << (fabricOk ? (std::string(COLOR_GREEN) + "PASSED") : (std::string(COLOR_RED) + "FAILED")) 
              << COLOR_RESET << "\n";

    std::cout << " 3. Azure Monitor Metrics API:" 
              << (azureOk ? (std::string(COLOR_GREEN) + "PASSED") : (std::string(COLOR_YELLOW) + "SKIPPED / PENDING")) 
              << COLOR_RESET << "\n\n";

    if (!details.empty()) {
        std::cout << COLOR_BOLD << "Diagnostic Notes:\n" << COLOR_RESET;
        std::cout << details << "\n";
    }
    std::cout << "===========================================================\n\n";
}

void TerminalUI::RenderTopConsumers(const std::vector<TopConsumerItem>& items) {
    std::cout << "\n" << COLOR_CYAN << COLOR_BOLD 
              << "========================================================================================\n"
              << "                      TOP CAPACITY CONSUMING WORKLOADS                                  \n"
              << "========================================================================================" 
              << COLOR_RESET << "\n\n";

    if (items.empty()) {
        std::cout << "No consumer records found in the metrics dataset.\n\n";
        return;
    }

    std::cout << COLOR_BOLD
              << std::left << std::setw(30) << "Item Name"
              << std::left << std::setw(18) << "Workload Type"
              << std::left << std::setw(24) << "Workspace"
              << std::right << std::setw(14) << "CU (s)"
              << COLOR_RESET << "\n";
    std::cout << "----------------------------------------------------------------------------------------\n";

    for (const auto& item : items) {
        std::string name = item.itemName.empty() ? (item.operationName.empty() ? "Unknown" : item.operationName) : item.itemName;
        if (name.length() > 28) name = name.substr(0, 25) + "...";

        std::string kind = item.itemKind.empty() ? "Operation" : item.itemKind;
        if (kind.length() > 16) kind = kind.substr(0, 13) + "...";

        std::string ws = item.workspaceName.empty() ? "N/A" : item.workspaceName;
        if (ws.length() > 22) ws = ws.substr(0, 19) + "...";

        std::cout << std::left << std::setw(30) << name
                  << COLOR_CYAN << std::left << std::setw(18) << kind << COLOR_RESET
                  << COLOR_GRAY << std::left << std::setw(24) << ws << COLOR_RESET
                  << COLOR_YELLOW << std::right << std::setw(14) << std::fixed << std::setprecision(2) << item.totalCuSeconds << COLOR_RESET
                  << "\n";
    }
    std::cout << "----------------------------------------------------------------------------------------\n\n";
}

} // namespace fabclm
