#include "config.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace fabclm {

namespace {

std::string GetEnvVar(const char* name) {
#ifdef _MSC_VER
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) == 0 && buf != nullptr) {
        std::string val(buf);
        free(buf);
        return val;
    }
    return "";
#else
    const char* val = std::getenv(name);
    return val ? std::string(val) : "";
#endif
}

} // anonymous namespace

AppConfig AppConfig::Load(const std::string& configPath) {
    AppConfig config;

    std::ifstream file(configPath);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;

            if (j.contains("auth")) {
                const auto& a = j["auth"];
                if (a.contains("tenant_id") && a["tenant_id"].is_string()) config.auth.tenantId = a["tenant_id"];
                if (a.contains("client_id") && a["client_id"].is_string()) config.auth.clientId = a["client_id"];
                if (a.contains("client_secret") && a["client_secret"].is_string()) config.auth.clientSecret = a["client_secret"];
                if (a.contains("use_device_code_flow") && a["use_device_code_flow"].is_boolean()) {
                    config.auth.useDeviceCodeFlow = a["use_device_code_flow"];
                }
            }

            if (j.contains("fabric")) {
                const auto& f = j["fabric"];
                if (f.contains("capacity_id") && f["capacity_id"].is_string()) config.fabric.capacityId = f["capacity_id"];
                if (f.contains("capacity_name") && f["capacity_name"].is_string()) config.fabric.capacityName = f["capacity_name"];
            }

            if (j.contains("azure")) {
                const auto& az = j["azure"];
                if (az.contains("subscription_id") && az["subscription_id"].is_string()) config.azure.subscriptionId = az["subscription_id"];
                if (az.contains("resource_group") && az["resource_group"].is_string()) config.azure.resourceGroup = az["resource_group"];
            }

            if (j.contains("metrics_app")) {
                const auto& m = j["metrics_app"];
                if (m.contains("dataset_id") && m["dataset_id"].is_string()) {
                    std::string id = m["dataset_id"];
                    if (id.find("OPTIONAL") == std::string::npos) {
                        config.metricsApp.datasetId = id;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Error parsing config file: " << e.what() << "\n";
        }
    }

    config.ApplyEnvironmentOverrides();
    return config;
}

void AppConfig::ApplyEnvironmentOverrides() {
    std::string val;

    if (!(val = GetEnvVar("FABRIC_TENANT_ID")).empty()) auth.tenantId = val;
    if (!(val = GetEnvVar("FABRIC_CLIENT_ID")).empty()) auth.clientId = val;
    if (!(val = GetEnvVar("FABRIC_CLIENT_SECRET")).empty()) auth.clientSecret = val;
    if (!(val = GetEnvVar("FABRIC_CAPACITY_ID")).empty()) fabric.capacityId = val;
    if (!(val = GetEnvVar("FABRIC_CAPACITY_NAME")).empty()) fabric.capacityName = val;
    if (!(val = GetEnvVar("AZURE_SUBSCRIPTION_ID")).empty()) azure.subscriptionId = val;
    if (!(val = GetEnvVar("AZURE_RESOURCE_GROUP")).empty()) azure.resourceGroup = val;
    if (!(val = GetEnvVar("METRICS_DATASET_ID")).empty()) metricsApp.datasetId = val;
}

bool AppConfig::Validate(std::vector<std::string>& errors) const {
    errors.clear();

    if (auth.tenantId.empty() || auth.tenantId == "00000000-0000-0000-0000-000000000000") {
        errors.push_back("auth.tenant_id is missing or placeholder");
    }
    if (auth.clientId.empty() || auth.clientId == "00000000-0000-0000-0000-000000000000") {
        errors.push_back("auth.client_id is missing or placeholder");
    }
    if (!auth.useDeviceCodeFlow && auth.clientSecret.empty()) {
        errors.push_back("auth.client_secret is required when use_device_code_flow is false");
    }

    if (fabric.capacityId.empty() && fabric.capacityName.empty()) {
        errors.push_back("Either fabric.capacity_id or fabric.capacity_name must be specified");
    }

    return errors.empty();
}

} // namespace fabclm
