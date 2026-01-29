#pragma once

#include <boost/json.hpp>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>

#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace logger {

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace json = boost::json;

// Произвольные json-данные, которые передаём в запись.
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

inline void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    using boost::posix_time::ptime;

    json::object obj;

    // timestamp
    if (auto ts = logging::extract<ptime>("TimeStamp", rec)) {
        obj["timestamp"] = boost::posix_time::to_iso_extended_string(ts.get());
    } else {
        obj["timestamp"] = "";
    }

    // data (ваш keyword работает корректно)
    if (auto data = rec[additional_data]) {
        obj["data"] = data.get();
    } else {
        obj["data"] = json::object{};
    }

    // message (строка сообщения хранится в атрибуте "Message")
    if (auto msg = logging::extract<std::string>("Message", rec)) {
        obj["message"] = msg.get();
    } else {
        obj["message"] = "";
    }

    strm << json::serialize(obj) << '\n';
}

inline void InitLogging() {
    static std::once_flag once;
    std::call_once(once, [] {
        logging::add_common_attributes();

        using text_sink = sinks::synchronous_sink<sinks::text_ostream_backend>;
        auto sink = boost::make_shared<text_sink>();

        // Пишем именно в stdout.
        sink->locked_backend()->add_stream(
            boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter{}));
        sink->locked_backend()->auto_flush(true);

        sink->set_formatter(&JsonFormatter);
        logging::core::get()->add_sink(sink);
    });
}

// --- Строго по ТЗ: message + data ---

inline void LogServerStarted(std::string_view address, unsigned short port) {
    json::object data;
    data["port"] = port;
    data["address"] = std::string(address);

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(additional_data, json::value(std::move(data)))
        << "server started";
}

inline void LogServerExited(int code, std::optional<std::string_view> exception = std::nullopt) {
    json::object data;
    data["code"] = code;
    if (exception) {
        data["exception"] = std::string(*exception);
    }

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(additional_data, json::value(std::move(data)))
        << "server exited";
}

inline void LogRequestReceived(std::string_view ip, std::string_view uri, std::string_view method) {
    json::object data;
    data["ip"] = std::string(ip);
    data["URI"] = std::string(uri);
    data["method"] = std::string(method);

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(additional_data, json::value(std::move(data)))
        << "request received";
}

inline void LogResponseSent(std::string_view ip, int response_time_ms, int code,
                            std::optional<std::string_view> content_type) {
    json::object data;
    data["ip"] = std::string(ip);
    data["response_time"] = response_time_ms;
    data["code"] = code;
    if (content_type) {
        data["content_type"] = std::string(*content_type);
    } else {
        data["content_type"] = nullptr;
    }

    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(additional_data, json::value(std::move(data)))
        << "response sent";
}

inline void LogNetError(const boost::system::error_code& ec, std::string_view where) {
    json::object data;
    data["code"] = ec.value();
    data["text"] = ec.message();
    data["where"] = std::string(where);

    BOOST_LOG_TRIVIAL(error)
        << logging::add_value(additional_data, json::value(std::move(data)))
        << "error";
}

} // namespace logger
