#include "http_server.h"
#include "logger.h"

namespace http_server {

using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view what) {
    logger::LogNetError(ec, what);
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() {
    request_ = {};
    stream_.expires_after(30s);
    http::async_read(stream_, buffer_, request_,
                     beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, std::size_t) {
    if (ec == http::error::end_of_stream) return Close();
    if (ec) return ReportError(ec, "read"sv);

    std::string client_ip;
    {
        beast::error_code ep_ec;
        auto ep = stream_.socket().remote_endpoint(ep_ec);
        if (!ep_ec) {
            client_ip = ep.address().to_string();
        }
    }

    HandleRequest(std::move(request_), std::move(client_ip));
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    if (ec) {
        return ReportError(ec, "write"sv);
    }

    if (close) {
        return Close();
    }

    Read();
}

void SessionBase::Close() {
    stream_.socket().shutdown(tcp::socket::shutdown_send);
}

}  // namespace http_server