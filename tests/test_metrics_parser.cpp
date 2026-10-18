#include "metrics_model.hpp"
#include "nlohmann/json.hpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[Test] Verifying metrics model and JSON parsing...\n";

    std::string sampleFabricJson = R"({
        "id": "12345678-1234-1234-1234-123456789abc",
        "displayName": "Prod-Capacity-F64",
        "sku": "F64",
        "state": "Active",
        "region": "West US",
        "admins": ["admin@contoso.com"]
    })";

    auto j = nlohmann::json::parse(sampleFabricJson);
    fabclm::CapacityInfo info;
    info.id = j["id"];
    info.displayName = j["displayName"];
    info.sku = j["sku"];
    info.state = fabclm::StringToCapacityState(j["state"]);
    info.region = j["region"];

    assert(info.displayName == "Prod-Capacity-F64");
    assert(info.sku == "F64");
    assert(info.state == fabclm::CapacityState::Active);

    std::cout << "[Test] Passed: Capacity JSON parsing verified.\n";
    return 0;
}
