#pragma once

#include "metrics_model.hpp"
#include "auth.hpp"
#include "http_client.hpp"
#include "nlohmann/json.hpp"
#include <string>
#include <vector>

namespace fabclm {

struct DiscoveredDataset {
    std::string id;
    std::string name;
    std::string workspaceId;
    std::string workspaceName;
};

class DaxClient {
public:
    DaxClient(HttpClient& http, TokenProvider& tokenProvider);

    // Automatically locate the Capacity Metrics semantic model in the tenant/workspaces
    bool DiscoverMetricsDatasets(std::vector<DiscoveredDataset>& outDatasets, std::string& outError);

    // Execute raw DAX query against the dataset
    bool ExecuteDax(const std::string& datasetId, 
                    const std::string& daxQuery, 
                    nlohmann::json& outRows, 
                    std::string& outError);

    // Fetch top consuming items from FUAM semantic model
    bool GetTopConsumers(const std::string& datasetId, 
                         int limit, 
                         std::vector<TopConsumerItem>& outItems, 
                         std::string& outError);

    // Fetch recent utilization timepoints from FUAM semantic model
    bool GetRecentTimepoints(const std::string& datasetId,
                            const std::string& capacityId,
                            CapacityMetricsSummary& outSummary,
                            std::string& outError);

private:
    HttpClient& http_;
    TokenProvider& tokenProvider_;
    const std::string pbiScope_ = "https://analysis.windows.net/powerbi/api/.default";
};

} // namespace fabclm
