#pragma once

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#define BOOST_BIND_NO_PLACEHOLDERS
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
//
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/asio/read.hpp>

#include <iostream>
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
    // s_new_message
} server_responce_e;

struct client_data_t : public std::enable_shared_from_this<client_data_t>{
    client_request_e request;
    std::string message_text;
};

struct server_data_t : public std::enable_shared_from_this<server_data_t>{
    client_request_e request;
    server_responce_e responce;
    std::string message_text;
};

void ReportError(beast::error_code ec, std::string_view what);

enum class client_state{
    no_name = 0,
    list_chats,
    in_chat
};

enum unblocked_read_state {
    REQ_TYPE,
    LENGTH,
    MESSAGE
};

template <typename Session>
class RequestHandlerBase{
public:
    virtual void handle(server::client_data_t&& req, server::client_state& state, std::shared_ptr<Session> session_ptr) = 0;
    virtual void handleDisconnection(Session*) = 0;
};

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket&& socket, RequestHandlerBase<Session>* request_handler)
        : socket_(std::move(socket))
        , request_handler_(request_handler) {
            socket_.non_blocking(true);
    }
    ~Session();
    void Run();
    void WriteKeepState(server_data_t && response);
    void Write(server_data_t&& response);

private:
    boost::asio::ip::tcp::socket socket_;
    client_data_t request_;
    RequestHandlerBase<Session>* request_handler_;

    unblocked_read_state read_state = REQ_TYPE;
    size_t read_message_length, write_message_length;
    client_state state_ = client_state::no_name;


    std::shared_ptr<Session> GetSharedThis();
    void Read();
    void ReadMessage(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read);
    void OnWrite(beast::error_code ec, [[maybe_unused]] std::size_t bytes_written);
    void HandleRequest(client_data_t && request);
};

struct Chat{
    Chat(net::io_context& io, const std::filesystem::path& chat_path) : strand(net::make_strand(io)), path(chat_path){}

    void AddUser(Session* session_ptr, std::string username);
    void DeleteUser(Session* session_ptr);
    void SendMessage(std::string message, Session* session_ptr);
    bool ContainsUser(Session* session_ptr);

    net::strand<net::io_context::executor_type> strand;
    std::unordered_map<Session*, std::streamoff> messages_offset; // Session to offset in chat file
    std::unordered_map<Session*, std::string> usernames; // Session to username
    std::filesystem::path path; // Path to the file
};

struct User{
    std::string name;
    std::string chat_name;
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

    bool SetName(std::string name, std::shared_ptr<Session> session_ptr);
    std::string ChatList();
    void UpdateChatList(std::shared_ptr<Session> session_ptr);
    void ConnectChat(std::string chat_name, std::shared_ptr<Session> session_ptr);
    bool CreateChat(std::string name);
    bool LeaveChat(std::shared_ptr<Session> session_ptr);
    void SendMessage(std::string message, std::shared_ptr<Session> session_ptr);

    void Disconnect(Session* session_ptr);
    
private:

    std::unordered_map<std::string, Chat> chats_; // чаты с юзернеймами
    std::unordered_map<Session*, User> users_; // мапа: СессияЫ к самому юзеру
    net::io_context& io_;
    net::strand<net::io_context::executor_type> chats_strand_, usernames_strand_;
    std::filesystem::path path_of_chats;


    User* IdentifyUser(Session* session_ptr){
        User* usr;
        net::dispatch(usernames_strand_, [this, &usr, &session_ptr]{
            if(!users_.contains(session_ptr)) usr = nullptr;
            else usr = &users_.at(session_ptr);
        });
        return usr;
    }
    
};

class RequestHandler : public RequestHandlerBase<Session>, std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(ChatManager&& new_manager) : chatManager(new_manager) {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    void handle(client_data_t&& req, client_state& state, std::shared_ptr<Session> shared_session_ptr);
    void handleDisconnection(Session* session_ptr);
private:
    ChatManager chatManager;
};

class Server : public std::enable_shared_from_this<Server> {
public:
    explicit Server(net::io_context& ioc, const tcp::endpoint& endpoint, std::string chats_path)
        : ioc_(ioc)
        // Обработчики асинхронных операций acceptor_ будут вызываться в своём strand
        , acceptor_(net::make_strand(ioc))
        , handler_(std::move(server::ChatManager(ioc, chats_path))) {
        
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
    RequestHandler handler_;

    std::shared_ptr<Server> GetSharedThis();
    void DoAccept();
    void OnAccept(sys::error_code ec, tcp::socket socket);
    void AsyncRunSession(tcp::socket&& socket);
};

}  // namespace server
