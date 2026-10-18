#pragma once

#include <string>
#include <vector>

namespace fabclm {

struct AuthConfig {
    std::string tenantId;
    std::string clientId;
    std::string clientSecret;
    bool useDeviceCodeFlow = false;
};

struct FabricConfig {
    std::string capacityId;
    std::string capacityName;
};

struct AzureConfig {
    std::string subscriptionId;
    std::string resourceGroup;
};

struct MetricsAppConfig {
    std::string datasetId;
};

struct AppConfig {
    AuthConfig auth;
    FabricConfig fabric;
    AzureConfig azure;
    MetricsAppConfig metricsApp;

    static AppConfig Load(const std::string& configPath = "config/config.json");
    bool Validate(std::vector<std::string>& errors) const;
    void ApplyEnvironmentOverrides();
};

} // namespace fabclm
