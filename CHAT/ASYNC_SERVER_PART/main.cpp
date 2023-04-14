#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <iostream>
#include <thread>

#include "server.h"
#include "logger.h"

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

        LOG_INIT()

        // 1. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 2. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                boost::json::value data = {
                    {"signal", signal_number}
                };
                logger::Logger::GetInstance().Log("Signal received"sv, data);
                ioc.stop();
            }
        });

        // 3. Настройка сетевых параметров
        constexpr boost::asio::ip::port_type port = 5000;
        const auto address = net::ip::tcp::endpoint(net::ip::tcp::v4(), port).address();
        net::ip::tcp::endpoint endpoint = {address, port};

        //4. Запускаем серверную обработку запросов
        std::make_shared<server::Server>(ioc,  endpoint, "./chats"s)->Run();

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        boost::json::value data = {
            {"address", address.to_string()},
            {"port", port}
        };
        logger::Logger::GetInstance().Log("Server started"sv, data);

        // 5. Запускаем обработку асинхронных операций
        RunThreads(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    }
    catch (const std::exception& ex) {
        boost::json::value data = {
            {"exception", ex.what()}
        };
        logger::Logger::GetInstance().Log("Exception occured"sv, data);
        return EXIT_FAILURE;
    }
}
