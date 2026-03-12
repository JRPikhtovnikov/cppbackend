#pragma once

#include "logger.h"
#include <boost/beast/http.hpp>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace http_logging {

namespace beast = boost::beast;
namespace http = beast::http;

template <class Decorated>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Decorated& decorated)
        : decorated_(decorated) {
    }

    template <class Body, class Fields, class Send>
    void operator()(http::request<Body, Fields>&& req, std::string client_ip, Send&& send) {
        LogRequest(req, client_ip);

        const auto started = std::chrono::steady_clock::now();

        decorated_(std::move(req),
                   [started, client_ip = std::move(client_ip), send = std::forward<Send>(send)]
                   (auto&& resp) mutable {
                       const auto finished = std::chrono::steady_clock::now();
                       const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(finished - started);
                       LogResponse(resp, client_ip, static_cast<int>(ms.count()));
                       send(std::forward<decltype(resp)>(resp));
                   });
    }

private:
    template <class Body, class Fields>
    static void LogRequest(const http::request<Body, Fields>& req, std::string_view client_ip) {
        const auto method_sv = http::to_string(req.method());
        const auto target_sv = req.target();

        logger::LogRequestReceived(
            client_ip,
            std::string_view(target_sv.data(), target_sv.size()),
            std::string_view(method_sv.data(), method_sv.size()));
    }

    template <class Body, class Fields>
    static void LogResponse(const http::response<Body, Fields>& resp,
                            std::string_view client_ip, int response_time_ms) {
        std::optional<std::string_view> content_type;
        auto it = resp.base().find(http::field::content_type);
        if (it != resp.base().end()) {
            const auto v = it->value();
            content_type = std::string_view(v.data(), v.size());
        }

        logger::LogResponseSent(client_ip, response_time_ms, resp.result_int(), content_type);
    }

    Decorated& decorated_;
};

} // namespace http_logging
