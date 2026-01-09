// main.cpp
#ifdef _WIN32
#include <sdkddkver.h>
#endif

// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <optional>

#include "http_server.h"

namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;  // Добавляем это

using namespace std::literals;

// Используем типы из http_server
using StringRequest = http_server::StringRequest;
using StringResponse = http_server::StringResponse;

// Вспомогательная функция для запуска рабочих потоков
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

// Структура ContentType задаёт область видимости для констант,
// задающих значения HTTP-заголовка Content-Type
struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
};

// Создаёт StringResponse с заданными параметрами
StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type = ContentType::TEXT_HTML) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, std::string(content_type));  // Исправить здесь
    response.body() = std::string(body);
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}

// Обработчик HTTP-запросов
StringResponse HandleRequest(StringRequest&& req) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto response = MakeStringResponse(
            http::status::method_not_allowed,
            "Invalid method"sv,
            req.version(),
            req.keep_alive()
        );
        response.set(http::field::allow, "GET, HEAD");
        return response;
    }

    std::string_view target = req.target();
    if (!target.empty() && target[0] == '/') {
        target.remove_prefix(1);
    }

    std::string body_for_get = "Hello, "s + std::string(target);
    
    std::string_view body_to_send;
    if (req.method() == http::verb::get) {
        body_to_send = body_for_get;
    } else {
        body_to_send = ""sv;
    }

    auto response = MakeStringResponse(
        http::status::ok,
        body_to_send,
        req.version(),
        req.keep_alive()
    );
    
    if (req.method() == http::verb::head) {
        response.content_length(body_for_get.size());
    }
    
    return response;
}

int main() {
    const unsigned num_threads = std::thread::hardware_concurrency();
    net::io_context ioc(num_threads);

    // Подписываемся на сигналы и при их получении завершаем работу сервера
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
        if (!ec) {
            std::cout << "Signal "sv << signal_number << " received"sv << std::endl;
            ioc.stop();
        }
    });

    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;

    // Запускаем HTTP-сервер
    http_server::ServeHttp(ioc, {address, port}, [](auto&& req, auto&& sender) {
        sender(HandleRequest(std::forward<decltype(req)>(req)));
    });

    // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
    std::cout << "Server has started..."sv << std::endl;

    // Запускаем обработку асинхронных операций
    RunWorkers(num_threads, [&ioc] {
        ioc.run();
    });

    std::cout << "Shutting down"sv << std::endl;
}