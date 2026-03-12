// file_handler.h
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

// Функция для URL-декодирования
std::string DecodeURL(std::string_view encoded);

// Базовый класс для обработчика отправки ответов
class SendHandler {
public:
    virtual ~SendHandler() = default;
    virtual void SendStringResponse(http::response<http::string_body>&& response) = 0;
    virtual void SendEmptyResponse(http::response<http::empty_body>&& response) = 0;
};

// Класс для обработки статических файлов
class StaticFileHandler {
public:
    StaticFileHandler(fs::path root_path);
    
    // Проверяет, является ли путь статическим файлом (не API)
    bool IsStaticFileRequest(std::string_view target) const;
    
    // Получает MIME-тип по расширению файла
    std::string GetMimeType(std::string_view extension) const;
    
    // Обрабатывает запрос статического файла
    void HandleFileRequest(std::string_view target, 
                          std::string_view method,
                          SendHandler& send_handler) const;
    
    // Проверяет, что путь находится внутри корневой директории
    bool IsPathWithinRoot(const fs::path& path) const;
    
private:
    fs::path root_path_;
    std::unordered_map<std::string, std::string> mime_types_;
    
    void InitMimeTypes();
};

} // namespace file_handler