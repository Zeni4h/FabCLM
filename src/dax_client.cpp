#include "dax_client.hpp"
#include <iostream>
#include <algorithm>

namespace fabclm {

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string FindFieldString(const nlohmann::json& row, const std::vector<std::string>& candidates) {
    for (auto it = row.begin(); it != row.end(); ++it) {
        std::string key = ToLower(it.key());
        for (const auto& cand : candidates) {
            if (key.find(ToLower(cand)) != std::string::npos && it.value().is_string()) {
                return it.value().get<std::string>();
            }
        }
    }
    return "";
}

double FindFieldNumber(const nlohmann::json& row, const std::vector<std::string>& candidates) {
    for (auto it = row.begin(); it != row.end(); ++it) {
        std::string key = ToLower(it.key());
        for (const auto& cand : candidates) {
            if (key.find(ToLower(cand)) != std::string::npos) {
                if (it.value().is_number()) {
                    return it.value().get<double>();
                }
            }
        }
    }
    return 0.0;
}

} // anonymous namespace

DaxClient::DaxClient(HttpClient& http, TokenProvider& tokenProvider)
    : http_(http), tokenProvider_(tokenProvider) {}

bool DaxClient::ExecuteDax(const std::string& datasetId, 
                           const std::string& daxQuery, 
                           nlohmann::json& outRows, 
                           std::string& outError) {
    outRows = nlohmann::json::array();

    if (datasetId.empty()) {
        outError = "Dataset ID cannot be empty.";
        return false;
    }

    std::string token;
    if (!tokenProvider_.GetToken(pbiScope_, token, outError)) {
        return false;
    }

    std::string url = "https://api.powerbi.com/v1.0/myorg/datasets/" + datasetId + "/executeQueries";

    nlohmann::json payload = {
        {"queries", nlohmann::json::array({
            {{"query", daxQuery}}
        })}
    };

    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + token},
        {"Content-Type", "application/json"},
        {"Accept", "application/json"}
    };

    HttpResponse res = http_.Post(url, payload.dump(), headers);
    if (res.statusCode == 401) {
        tokenProvider_.InvalidateToken(pbiScope_);
        if (tokenProvider_.GetToken(pbiScope_, token, outError)) {
            headers["Authorization"] = "Bearer " + token;
            res = http_.Post(url, payload.dump(), headers);
        }
    }

    if (!res.IsSuccess()) {
        outError = "Power BI executeQueries failed (HTTP " + std::to_string(res.statusCode) + "): " + res.body;
        return false;
    }

    try {
        nlohmann::json root = nlohmann::json::parse(res.body);
        if (root.contains("results") && root["results"].is_array() && !root["results"].empty()) {
            const auto& firstResult = root["results"][0];
            if (firstResult.contains("tables") && firstResult["tables"].is_array() && !firstResult["tables"].empty()) {
                const auto& firstTable = firstResult["tables"][0];
                if (firstTable.contains("rows") && firstTable["rows"].is_array()) {
                    outRows = firstTable["rows"];
                    return true;
                }
            }
            if (firstResult.contains("error")) {
                outError = "DAX execution error: " + firstResult["error"].dump();
                return false;
            }
        }
        outError = "No tables or rows returned in executeQueries response.";
        return false;
    } catch (const std::exception& e) {
        outError = "Failed to parse executeQueries JSON: " + std::string(e.what());
        return false;
    }
}

bool DaxClient::DiscoverMetricsDatasets(std::vector<DiscoveredDataset>& outDatasets, std::string& outError) {
    outDatasets.clear();

    std::string token;
    if (!tokenProvider_.GetToken(pbiScope_, token, outError)) {
        return false;
    }

    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + token},
        {"Accept", "application/json"}
    };

    // 1. Try listing user datasets
    HttpResponse res = http_.Get("https://api.powerbi.com/v1.0/myorg/datasets", headers);
    if (res.IsSuccess()) {
        try {
            nlohmann::json root = nlohmann::json::parse(res.body);
            if (root.contains("value") && root["value"].is_array()) {
                for (const auto& ds : root["value"]) {
                    std::string name = ds.value("name", "");
                    if (ToLower(name).find("capacity metrics") != std::string::npos ||
                        ToLower(name).find("fabric metrics") != std::string::npos) {
                        DiscoveredDataset d;
                        d.id = ds.value("id", "");
                        d.name = name;
                        d.workspaceName = "My Workspace";
                        outDatasets.push_back(d);
                    }
                }
            }
        } catch (...) {}
    }

    // 2. Query workspaces (groups) and check their datasets
    HttpResponse groupsRes = http_.Get("https://api.powerbi.com/v1.0/myorg/groups", headers);
    if (groupsRes.IsSuccess()) {
        try {
            nlohmann::json root = nlohmann::json::parse(groupsRes.body);
            if (root.contains("value") && root["value"].is_array()) {
                for (const auto& g : root["value"]) {
                    std::string gid = g.value("id", "");
                    std::string gname = g.value("name", "");

                    HttpResponse dsRes = http_.Get("https://api.powerbi.com/v1.0/myorg/groups/" + gid + "/datasets", headers);
                    if (dsRes.IsSuccess()) {
                        nlohmann::json dsRoot = nlohmann::json::parse(dsRes.body);
                        if (dsRoot.contains("value") && dsRoot["value"].is_array()) {
                            for (const auto& ds : dsRoot["value"]) {
                                std::string name = ds.value("name", "");
                                if (ToLower(name).find("capacity metrics") != std::string::npos ||
                                    ToLower(name).find("fabric metrics") != std::string::npos ||
                                    ToLower(gname).find("capacity metrics") != std::string::npos) {
                                    DiscoveredDataset d;
                                    d.id = ds.value("id", "");
                                    d.name = name;
                                    d.workspaceId = gid;
                                    d.workspaceName = gname;
                                    outDatasets.push_back(d);
                                }
                            }
                        }
                    }
                }
            }
        } catch (...) {}
    }

    if (outDatasets.empty()) {
        outError = "No dataset matching 'Capacity Metrics' found in accessible workspaces.";
        return false;
    }
    return true;
}

bool DaxClient::GetTopConsumers(const std::string& datasetId, 
                                int limit, 
                                std::vector<TopConsumerItem>& outItems, 
                                std::string& outError) {
    outItems.clear();

    // Target confirmed Items table
    std::string q = "EVALUATE TOPN(" + std::to_string(limit) + ", 'Items')";
    nlohmann::json rows;
    if (!ExecuteDax(datasetId, q, rows, outError)) {
        return false;
    }

    for (const auto& row : rows) {
        TopConsumerItem item;
        item.itemName = FindFieldString(row, {"ItemName", "Name", "Operation"});
        item.itemKind = FindFieldString(row, {"ItemKind", "Kind", "Type", "Workload"});
        item.workspaceName = FindFieldString(row, {"WorkspaceName", "Workspace"});
        item.operationName = FindFieldString(row, {"OperationName", "Operation"});

        double cu = FindFieldNumber(row, {"TotalCU", "CU (s)", "CapacityUnitMs", "CU"});
        // If it's capacityUnitMs, convert to CU-seconds
        for (auto it = row.begin(); it != row.end(); ++it) {
            std::string key = ToLower(it.key());
            if (key.find("capacityunitms") != std::string::npos && it.value().is_number()) {
                cu = it.value().get<double>() / 1000.0;
                break;
            }
        }
        item.totalCuSeconds = cu;
        outItems.push_back(item);
    }

    return true;
}

bool DaxClient::GetRecentTimepoints(const std::string& datasetId,
                                    const std::string& capacityId,
                                    CapacityMetricsSummary& outSummary,
                                    std::string& outError) {
    // Query recent timepoints
    std::string q = "EVALUATE TOPN(20, 'TimePoints', 'TimePoints'[Timepoint], DESC)";

    nlohmann::json rows;
    if (!ExecuteDax(datasetId, q, rows, outError)) {
        return false;
    }

    outSummary.recentDataPoints.clear();
    double sum = 0.0;
    int count = 0;
    double peak = 0.0;
    double latestVal = 0.0;
    std::string latestTime;

    for (const auto& row : rows) {
        // Filter by capacity if row has CapacityId
        std::string rowCap = FindFieldString(row, {"CapacityId", "Capacity"});
        if (!rowCap.empty() && !capacityId.empty() && rowCap != capacityId) {
            continue;
        }

        std::string tp = FindFieldString(row, {"TimePoint", "Timestamp", "Date"});
        double util = FindFieldNumber(row, {"Total (CU)%", "Total Utilization %", "Utilization", "Percent"});

        if (latestTime.empty()) {
            latestTime = tp;
            latestVal = util;
        }

        MetricDataPoint pt;
        pt.timestamp = tp;
        pt.value = util;
        outSummary.recentDataPoints.push_back(pt);

        sum += util;
        count++;
        if (util > peak) peak = util;
    }

    if (count > 0) {
        outSummary.currentUtilizationPct = latestVal;
        outSummary.peakUtilizationPct = peak;
        outSummary.averageUtilizationPct = sum / count;
        outSummary.lastUpdated = latestTime;
        outSummary.isThrottled = (latestVal >= 100.0);
        return true;
    }

    outError = "No matching timepoint rows found for capacity in dataset.";
    return false;
}

} // namespace fabclm
