#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace fabclm {

enum class CapacityState {
    Active,
    Paused,
    Transitioning,
    Unknown
};

inline std::string CapacityStateToString(CapacityState state) {
    switch (state) {
        case CapacityState::Active: return "Active";
        case CapacityState::Paused: return "Paused";
        case CapacityState::Transitioning: return "Transitioning";
        default: return "Unknown";
    }
}

inline CapacityState StringToCapacityState(const std::string& str) {
    if (str == "Active" || str == "active") return CapacityState::Active;
    if (str == "Paused" || str == "paused") return CapacityState::Paused;
    if (str == "Transitioning" || str == "transitioning") return CapacityState::Transitioning;
    return CapacityState::Unknown;
}

struct CapacityInfo {
    std::string id;
    std::string displayName;
    std::string sku;           // e.g. "F64", "F128"
    CapacityState state = CapacityState::Unknown;
    std::string region;
    std::vector<std::string> admins;
};

struct MetricDataPoint {
    std::string timestamp;     // ISO 8601
    double value = 0.0;        // e.g. utilization percentage
};

struct CapacityMetricsSummary {
    CapacityInfo capacity;
    double currentUtilizationPct = 0.0;
    double peakUtilizationPct = 0.0;
    double averageUtilizationPct = 0.0;
    bool isThrottled = false;
    std::vector<MetricDataPoint> recentDataPoints;
    std::string lastUpdated;
};

struct TopConsumerItem {
    std::string itemName;
    std::string itemKind;
    std::string workspaceName;
    double totalCuSeconds = 0.0;
    std::string operationName;
    int operationCount = 0;
};

} // namespace fabclm
