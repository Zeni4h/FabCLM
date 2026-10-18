#include "azure_monitor_client.hpp"
#include "nlohmann/json.hpp"
#include <iostream>
#include <algorithm>
#include <numeric>

namespace fabclm {

AzureMonitorClient::AzureMonitorClient(HttpClient& http, TokenProvider& tokenProvider)
    : http_(http), tokenProvider_(tokenProvider) {}

bool AzureMonitorClient::GetCapacityMetrics(const std::string& subscriptionId,
                                            const std::string& resourceGroup,
                                            const std::string& capacityName,
                                            CapacityMetricsSummary& outSummary,
                                            std::string& outError) {
    if (subscriptionId.empty() || resourceGroup.empty() || capacityName.empty()) {
        outError = "Azure subscription_id, resource_group, and capacity_name must be configured to query Azure Monitor metrics.";
        return false;
    }

    std::string token;
    if (!tokenProvider_.GetToken(armScope_, token, outError)) {
        return false;
    }

    // Azure Monitor metrics endpoint for Microsoft Fabric capacities
    std::string url = "https://management.azure.com/subscriptions/" + subscriptionId +
                      "/resourceGroups/" + resourceGroup +
                      "/providers/Microsoft.Fabric/capacities/" + capacityName +
                      "/providers/microsoft.insights/metrics?api-version=2024-02-01" +
                      "&metricnames=AverageCapacityUnitsUtilizationPercentage" +
                      "&timespan=PT1H&interval=PT1M";

    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + token},
        {"Accept", "application/json"}
    };

    HttpResponse res = http_.Get(url, headers);
    if (res.statusCode == 401) {
        tokenProvider_.InvalidateToken(armScope_);
        if (tokenProvider_.GetToken(armScope_, token, outError)) {
            headers["Authorization"] = "Bearer " + token;
            res = http_.Get(url, headers);
        }
    }

    if (!res.IsSuccess()) {
        outError = "Azure Monitor API error (HTTP " + std::to_string(res.statusCode) + "): " + res.body;
        return false;
    }

    try {
        nlohmann::json root = nlohmann::json::parse(res.body);
        outSummary.recentDataPoints.clear();

        if (root.contains("value") && root["value"].is_array() && !root["value"].empty()) {
            const auto& metricObj = root["value"][0];
            if (metricObj.contains("timeseries") && metricObj["timeseries"].is_array() && !metricObj["timeseries"].empty()) {
                const auto& tsObj = metricObj["timeseries"][0];
                if (tsObj.contains("data") && tsObj["data"].is_array()) {
                    double sum = 0.0;
                    int count = 0;
                    double peak = 0.0;
                    double latestVal = 0.0;
                    std::string latestTime;

                    for (const auto& dp : tsObj["data"]) {
                        std::string t = dp.value("timeStamp", "");
                        // Azure Monitor data point may have "average", "total", or "maximum"
                        if (dp.contains("average") && !dp["average"].is_null()) {
                            double v = dp["average"].get<double>();
                            MetricDataPoint point;
                            point.timestamp = t;
                            point.value = v;
                            outSummary.recentDataPoints.push_back(point);

                            sum += v;
                            count++;
                            if (v > peak) peak = v;
                            latestVal = v;
                            latestTime = t;
                        }
                    }

                    outSummary.currentUtilizationPct = latestVal;
                    outSummary.peakUtilizationPct = peak;
                    outSummary.averageUtilizationPct = (count > 0) ? (sum / count) : 0.0;
                    outSummary.lastUpdated = latestTime;
                    outSummary.isThrottled = (latestVal >= 100.0);
                    return true;
                }
            }
        }

        // If no timeseries data points found
        outSummary.currentUtilizationPct = 0.0;
        outSummary.peakUtilizationPct = 0.0;
        outSummary.averageUtilizationPct = 0.0;
        outSummary.lastUpdated = "No recent data";
        return true;
    } catch (const std::exception& e) {
        outError = "Failed to parse Azure Monitor metrics JSON: " + std::string(e.what());
        return false;
    }
}

} // namespace fabclm
