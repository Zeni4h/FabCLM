#pragma once

#include "config.hpp"
#include "http_client.hpp"
#include <string>
#include <map>
#include <chrono>

namespace fabclm {

struct CachedToken {
    std::string token;
    std::chrono::system_clock::time_point expiresAt;
};

class TokenProvider {
public:
    explicit TokenProvider(const AuthConfig& config, HttpClient& httpClient);

    // Returns a bearer access token for the given resource scope.
    // Example scopes:
    // - Fabric: "https://api.fabric.microsoft.com/.default"
    // - ARM / Azure Monitor: "https://management.azure.com/.default"
    bool GetToken(const std::string& scope, std::string& outToken, std::string& outError);

    // Clear cached tokens (e.g. after 401 Unauthorized)
    void InvalidateToken(const std::string& scope);

private:
    bool AcquireTokenClientCredentials(const std::string& scope, std::string& outToken, std::string& outError);
    bool AcquireTokenDeviceCode(const std::string& scope, std::string& outToken, std::string& outError);

    AuthConfig config_;
    HttpClient& http_;
    std::map<std::string, CachedToken> tokenCache_;
};

} // namespace fabclm
