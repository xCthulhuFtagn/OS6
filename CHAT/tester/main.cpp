#include <iostream>
#include <coroutine>
#include <memory>
#include <thread>
#include <sstream>
#include <iomanip>
#include <random>
#include <vector>

#include <boost/array.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace sys = boost::system;

using namespace std::literals;

enum client_request_e {
    c_set_name = 0,
    c_create_chat,
    c_connect_chat,
    c_send_message,
    c_leave_chat,
    c_disconnect,
    c_receive_message,
    c_wrong_request,
    c_get_chats
};

enum server_response_e{
    s_success = 0,
    s_failure,
};

struct client_data_t{
    client_request_e request;
    std::string message_text;
};

struct server_data_t{
    client_request_e request;
    server_response_e response;
    std::string message_text;
};


//get random element from container
template<typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return select_randomly(start, end, gen);
}

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

net::awaitable<void> Write(tcp::socket* socket, client_data_t&& request){
    size_t message_length = request.message_text.size();
    auto safe_request = request;

    // std::cout << "Sending message: {request: " << response.request 
    // << ", response: " << response.response << ", message: \"" << response.message_text << "\"}" << std::endl;

    auto [ec, bytes] = co_await net::async_write(
        *socket, 
        boost::array<boost::asio::mutable_buffer, 4>({
            net::buffer((void*)&safe_request.request, sizeof(client_request_e)),
            net::buffer((void*)&message_length, sizeof(size_t)),
            net::buffer(safe_request.message_text)
        }),
        // boost::asio::transfer_all(), // maybe needed
        net::experimental::as_tuple(net::use_awaitable)
    );

    if(ec) {
        //log here
        throw ec;
    }

    // std::cout << "Message sent" << std::endl;
}

net::awaitable<server_data_t> Read(tcp::socket* socket){
    server_data_t response;
    size_t message_length;

    auto [ec, bytes] = co_await 
        boost::asio::async_read(
            *socket, 
            boost::array<boost::asio::mutable_buffer, 3>({
            net::buffer((void*)&response.request, sizeof(client_request_e)),
            net::buffer((void*)&response.response, sizeof(server_response_e)),
            net::buffer((void*)&message_length, sizeof(size_t)),
            }),
            // boost::asio::transfer_all(), // maybe needed
            boost::asio::experimental::as_tuple(boost::asio::use_awaitable)
        );
        if (!ec) {
            response.message_text.resize(message_length);
            auto [ec2, bytes2] = co_await boost::asio::async_read(
                *socket, 
                boost::asio::buffer(response.message_text),
                // boost::asio::transfer_all(), // maybe needed
                boost::asio::experimental::as_tuple(boost::asio::use_awaitable)
            );
            if(ec2) {
                //log here
                throw ec2;
            }
        }
        else {
            //log here 
            throw ec;
        }
        co_return response;
}

std::vector<std::string> SplitString(const std::string& str, const std::string& delimiter){

    size_t start = 0, end;
    std::vector<std::string> split;
    while ((end = str.find(delimiter)) != std::string::npos) {
        split.push_back(str.substr(start, end));
        start += split.rbegin()->length() + delimiter.length();
    }
    return split;
}


boost::asio::awaitable<void> User(tcp::socket&& socket_){
    try{
        tcp::socket socket(std::move(socket_));

        client_data_t request;
        server_data_t response;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        //set name
        std::random_device random_device;
        std::mt19937_64 generator{[&] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device);
        }()};

        do{ // try until the name will be set
            //generating random name
            std::stringstream ss;
            ss << std::setw(8) << std::setfill('0') << std::hex << generator();
            request.message_text = ss.str();
            request.request = c_set_name;

            co_await Write(&socket, std::move(request));
            response = co_await Read(&socket);
        }while(response.response == s_failure);

        while(true){ //loop to switch between being in or out of chat
            //list of chats
            //getting rid of extra messages
            do{
                response = co_await Read(&socket); // receiving list of chats
            }while(response.request != c_get_chats);
            //split here
            auto chats = SplitString(response.message_text, "\n"s);

            while(true){ // list of chat loop
                if(chats.empty() || generator() % 100 < 10){ 
                    // 10% possibility to create chat & if no chats -> it will be created by default
                    request.request = c_create_chat;
                    // generating random chat name
                    std::stringstream ss;
                    ss << std::setw(8) << std::setfill('0') << std::hex << generator();
                    request.message_text = ss.str();
                    co_await Write(&socket, std::move(request));
                    while(true){
                        response = co_await Read(&socket);
                        if(response.request == c_get_chats) chats = SplitString(response.message_text, "\n"s);
                        else break;
                    }
                }
                else{ // connect to chat
                    request.request = c_connect_chat;
                    request.message_text = *select_randomly(chats.begin(), chats.end());
                    co_await Write(&socket, std::move(request));
                    while(true){
                        response = co_await Read(&socket);
                        if(response.request == c_get_chats) chats = SplitString(response.message_text, "\n"s);
                        else break;
                    }
                    if(response.response == s_success) break; // after connecting 
                }
            }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //in_chat

            while(true){ // chat loop
            //getting rid of extra messages
                do{
                    response = co_await Read(&socket);
                }while(response.request == c_get_chats || response.request == c_receive_message);
                auto i = generator();
                if(i % 100 < 5){ // 5% probability that user leaves chat
                    request.message_text.clear();
                    request.request = c_leave_chat;
                    co_await Write(&socket, std::move(request));
                    break;
                }
                else{
                    // random message
                    std::stringstream ss;
                    ss << std::setw(16) << std::setfill('0') << std::hex << generator() << generator();
                    request.message_text = ss.str();
                    request.request = c_send_message;
                    co_await Write(&socket, std::move(request));
                }
            }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //after exiting the chat loop user returns to out-of-chat loop
        }
    } catch(...){
        std::cout << "Error occured!" << std::endl;
    }
    co_return;
}

int main(int argc, const char* argv[]) {
    if(argc != 2){
        std::cout << "The only one correct parameter - number of user (currently " << argc - 1 << "arguments used)" << std::endl;
        return EXIT_FAILURE;
    }
    int N = std::stoi(argv[1]);
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

        // 3. Настройка сетевых параметров
        constexpr boost::asio::ip::port_type port = 5000;
        // пока локальный адрес, но дальше надо вписать другой
        const auto address = net::ip::tcp::endpoint(net::ip::tcp::v4(), port).address();  
        net::ip::tcp::endpoint endpoint = {address, port};


        //4. Запускаем N клиентов внутри контекста
        for(auto i = 0; i < N; ++i){
            tcp::socket socket(ioc);
            socket.connect(endpoint);
            boost::asio::co_spawn(ioc, User(std::move(socket)), net::detached);
        }

        // Эта надпись сообщает тестам о том, что тестер запущен и готов генерировать нагрузку
        std::cout << "Tester has started..."sv << std::endl;

        // 5. Запускаем обработку асинхронных операций
        RunThreads(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}