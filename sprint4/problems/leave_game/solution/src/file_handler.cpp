// file_handler.cpp
#include "file_handler.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace file_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;

std::string DecodeURL(std::string_view encoded) {
    std::string result;
    result.reserve(encoded.size());
    
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            int hex1 = std::tolower(static_cast<unsigned char>(encoded[i + 1]));
            int hex2 = std::tolower(static_cast<unsigned char>(encoded[i + 2]));
            
            if (std::isxdigit(hex1) && std::isxdigit(hex2)) {
                auto hex_char_to_int = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return 0;
                };
                
                int value = hex_char_to_int(encoded[i + 1]) * 16 + hex_char_to_int(encoded[i + 2]);
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += encoded[i];
            }
        } else if (encoded[i] == '+') {
            result += ' ';
        } else {
            result += encoded[i];
        }
    }
    
    return result;
}

StaticFileHandler::StaticFileHandler(fs::path root_path) 
    : root_path_(fs::weakly_canonical(root_path)) {
    InitMimeTypes();
}

void StaticFileHandler::InitMimeTypes() {
    mime_types_ = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpe", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"}
    };
}

bool StaticFileHandler::IsStaticFileRequest(std::string_view target) const {
    return !target.starts_with("/api/");
}

std::string StaticFileHandler::GetMimeType(std::string_view extension) const {
    std::string ext_lower;
    ext_lower.reserve(extension.size());
    std::transform(extension.begin(), extension.end(), 
                   std::back_inserter(ext_lower),
                   [](unsigned char c) { return std::tolower(c); });
    
    auto it = mime_types_.find(ext_lower);
    if (it != mime_types_.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}

bool StaticFileHandler::IsPathWithinRoot(const fs::path& path) const {
    try {
        auto canonical_path = fs::weakly_canonical(path);
        auto canonical_root = fs::weakly_canonical(root_path_);
        
        auto rel_path = fs::relative(canonical_path, canonical_root);
        return !rel_path.empty() && rel_path.native()[0] != '.';
    } catch (const std::exception&) {
        return false;
    }
}

void StaticFileHandler::HandleFileRequest(std::string_view target, 
                                         std::string_view method,
                                         SendHandler& send_handler) const {
    std::string decoded_path = DecodeURL(target);
    
    if (decoded_path.starts_with('/')) {
        decoded_path = decoded_path.substr(1);
    }
    
    if (decoded_path.empty() || decoded_path.back() == '/') {
        decoded_path += "index.html";
    }
    
    fs::path file_path = root_path_ / decoded_path;
    
    if (!IsPathWithinRoot(file_path)) {
        http::response<http::string_body> response{
            http::status::bad_request, 11};
        response.set(http::field::content_type, "text/plain");
        response.body() = "Bad Request: Path traversal attempt detected";
        response.prepare_payload();
        send_handler.SendStringResponse(std::move(response));
        return;
    }
    
    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        http::response<http::string_body> response{
            http::status::not_found, 11};
        response.set(http::field::content_type, "text/plain");
        response.body() = "File Not Found";
        response.prepare_payload();
        send_handler.SendStringResponse(std::move(response));
        return;
    }
    
    if (method == "HEAD") {
        http::response<http::empty_body> response{
            http::status::ok, 11};
        
        std::string extension = file_path.extension().string();
        response.set(http::field::content_type, GetMimeType(extension));
        response.set(http::field::content_length, 
                    std::to_string(fs::file_size(file_path)));
        response.prepare_payload();
        
        send_handler.SendEmptyResponse(std::move(response));
        return;
    }
    
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        http::response<http::string_body> response{
            http::status::not_found, 11};
        response.set(http::field::content_type, "text/plain");
        response.body() = "File Not Found";
        response.prepare_payload();
        send_handler.SendStringResponse(std::move(response));
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string file_content = buffer.str();
    
    http::response<http::string_body> response{
        http::status::ok, 11};
    
    std::string extension = file_path.extension().string();
    response.set(http::field::content_type, GetMimeType(extension));
    response.set(http::field::content_length, 
                std::to_string(file_content.size()));
    response.body() = std::move(file_content);
    response.prepare_payload();
    
    send_handler.SendStringResponse(std::move(response));
}

} // namespace file_handler