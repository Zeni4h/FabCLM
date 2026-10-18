#pragma once

#include <string>
#include <map>
#include <vector>

namespace fabclm {

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string error;

    bool IsSuccess() const {
        return statusCode >= 200 && statusCode < 300;
    }
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Prevent copy, allow move
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    HttpResponse Get(const std::string& url, 
                     const std::map<std::string, std::string>& headers = {});

    HttpResponse Post(const std::string& url, 
                      const std::string& body,
                      const std::map<std::string, std::string>& headers = {});

private:
    struct Impl;
    Impl* pImpl;
};

} // namespace fabclm
