#pragma once

#include "metrics_model.hpp"
#include "auth.hpp"
#include "http_client.hpp"
#include <string>
#include <vector>

namespace fabclm {

class FabricClient {
public:
    FabricClient(HttpClient& http, TokenProvider& tokenProvider);

    // Get capacity information by ID or by Name
    bool GetCapacity(const std::string& capacityIdOrName, CapacityInfo& outInfo, std::string& outError);

    // List all capacities accessible to the caller
    bool ListCapacities(std::vector<CapacityInfo>& outCapacities, std::string& outError);

private:
    HttpClient& http_;
    TokenProvider& tokenProvider_;
    const std::string fabricScope_ = "https://api.fabric.microsoft.com/.default";
};

} // namespace fabclm
