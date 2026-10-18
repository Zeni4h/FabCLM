#include "http_client.hpp"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <iostream>

namespace fabclm {

namespace {

std::wstring ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return wstr;
}

std::string ToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size, nullptr, nullptr);
    return str;
}

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool isHttps = true;
};

bool ParseUrl(const std::string& urlStr, ParsedUrl& out) {
    std::wstring wUrl = ToWString(urlStr);
    URL_COMPONENTS urlComp = { 0 };
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) {
        return false;
    }

    out.host = std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength);
    out.port = urlComp.nPort;
    out.isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    std::wstring path = (urlComp.dwUrlPathLength > 0) ? std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength) : L"/";
    if (urlComp.dwExtraInfoLength > 0) {
        path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }
    out.path = path;
    return true;
}

class AutoWinHttpHandle {
public:
    AutoWinHttpHandle(HINTERNET h = nullptr) : handle(h) {}
    ~AutoWinHttpHandle() {
        if (handle) {
            WinHttpCloseHandle(handle);
        }
    }
    HINTERNET get() const { return handle; }
    void reset(HINTERNET h = nullptr) {
        if (handle) WinHttpCloseHandle(handle);
        handle = h;
    }
    operator HINTERNET() const { return handle; }
private:
    HINTERNET handle;
};

} // anonymous namespace

struct HttpClient::Impl {
    AutoWinHttpHandle session;

    Impl() {
        session.reset(WinHttpOpen(
            L"FabCLM-CLI/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        ));
    }
};

HttpClient::HttpClient() : pImpl(new Impl()) {}

HttpClient::~HttpClient() {
    delete pImpl;
}

HttpClient::HttpClient(HttpClient&& other) noexcept : pImpl(other.pImpl) {
    other.pImpl = nullptr;
}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
    if (this != &other) {
        delete pImpl;
        pImpl = other.pImpl;
        other.pImpl = nullptr;
    }
    return *this;
}

HttpResponse HttpClient::Get(const std::string& url, const std::map<std::string, std::string>& headers) {
    HttpResponse response;
    if (!pImpl || !pImpl->session.get()) {
        response.error = "WinHTTP session initialization failed";
        return response;
    }

    ParsedUrl parsed;
    if (!ParseUrl(url, parsed)) {
        response.error = "Invalid URL: " + url;
        return response;
    }

    AutoWinHttpHandle connect(WinHttpConnect(pImpl->session.get(), parsed.host.c_str(), parsed.port, 0));
    if (!connect.get()) {
        response.error = "Failed to connect to host: " + ToString(parsed.host);
        return response;
    }

    DWORD flags = parsed.isHttps ? WINHTTP_FLAG_SECURE : 0;
    AutoWinHttpHandle request(WinHttpOpenRequest(
        connect.get(),
        L"GET",
        parsed.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    ));

    if (!request.get()) {
        response.error = "Failed to open request";
        return response;
    }

    // Set auto-redirect policy
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    // Add custom headers
    std::wstring headerStr;
    for (const auto& [k, v] : headers) {
        headerStr += ToWString(k) + L": " + ToWString(v) + L"\r\n";
    }

    if (!headerStr.empty()) {
        WinHttpAddRequestHeaders(
            request.get(),
            headerStr.c_str(),
            (DWORD)headerStr.length(),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE
        );
    }

    // Send request
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        response.error = "WinHttpSendRequest failed with error: " + std::to_string(GetLastError());
        return response;
    }

    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        response.error = "WinHttpReceiveResponse failed with error: " + std::to_string(GetLastError());
        return response;
    }

    // Read status code
    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    if (WinHttpQueryHeaders(
        request.get(),
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &size,
        WINHTTP_NO_HEADER_INDEX)) {
        response.statusCode = (int)statusCode;
    }

    // Read response body
    std::string responseBody;
    DWORD bytesAvailable = 0;
    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &bytesAvailable)) {
            break;
        }

        if (bytesAvailable == 0) {
            break;
        }

        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (WinHttpReadData(request.get(), buffer.data(), bytesAvailable, &bytesRead)) {
            responseBody.append(buffer.data(), bytesRead);
        }
    } while (bytesAvailable > 0);

    response.body = std::move(responseBody);
    return response;
}

HttpResponse HttpClient::Post(const std::string& url, 
                              const std::string& body,
                              const std::map<std::string, std::string>& headers) {
    HttpResponse response;
    if (!pImpl || !pImpl->session.get()) {
        response.error = "WinHTTP session initialization failed";
        return response;
    }

    ParsedUrl parsed;
    if (!ParseUrl(url, parsed)) {
        response.error = "Invalid URL: " + url;
        return response;
    }

    AutoWinHttpHandle connect(WinHttpConnect(pImpl->session.get(), parsed.host.c_str(), parsed.port, 0));
    if (!connect.get()) {
        response.error = "Failed to connect to host: " + ToString(parsed.host);
        return response;
    }

    DWORD flags = parsed.isHttps ? WINHTTP_FLAG_SECURE : 0;
    AutoWinHttpHandle request(WinHttpOpenRequest(
        connect.get(),
        L"POST",
        parsed.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    ));

    if (!request.get()) {
        response.error = "Failed to open request";
        return response;
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    // Add headers
    std::wstring headerStr;
    for (const auto& [k, v] : headers) {
        headerStr += ToWString(k) + L": " + ToWString(v) + L"\r\n";
    }

    if (!headerStr.empty()) {
        WinHttpAddRequestHeaders(
            request.get(),
            headerStr.c_str(),
            (DWORD)headerStr.length(),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE
        );
    }

    // Send request with body
    LPVOID pOptional = (LPVOID)body.data();
    DWORD optionalLength = (DWORD)body.length();

    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, pOptional, optionalLength, optionalLength, 0)) {
        response.error = "WinHttpSendRequest failed with error: " + std::to_string(GetLastError());
        return response;
    }

    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        response.error = "WinHttpReceiveResponse failed with error: " + std::to_string(GetLastError());
        return response;
    }

    // Read status code
    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    if (WinHttpQueryHeaders(
        request.get(),
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &size,
        WINHTTP_NO_HEADER_INDEX)) {
        response.statusCode = (int)statusCode;
    }

    // Read body
    std::string responseBody;
    DWORD bytesAvailable = 0;
    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &bytesAvailable)) {
            break;
        }

        if (bytesAvailable == 0) {
            break;
        }

        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (WinHttpReadData(request.get(), buffer.data(), bytesAvailable, &bytesRead)) {
            responseBody.append(buffer.data(), bytesRead);
        }
    } while (bytesAvailable > 0);

    response.body = std::move(responseBody);
    return response;
}

} // namespace fabclm
#endif
