#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <functional>
#include <boost/beast/http.hpp>

namespace file_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;

std::string DecodeURL(std::string_view encoded);

class SendHandler {
public:
    virtual ~SendHandler() = default;
    virtual void SendStringResponse(http::response<http::string_body>&& response) = 0;
    virtual void SendEmptyResponse(http::response<http::empty_body>&& response) = 0;
};

class StaticFileHandler {
public:
    StaticFileHandler(fs::path root_path);
    
    static bool IsStaticFileRequest(std::string_view target);
    
    std::string GetMimeType(std::string_view extension) const;
    
    void HandleFileRequest(std::string_view target, 
                          std::string_view method,
                          SendHandler& send_handler) const;
    
    bool IsPathWithinRoot(const fs::path& path) const;
    
private:
    fs::path root_path_;
    std::unordered_map<std::string, std::string> mime_types_;
    
    void InitMimeTypes();
};

} // namespace file_handler