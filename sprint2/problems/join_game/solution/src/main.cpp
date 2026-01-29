#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <filesystem>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "logging_request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace fs = std::filesystem;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) workers.emplace_back(fn);
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> [static-path]"sv << std::endl;
        return EXIT_FAILURE;
    }

    logger::InitLogging();

    try {
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        model::Game game = json_loader::LoadGame(argv[1]);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int) {
            if (!ec) ioc.stop();
        });

        fs::path static_path;
        if (argc == 3) {
            static_path = fs::path(argv[2]);
            if (!fs::exists(static_path) || !fs::is_directory(static_path)) {
                throw std::runtime_error("Static directory does not exist: " +
                                         fs::absolute(static_path).string());
            }
        }

        http_handler::RequestHandler handler{game, static_path};
        http_logging::LoggingRequestHandler logging_handler{handler};

        http_server::ServeHttp(ioc, {address, port}, logging_handler);

        logger::LogServerStarted(address.to_string(), port);

        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

        logger::LogServerExited(0);
        return 0;
    } catch (const std::exception& ex) {
        logger::LogServerExited(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }
}
