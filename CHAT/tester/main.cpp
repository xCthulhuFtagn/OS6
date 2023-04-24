#include <iostream>
#include <coroutine>
#include <memory>
#include <thread>
#include <sstream>
#include <iomanip>
#include <random>
#include <vector>
#include <chrono>

#include <boost/array.hpp>
#include <boost/scoped_ptr.hpp>

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
#include <boost/asio/connect.hpp>

// #include <boost/json.hpp>

#include "logger.h"

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

std::string StringifyRequest(client_request_e r){
    switch(r){
        case 0: return "c_set_name"s;
        case 1: return "c_create_chat"s;
        case 2: return "c_connect_chat"s;
        case 3: return "c_send_message"s;
        case 4: return "c_leave_chat"s;
        case 5: return "c_disconnect"s;
        case 6: return "c_receive_message"s;
        case 7: return "c_wrong_request"s;
        case 8: return "c_get_chats"s;
        default: return "wrong input"s;
    }
}

std::string StringifyResponse(server_response_e r){
    switch(r){
        case 0: return "s_success"s;
        case 1: return "s_failure"s;
        default: return "wrong input"s;
    }
}

net::awaitable<void> Write(std::shared_ptr<tcp::socket> socket, client_data_t&& request){
    size_t message_length = request.message_text.size();
    boost::scoped_ptr<client_data_t> safe_request(new client_data_t(request));

    // std::cout << "Sending message: {request: " << response.request 
    // << ", response: " << response.response << ", message: \"" << response.message_text << "\"}" << std::endl;

    auto [ec, bytes] = co_await net::async_write(
        *socket, 
        boost::array<boost::asio::mutable_buffer, 4>({
            net::buffer((void*)&safe_request->request, sizeof(client_request_e)),
            net::buffer((void*)&message_length, sizeof(size_t)),
            net::buffer(safe_request->message_text)
        }),
        // boost::asio::transfer_all(), // maybe needed
        net::experimental::as_tuple(net::use_awaitable)
    );

    if(ec) {
        //log here
        boost::json::value data = {
            {"error", ec.what()}
        };
        logger::Logger::GetInstance().Log("Client write error"sv, data);
        throw ec;
    }

    boost::json::value data = {
        {"request", 
            {   
                {"client_request_e", StringifyRequest(request.request)},
                {"message", request.message_text}
            }
        }
    };
    logger::Logger::GetInstance().Log("Wrote request to server"sv, data);
}

net::awaitable<server_data_t> Read(std::shared_ptr<tcp::socket> socket){
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
                boost::json::value data = {
                    {"error", ec.what()}
                };
                logger::Logger::GetInstance().Log("Client read error"sv, data);
                throw ec2;
            }
        }
        else {
            //log here 
            boost::json::value data = {
                {"error", ec.what()}
            };
            logger::Logger::GetInstance().Log("Client read error"sv, data);
            throw ec;
        }
        boost::json::value data = {
        {"response", 
                {   
                    {"client_request_e", StringifyRequest(response.request)},
                    {"server_response_e", StringifyResponse(response.response)},
                    {"message", response.message_text}
                }
            }
        };
        logger::Logger::GetInstance().Log("Read response from server"sv, data);
        co_return response;
}

std::vector<std::string> SplitString(std::string str, const std::string& delimiter){

    size_t pos;
    std::vector<std::string> split;
    while ((pos = str.find(delimiter)) != std::string::npos) {
        split.push_back(str.substr(0, pos));
        str.erase(0, pos + delimiter.length());
    }
    return split;
}


boost::asio::awaitable<void> User(tcp::socket socket_){
    try{
       std::shared_ptr<tcp::socket> socket = std::make_shared<tcp::socket>(std::move(socket_));

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
            ss << "Bot" << std::setw(8) << std::setfill('0') << std::hex << generator();
            request.message_text = ss.str();
            request.request = c_set_name;

            co_await Write(socket, std::move(request));
            std::chrono::system_clock::time_point start_ts = std::chrono::system_clock::now();
            response = co_await Read(socket);
            std::chrono::system_clock::time_point end_ts = std::chrono::system_clock::now();

            boost::json::value data = {
                {"response", 
                        {   
                            {"client_request_e", StringifyRequest(response.request)},
                            {"server_response_e", StringifyResponse(response.response)},
                            {"message", response.message_text}
                        }
                },
                {"response time", (end_ts - start_ts).count()}
            };
            logger::Logger::GetInstance().Log("Read first response from server"sv, data);

        }while(response.response == s_failure);

        while(true){ //loop to switch between being in and out of chat
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //list of chats
            //getting rid of extra messages
            do{
                response = co_await Read(socket); // receiving list of chats
            }while(response.request != c_get_chats); // other messages are just ignored
            //split here
            auto chats = SplitString(response.message_text, "\n"s);

            while(true){ // list of chat loop
                if(chats.size() < 4 || generator() % 1000 < 2){ 
                    // 0.2% possibility to create chat & if no chats -> it will be created by default
                    request.request = c_create_chat;
                    // generating random chat name
                    std::stringstream ss;
                    ss  << "Chat" << std::setw(8) << std::setfill('0') << std::hex << generator();
                    request.message_text = ss.str();
                    co_await Write(socket, std::move(request));
                    std::chrono::system_clock::time_point start_ts = std::chrono::system_clock::now();
                    while(true){
                        response = co_await Read(socket);
                        if(response.request == c_get_chats) {
                            chats = SplitString(response.message_text, "\n"s);
                            std::chrono::system_clock::time_point end_ts = std::chrono::system_clock::now();

                            boost::json::value data = {
                                {"response", 
                                        {   
                                            {"client_request_e", StringifyRequest(response.request)},
                                            {"server_response_e", StringifyResponse(response.response)},
                                            {"message", response.message_text}
                                        }
                                },
                                {"response time", (end_ts - start_ts).count()}
                            };
                            logger::Logger::GetInstance().Log("Read first response from server"sv, data);

                            break;
                        }
                    }
                }
                else{ // connect to chat
                    request.request = c_connect_chat;
                    request.message_text = *select_randomly(chats.begin(), chats.end());
                    co_await Write(socket, std::move(request));
                    std::chrono::system_clock::time_point start_ts = std::chrono::system_clock::now(); 
                    while(true){
                        response = co_await Read(socket);
                        if(response.request == c_get_chats) chats = SplitString(response.message_text, "\n"s);
                        else {
                            std::chrono::system_clock::time_point end_ts = std::chrono::system_clock::now();

                            boost::json::value data = {
                                {"response", 
                                        {   
                                            {"client_request_e", StringifyRequest(response.request)},
                                            {"server_response_e", StringifyResponse(response.response)},
                                            {"message", response.message_text}
                                        }
                                },
                                {"response time", (end_ts - start_ts).count()}
                            };
                            logger::Logger::GetInstance().Log("Read first response from server"sv, data);
                            
                            break;
                        }
                    }
                    if(response.response == s_success) break; // after connecting 
                }
            }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //in_chat

            while(true){ // chat loop
            //getting rid of extra messages
                while(socket->available() > 0){
                    response = co_await Read(socket);
                    if(response.request != c_get_chats && response.request != c_receive_message) break;
                }
                auto i = generator();
                if(i % 100 < 5){ // 5% probability that user leaves chat
                    request.message_text.clear();
                    request.request = c_leave_chat;
                    co_await Write(socket, std::move(request));
                    break;
                }
                else{
                    // random message
                    std::stringstream ss;
                    ss << std::setw(16) << std::setfill('0') << std::hex << generator() << generator();
                    request.message_text = ss.str();
                    request.request = c_send_message;
                    co_await Write(socket, std::move(request));
                }
            }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //after exiting the chat loop user returns to out-of-chat loop
        }
    }
    catch(sys::error_code& ec){}
    catch(std::exception& ex){
        boost::json::value data = {
            {"exception", ex.what()}
        };
        logger::Logger::GetInstance().Log("Client caught exception"sv, data);
    }
    catch(...){
        boost::json::value data = {};
        logger::Logger::GetInstance().Log("Client unknown error"sv, data);
    }

    boost::json::value data = {};
    logger::Logger::GetInstance().Log("Client finished it's work"sv, data);

    co_return;
}

int main(int argc, const char* argv[]) {
    if(argc != 2){
        std::cout << "The only one correct parameter - number of user (currently " << argc - 1 << "arguments used)" << std::endl;
        return EXIT_FAILURE;
    }
    int N = std::stoi(argv[1]);
    try {

        LOG_INIT()

        // 1. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 2. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                // std::cout << std::endl << "Signal "sv << signal_number << " received"sv << std::endl;
                ioc.stop();
            }
        });

        // 3. Настройка сетевых параметров
        constexpr boost::asio::ip::port_type port = 5000;
        // пока локальный адрес, но дальше надо вписать другой
        net::ip::tcp::endpoint endpoint = net::ip::tcp::endpoint(net::ip::address_v4::from_string("127.0.0.1"), port);  

        //4. Запускаем N клиентов внутри контекста
        for(auto i = 0; i < N; ++i){
            tcp::socket socket(ioc);
            tcp::resolver resolver(ioc);
            net::connect(socket, resolver.resolve(endpoint));
            boost::asio::co_spawn(ioc, User(std::move(socket)), net::detached);
            // [](std::exception_ptr ec)
            // {
            //     std::cout << "Handler" << std::endl;
            //     throw ec; 
            // });
        }

        // Эта надпись сообщает тестам о том, что тестер запущен и готов генерировать нагрузку
        boost::json::value data = {
            {"port", port},
            {"endpoint address", endpoint.address().to_string()}
        };
        logger::Logger::GetInstance().Log("Tester started"sv, data);

        // 5. Запускаем обработку асинхронных операций
        RunThreads(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    }
    catch (const std::exception& ex) {
        boost::json::value data = {
            {"exception", ex.what()}
        };
        logger::Logger::GetInstance().Log("Tester caught exception"sv, data);
        return EXIT_FAILURE;
    }
}