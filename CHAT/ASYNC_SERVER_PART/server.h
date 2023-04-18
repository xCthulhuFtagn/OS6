#pragma once

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
//defined to get rid of warnings

#include <ios>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <filesystem>
#include <functional>
#include <utility>
#include <tuple>

#include <coroutine>
//
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/as_tuple.hpp>

#include <iostream>

#include <filesystem>
namespace server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

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

typedef enum {
    s_success = 0,
    s_failure,
} server_response_e;

using namespace std::literals;

std::string StringifyRequest(client_request_e r);

std::string StringifyResponse(server_response_e r);

struct client_data_t : public std::enable_shared_from_this<client_data_t>{
    client_request_e request;
    std::string message_text;
};

struct server_data_t : public std::enable_shared_from_this<server_data_t>{
    client_request_e request;
    server_response_e response;
    std::string message_text;
};

// void ReportError(beast::error_code ec, std::string_view what);

enum class client_state{
    no_name = 0,
    list_chats,
    in_chat
};

enum class session_state{
    reading = 0,
    handling,
};

net::awaitable<void> Write(tcp::socket* socket, server_data_t&& response);

struct User{
    std::string name;
    std::string chat_name;
};

class Chat{
public:
    Chat(net::io_context& io, const std::filesystem::path& chat_path) : strand(net::make_strand(io)), path(chat_path){ 

        // file.exceptions(std::ifstream::failbit | std::ifstream::badbit );
        try{
            std::ofstream {path, std::fstream::app};
            // If no file is created, then
            // show the error message.

            // log
        } catch(std::ifstream::failure e){
            //log
            throw e;
        }
    }

    boost::asio::awaitable<void> AddUser(std::shared_ptr<tcp::socket> socket, std::string username);
    boost::asio::awaitable<void> SendMessage(std::shared_ptr<tcp::socket> socket, std::string message);
    boost::asio::awaitable<bool> ContainsUser(std::shared_ptr<tcp::socket> socket);

    boost::asio::awaitable<void> DeleteUser(std::shared_ptr<tcp::socket> socket);
private:
    net::strand<net::io_context::executor_type> strand;
    std::unordered_map<std::shared_ptr<tcp::socket>, std::streamoff> messages_offset; // Session to offset in chat file
    std::unordered_map<std::shared_ptr<tcp::socket>, std::string> usernames; // Session to username
    std::filesystem::path path; // Path to the file
};

class ChatManager{
public:
    ChatManager(net::io_context& io, std::string path) : io_(io), chats_strand_(net::make_strand(io)), usernames_strand_(net::make_strand(io)), path_of_chats(path.data()){
        namespace fs = std::filesystem;
        if(!fs::exists(path_of_chats) || !fs::is_directory(path_of_chats)) fs::create_directory(path_of_chats);
        for (const auto& entry : fs::directory_iterator(path_of_chats)) {
            if (entry.is_regular_file() && fs::path(entry).extension() == ".chat") {
                chats_.insert({fs::path(entry).stem().string(), Chat(io, entry.path())});
            }
        }
    }

    boost::asio::awaitable<bool> SetName(std::string, std::shared_ptr<tcp::socket>);
    boost::asio::awaitable<std::string> ChatList();
    boost::asio::awaitable<void> UpdateChatList(std::shared_ptr<tcp::socket>);
    boost::asio::awaitable<void> ConnectChat(std::string, std::shared_ptr<tcp::socket>);
    boost::asio::awaitable<bool> CreateChat(std::string);
    boost::asio::awaitable<bool> LeaveChat(std::shared_ptr<tcp::socket>);
    boost::asio::awaitable<void> SendMessage(std::string, std::shared_ptr<tcp::socket>);

    boost::asio::awaitable<void> Disconnect(std::shared_ptr<tcp::socket>);

    boost::asio::awaitable<User*> IdentifyUser(std::shared_ptr<tcp::socket>);

private:

    std::unordered_map<std::string, Chat> chats_; // чаты с юзернеймами
    std::unordered_map<std::shared_ptr<tcp::socket>, User> users_; // юзеры
    net::io_context& io_;
    net::strand<net::io_context::executor_type> chats_strand_, usernames_strand_;
    std::filesystem::path path_of_chats;
};

boost::asio::awaitable<void> Session(tcp::socket&& socket_, ChatManager* chatManager);

class Server : public std::enable_shared_from_this<Server> {
public:
    explicit Server(net::io_context& ioc, const tcp::endpoint& endpoint, std::string chats_path)
        : ioc_(ioc)
        // Обработчики асинхронных операций acceptor_ будут вызываться в своём strand
        , acceptor_(net::make_strand(ioc))
        , chat_manager_(ioc, chats_path)
        {
        
        // Открываем acceptor, используя протокол (IPv4 или IPv6), указанный в endpoint
        acceptor_.open(endpoint.protocol());

        // После закрытия TCP-соединения сокет некоторое время может считаться занятым,
        // чтобы компьютеры могли обменяться завершающими пакетами данных.
        // Однако это может помешать повторно открыть сокет в полузакрытом состоянии.
        // Флаг reuse_address разрешает открыть сокет, когда он "наполовину закрыт"
        acceptor_.set_option(net::socket_base::reuse_address(true));
        // Привязываем acceptor к адресу и порту endpoint
        acceptor_.bind(endpoint);
        // Переводим acceptor в состояние, в котором он способен принимать новые соединения
        // Благодаря этому новые подключения будут помещаться в очередь ожидающих соединений
        acceptor_.listen(net::socket_base::max_listen_connections);
    }

    void Run();
private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    ChatManager chat_manager_;

    std::shared_ptr<Server> GetSharedThis();
    void DoAccept();
    void OnAccept(sys::error_code ec, tcp::socket socket);
    void AsyncRunSession(tcp::socket&& socket);
};

}  // namespace server
