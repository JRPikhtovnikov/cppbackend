#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <filesystem>

#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace fs = std::filesystem;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    // Проверяем количество аргументов (теперь нужно 2 или 3)
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> [static-path]"sv << std::endl;
        return EXIT_FAILURE;
    }
    try {
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        
        // 1. Загружаем карту из файла
        model::Game game = json_loader::LoadGame(argv[1]);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем обработчик сигналов
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number) {
            if (!ec) {
                std::cout << "Signal " << signal_number << " received, stopping..." << std::endl;
                ioc.stop();
            }
        });

        // 4. Создаём обработчик HTTP-запросов
        fs::path static_path;
        if (argc == 3) {
            static_path = fs::path(argv[2]);
            
            // Проверяем, существует ли директория
            if (!fs::exists(static_path) || !fs::is_directory(static_path)) {
                std::cerr << "Static directory does not exist: " << static_path << std::endl;
                return EXIT_FAILURE;
            }
            
            std::cout << "Serving static files from: " << fs::absolute(static_path) << std::endl;
        }
        
        http_handler::RequestHandler handler{game, static_path};

        // 5. Запускаем HTTP сервер
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // 6. Сообщаем о запуске сервера
        std::cout << "Server has started..."sv << std::endl;

        // 7. Запускаем обработку
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}