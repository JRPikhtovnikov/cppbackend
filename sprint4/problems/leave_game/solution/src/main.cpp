#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <thread>
#include <filesystem>
#include <optional>
#include <vector>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "logging_request_handler.h"
#include "http_server.h"
#include "ticker.h"

using namespace std::literals;
namespace net = boost::asio;
namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) workers.emplace_back(fn);
    fn();
}

struct Args {
    std::optional<int> tick_period_ms;
    fs::path config_file;
    fs::path www_root;
    bool randomize_spawn_points = false;
    std::optional<fs::path> state_file;        
    std::optional<int> save_state_period_ms;    
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    Args args;
    po::options_description desc{"Allowed options"s};
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<int>()->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value<std::string>()->value_name("file"s), "set config file path")
        ("www-root,w", po::value<std::string>()->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points),
                                   "spawn dogs at random positions")
        ("state-file", po::value<std::string>()->value_name("file"s), "set state file path")
        ("save-state-period", po::value<int>()->value_name("milliseconds"s), "set state save period (ignored without --state-file)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    if (!vm.contains("config-file"s))
        throw std::runtime_error("Config file path is not specified");
    if (!vm.contains("www-root"s))
        throw std::runtime_error("Static files root is not specified");

    args.config_file = vm["config-file"s].as<std::string>();
    args.www_root = vm["www-root"s].as<std::string>();

    if (vm.contains("tick-period"s)) {
        int p = vm["tick-period"s].as<int>();
        if (p <= 0) throw std::runtime_error("tick-period must be positive");
        args.tick_period_ms = p;
    }

    if (vm.contains("state-file"s)) {
        args.state_file = fs::path(vm["state-file"s].as<std::string>());
        if (vm.contains("save-state-period"s)) {
            int p = vm["save-state-period"s].as<int>();
            if (p <= 0) throw std::runtime_error("save-state-period must be positive");
            args.save_state_period_ms = p;
        }
    }

    return args;
}

} // namespace

int main(int argc, const char* argv[]) {
    logger::InitLogging();

    try {
        auto parsed = ParseCommandLine(argc, argv);
        if (!parsed) {
            return EXIT_SUCCESS;
        }
        const auto& args = *parsed;

        const char* config_env = std::getenv("CONFIG_PATH");
        if (config_env) {
            args.config_file = fs::path(config_env);
        }

        const char* db_url = std::getenv("GAME_DB_URL");
        if (!db_url) {
            throw std::runtime_error("GAME_DB_URL environment variable not set");
        }

        const unsigned num_threads = std::thread::hardware_concurrency();
        db::ConnectionPool conn_pool(num_threads, [db_url] {
            return std::make_shared<pqxx::connection>(db_url);
        });

        db::Database db(conn_pool);
        db.Initialize();

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        if (!fs::exists(args.www_root) || !fs::is_directory(args.www_root)) {
            throw std::runtime_error("Static directory does not exist: " + fs::absolute(args.www_root).string());
        }

        net::io_context ioc(std::max(1u, num_threads));

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int) {
            if (!ec) ioc.stop();
        });

        auto api_strand = net::make_strand(ioc);

        const bool realtime = args.tick_period_ms.has_value();
        const bool manual_tick_enabled = !realtime;

        double dog_retirement_time = 60.0;
        json_loader::LootTypesMap loot_types_map;
        json_loader::LootValuesMap loot_values_map;
        double loot_period = 0.0, loot_probability = 0.0;
        model::Game game = json_loader::LoadGame(args.config_file, loot_types_map, loot_values_map, loot_period, loot_probability, dog_retirement_time);
        auto handler = std::make_shared<http_handler::RequestHandler>(
            game, api_strand, args.www_root,
            args.randomize_spawn_points,
            manual_tick_enabled,
            loot_types_map, loot_values_map,
            loot_period, loot_probability,
            args.state_file,                                         
            args.save_state_period_ms ? std::optional(std::chrono::milliseconds(*args.save_state_period_ms)) : std::nullopt,
            db, dog_retirement_time
        );

        if (args.state_file) {
            try {
                handler->LoadState();
            } catch (const std::exception& e) {
                logger::LogServerExited(EXIT_FAILURE, e.what());
                return EXIT_FAILURE;
            }
        }

        http_logging::LoggingRequestHandler logging_handler{*handler};

        http_server::ServeHttp(ioc, {address, port}, logging_handler);

        std::shared_ptr<Ticker> ticker;
        if (realtime) {
            ticker = std::make_shared<Ticker>(
                api_strand,
                std::chrono::milliseconds(*args.tick_period_ms),
                [&handler](std::chrono::milliseconds delta) {
                    handler->Tick(delta);
                });
            ticker->Start();
        }

        logger::LogServerStarted(address.to_string(), port);

        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

        if (args.state_file) {
            try {
                handler->SaveState();
            } catch (const std::exception& e) {
                logger::LogServerExited(EXIT_FAILURE, e.what());
                return EXIT_FAILURE;
            }
        }

        logger::LogServerExited(0);
        return 0;
    } catch (const std::exception& ex) {
        logger::LogServerExited(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }
}
