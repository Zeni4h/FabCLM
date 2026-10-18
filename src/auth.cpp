#include "auth.hpp"
#include "nlohmann/json.hpp"
#include <iostream>
#include <thread>
#include <sstream>
#include <iomanip>

namespace fabclm {

namespace {

std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase << std::setw(2) << int((unsigned char)c) << std::nouppercase;
        }
    }
    return escaped.str();
}

} // anonymous namespace

TokenProvider::TokenProvider(const AuthConfig& config, HttpClient& httpClient)
    : config_(config), http_(httpClient) {}

void TokenProvider::InvalidateToken(const std::string& scope) {
    tokenCache_.erase(scope);
}

bool TokenProvider::GetToken(const std::string& scope, std::string& outToken, std::string& outError) {
    auto now = std::chrono::system_clock::now();

    // Check cache
    auto it = tokenCache_.find(scope);
    if (it != tokenCache_.end()) {
        // 60-second safety margin
        if (it->second.expiresAt > (now + std::chrono::seconds(60))) {
            outToken = it->second.token;
            return true;
        }
    }

    if (config_.useDeviceCodeFlow) {
        return AcquireTokenDeviceCode(scope, outToken, outError);
    } else {
        return AcquireTokenClientCredentials(scope, outToken, outError);
    }
}

bool TokenProvider::AcquireTokenClientCredentials(const std::string& scope, std::string& outToken, std::string& outError) {
    std::string tokenUrl = "https://login.microsoftonline.com/" + config_.tenantId + "/oauth2/v2.0/token";

    std::string body = "grant_type=client_credentials"
                     + std::string("&client_id=") + UrlEncode(config_.clientId)
                     + std::string("&client_secret=") + UrlEncode(config_.clientSecret)
                     + std::string("&scope=") + UrlEncode(scope);

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Accept", "application/json"}
    };

    HttpResponse res = http_.Post(tokenUrl, body, headers);
    if (!res.IsSuccess()) {
        outError = "Token acquisition failed (HTTP " + std::to_string(res.statusCode) + "): " + res.body;
        if (!res.error.empty()) {
            outError += " [" + res.error + "]";
        }
        return false;
    }

    try {
        nlohmann::json jsonRes = nlohmann::json::parse(res.body);
        if (jsonRes.contains("access_token")) {
            outToken = jsonRes["access_token"].get<std::string>();
            int expiresIn = jsonRes.value("expires_in", 3600);

            CachedToken cached;
            cached.token = outToken;
            cached.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(expiresIn);
            tokenCache_[scope] = cached;
            return true;
        } else {
            outError = "Token response missing 'access_token': " + res.body;
            return false;
        }
    } catch (const std::exception& e) {
        outError = "Failed to parse token response JSON: " + std::string(e.what());
        return false;
    }
}

bool TokenProvider::AcquireTokenDeviceCode(const std::string& scope, std::string& outToken, std::string& outError) {
    std::string deviceCodeUrl = "https://login.microsoftonline.com/" + config_.tenantId + "/oauth2/v2.0/devicecode";

    std::string body = "client_id=" + UrlEncode(config_.clientId)
                     + "&scope=" + UrlEncode(scope);

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Accept", "application/json"}
    };

    HttpResponse res = http_.Post(deviceCodeUrl, body, headers);
    if (!res.IsSuccess()) {
        outError = "Device code request failed (HTTP " + std::to_string(res.statusCode) + "): " + res.body;
        return false;
    }

    std::string deviceCode;
    std::string userCode;
    std::string verificationUri;
    int interval = 5;
    int expiresIn = 900;

    try {
        nlohmann::json j = nlohmann::json::parse(res.body);
        deviceCode = j.at("device_code").get<std::string>();
        userCode = j.at("user_code").get<std::string>();
        verificationUri = j.at("verification_uri").get<std::string>();
        interval = j.value("interval", 5);
        expiresIn = j.value("expires_in", 900);
    } catch (const std::exception& e) {
        outError = "Failed to parse device code JSON: " + std::string(e.what());
        return false;
    }

    std::cout << "\n=======================================================\n";
    std::cout << " MICROSOFT ENTRA ID AUTHENTICATION REQUIRED\n";
    std::cout << "=======================================================\n";
    std::cout << " 1. Open your browser: " << verificationUri << "\n";
    std::cout << " 2. Enter this code:   " << userCode << "\n";
    std::cout << "=======================================================\n";
    std::cout << "Waiting for login to complete...\n";

    std::string tokenUrl = "https://login.microsoftonline.com/" + config_.tenantId + "/oauth2/v2.0/token";
    std::string pollBody = "grant_type=urn:ietf:params:oauth:grant-type:device_code"
                         + std::string("&client_id=") + UrlEncode(config_.clientId)
                         + std::string("&device_code=") + UrlEncode(deviceCode);

    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(interval));

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > expiresIn) {
            outError = "Device code authentication timed out.";
            return false;
        }

        HttpResponse pollRes = http_.Post(tokenUrl, pollBody, headers);
        if (pollRes.IsSuccess()) {
            try {
                nlohmann::json tj = nlohmann::json::parse(pollRes.body);
                outToken = tj.at("access_token").get<std::string>();
                int tokenExpiresIn = tj.value("expires_in", 3600);

                CachedToken cached;
                cached.token = outToken;
                cached.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(tokenExpiresIn);
                tokenCache_[scope] = cached;

                std::cout << "Authentication successful!\n\n";
                return true;
            } catch (const std::exception& e) {
                outError = "Failed to parse token response: " + std::string(e.what());
                return false;
            }
        }

        try {
            nlohmann::json errJson = nlohmann::json::parse(pollRes.body);
            std::string err = errJson.value("error", "");
            if (err == "authorization_pending") {
                // User hasn't finished signing in yet, keep waiting
                continue;
            } else if (err == "slow_down") {
                interval += 5;
                continue;
            } else {
                outError = "Authentication failed: " + errJson.value("error_description", err);
                return false;
            }
        } catch (...) {
            outError = "Unexpected error response from token endpoint: " + pollRes.body;
            return false;
        }
    }
}

} // namespace fabclm
