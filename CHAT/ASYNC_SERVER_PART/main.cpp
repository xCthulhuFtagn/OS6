#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <iostream>
#include <thread>

#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;


namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunThreads(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        // 1. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 2. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                std::cout << std::endl << "Signal "sv << signal_number << " received"sv << std::endl;
                ioc.stop();
            }
        });
        // 3. Создаём обработчик клиентских запросов и связываем его с менеджером чатов
        server::ChatManager<std::function<void(server::server_data_t&&)>> new_manager(ioc, "./chats");
        handler::RequestHandler handler(std::move(new_manager));

        // 4. Запускаем обработку запросов, делегируя их обработчику запросов
        constexpr net::ip::port_type port = 5000;
        const auto address = net::ip::tcp::endpoint(net::ip::tcp::v4(), port).address();
        server::ServeRequest(ioc, {address, port}, [&handler](auto&& req, auto&& state, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(state)>(state), std::forward<decltype(send)>(send));
        });

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        std::cout << "Server has started..."sv << std::endl;

        // 6. Запускаем обработку асинхронных операций
        RunThreads(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
