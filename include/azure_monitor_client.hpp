#pragma once

#include "metrics_model.hpp"
#include "auth.hpp"
#include "http_client.hpp"
#include <string>

namespace fabclm {

class AzureMonitorClient {
public:
    AzureMonitorClient(HttpClient& http, TokenProvider& tokenProvider);

    // Queries Azure Monitor for capacity metrics
    bool GetCapacityMetrics(const std::string& subscriptionId,
                            const std::string& resourceGroup,
                            const std::string& capacityName,
                            CapacityMetricsSummary& outSummary,
                            std::string& outError);

private:
    HttpClient& http_;
    TokenProvider& tokenProvider_;
    const std::string armScope_ = "https://management.azure.com/.default";
};

} // namespace fabclm
