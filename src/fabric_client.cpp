#include "fabric_client.hpp"
#include "nlohmann/json.hpp"
#include <iostream>

namespace fabclm {

namespace {

CapacityInfo ParseCapacityJson(const nlohmann::json& item) {
    CapacityInfo info;
    info.id = item.value("id", "");
    info.displayName = item.value("displayName", "");
    info.sku = item.value("sku", "");
    info.state = StringToCapacityState(item.value("state", "Unknown"));
    info.region = item.value("region", "");

    if (item.contains("admins") && item["admins"].is_array()) {
        for (const auto& admin : item["admins"]) {
            if (admin.is_string()) {
                info.admins.push_back(admin.get<std::string>());
            }
        }
    }
    return info;
}

} // anonymous namespace

FabricClient::FabricClient(HttpClient& http, TokenProvider& tokenProvider)
    : http_(http), tokenProvider_(tokenProvider) {}

bool FabricClient::ListCapacities(std::vector<CapacityInfo>& outCapacities, std::string& outError) {
    outCapacities.clear();

    std::string token;
    if (!tokenProvider_.GetToken(fabricScope_, token, outError)) {
        return false;
    }

    std::string url = "https://api.fabric.microsoft.com/v1/capacities";
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + token},
        {"Accept", "application/json"}
    };

    HttpResponse res = http_.Get(url, headers);
    if (res.statusCode == 401) {
        tokenProvider_.InvalidateToken(fabricScope_);
        if (tokenProvider_.GetToken(fabricScope_, token, outError)) {
            headers["Authorization"] = "Bearer " + token;
            res = http_.Get(url, headers);
        }
    }

    if (!res.IsSuccess()) {
        outError = "Fabric API error (HTTP " + std::to_string(res.statusCode) + "): " + res.body;
        return false;
    }

    try {
        nlohmann::json root = nlohmann::json::parse(res.body);
        if (root.contains("value") && root["value"].is_array()) {
            for (const auto& item : root["value"]) {
                outCapacities.push_back(ParseCapacityJson(item));
            }
            return true;
        } else {
            outError = "Unexpected Fabric API response structure: " + res.body;
            return false;
        }
    } catch (const std::exception& e) {
        outError = "Failed to parse Fabric capacity list JSON: " + std::string(e.what());
        return false;
    }
}

bool FabricClient::GetCapacity(const std::string& capacityIdOrName, CapacityInfo& outInfo, std::string& outError) {
    if (capacityIdOrName.empty()) {
        outError = "Capacity ID or Name cannot be empty";
        return false;
    }

    // First try direct lookup by ID
    std::string token;
    if (!tokenProvider_.GetToken(fabricScope_, token, outError)) {
        return false;
    }

    std::string url = "https://api.fabric.microsoft.com/v1/capacities/" + capacityIdOrName;
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + token},
        {"Accept", "application/json"}
    };

    HttpResponse res = http_.Get(url, headers);
    if (res.IsSuccess()) {
        try {
            nlohmann::json root = nlohmann::json::parse(res.body);
            outInfo = ParseCapacityJson(root);
            return true;
        } catch (const std::exception& e) {
            outError = "Failed to parse capacity JSON: " + std::string(e.what());
            return false;
        }
    }

    // If direct lookup fails (e.g. 404 or bad parameter because user provided a name rather than GUID),
    // fallback to listing all capacities and searching by displayName
    std::vector<CapacityInfo> all;
    if (ListCapacities(all, outError)) {
        for (const auto& cap : all) {
            if (cap.id == capacityIdOrName || cap.displayName == capacityIdOrName) {
                outInfo = cap;
                return true;
            }
        }
        outError = "Capacity not found: " + capacityIdOrName;
        return false;
    }

    return false;
}

} // namespace fabclm
