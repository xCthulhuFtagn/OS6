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
#include <boost/asio/co_spawn.hpp>

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

enum class session_state{
    reading = 0,
    handling,
};

net::awaitable<beast::error_code> Write(tcp::socket* socket, server_data_t&& response){
    size_t message_length = response.message_text.size();
    auto safe_response = std::make_shared<server_data_t>(std::move(response));
    auto [ec, bytes] = co_await net::async_write(
        *socket, 
        boost::array<boost::asio::mutable_buffer, 4>({
            net::buffer((void*)&safe_response->request, sizeof(client_request_e)),
            net::buffer((void*)&safe_response->responce, sizeof(server_responce_e)),
            net::buffer((void*)&message_length, sizeof(size_t)),
            net::buffer(safe_response->message_text)
        }),
        // boost::asio::transfer_all(), // maybe needed
        net::experimental::as_tuple(net::use_awaitable)
    );
    co_return ec;
}

struct User{
    std::string name;
    std::string chat_name;
};

class Chat{
public:
    Chat(net::io_context& io, std::string path){}
private:
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

    boost::asio::awaitable<bool> SetName(std::string, tcp::socket*);
    boost::asio::awaitable<std::string> ChatList();
    boost::asio::awaitable<void> UpdateChatList();
    boost::asio::awaitable<void> ConnectChat(std::string, tcp::socket*);
    boost::asio::awaitable<bool> CreateChat(std::string);
    boost::asio::awaitable<bool> LeaveChat(tcp::socket*);
    boost::asio::awaitable<void> SendMessage(std::string, tcp::socket*);

    boost::asio::awaitable<void> Disconnect(tcp::socket*);

    boost::asio::awaitable<User*> IdentifyUser(tcp::socket*);

private:

    std::unordered_map<std::string, Chat> chats_; // чаты с юзернеймами
    std::unordered_map<tcp::socket*, User> users_; // юзеры
    net::io_context& io_;
    net::strand<net::io_context::executor_type> chats_strand_, usernames_strand_;
    std::filesystem::path path_of_chats;
};

boost::asio::awaitable<void> Session(tcp::socket&& socket_, ChatManager* chatManager){
    bool running = true;
    session_state sessionState = session_state::reading;
    client_state clientState = client_state::no_name;
    client_data_t request_;
    server_data_t response_;
    size_t message_length;

    beast::error_code error_code;
    do{
        switch(sessionState){
        case session_state::reading:
        {
            auto [ec, bytes] = co_await boost::asio::async_read(
                socket_, 
                boost::array<boost::asio::mutable_buffer, 2>({
                    boost::asio::buffer((void*)&request_.request, sizeof(request_.request)),
                    boost::asio::buffer((void*)message_length, sizeof(message_length))
                }),
                // boost::asio::transfer_all(), // maybe needed
                boost::asio::experimental::as_tuple(boost::asio::use_awaitable)
            );
            if (!ec) {
                request_.message_text.resize(message_length);
                auto [ec2, bytes2] = co_await boost::asio::async_read(
                    socket_, 
                    boost::asio::buffer(request_.message_text),
                    boost::asio::experimental::as_tuple(boost::asio::use_awaitable)
                );
                if(!ec2) sessionState = session_state::handling;
                else running = false;
            }
            else running = false;
            break;
        }
        case session_state::handling:
            switch(request_.request){
                case client_request_e::c_set_name:
                    if(clientState != client_state::no_name){
                        response_.message_text = "Cannot set name twice!";
                        response_.responce = server_responce_e::s_failure;
                        if(!co_await Write(&socket_, std::move(response_))) {
                            running = false;
                            break;
                        }
                    } else {
                        if(co_await chatManager->SetName(request_.message_text, &socket_)){
                            clientState = client_state::list_chats;
                            response_.message_text = "";
                            response_.responce = server_responce_e::s_success;
                            if(!co_await Write(&socket_, std::move(response_))) {
                                running = false;
                                break;
                            }

                            response_.request = client_request_e::c_get_chats;
                            response_.responce = server_responce_e::s_success;
                            response_.message_text = co_await chatManager->ChatList();
                            

                            if(!co_await Write(&socket_, std::move(response_))) {
                                running = false;
                                break;
                            }

                        } else {
                            response_.message_text = "Username is already in use";
                            response_.responce = server_responce_e::s_failure;
                            if(!co_await Write(&socket_, std::move(response_))) {
                                running = false;
                                break;
                            }
                        }
                    }
                    break;
                case client_request_e::c_create_chat:
                    if(clientState != client_state::list_chats){
                        response_.message_text = "Cannot create chats, wrong state";
                        response_.responce = server_responce_e::s_failure;
                        if(!co_await Write(&socket_, std::move(response_))) {
                            running = false;
                            break;
                        }
                    }
                    else{
                        if(!co_await chatManager->CreateChat(request_.message_text)){
                            response_.message_text = "Chat with such name already exists";
                            response_.responce = server_responce_e::s_failure;
                            if(!(co_await Write(&socket_, std::move(response_)))) {
                                running = false;
                                break;
                            }
                        }
                        else{
                            // send everyone out of chat new list
                            co_await chatManager->UpdateChatList();
                        }
                    }
                    break;
                case client_request_e::c_connect_chat:
                    if(clientState != client_state::list_chats){
                        response_.message_text = "Cannot connect chat, wrong state!";
                        response_.responce = server_responce_e::s_failure;
                        if(!co_await Write(&socket_, std::move(response_))) {
                            running = false;
                            break;
                        }
                    }
                    else {
                        co_await chatManager->ConnectChat(request_.message_text, &socket_);
                        clientState = client_state::in_chat;
                    }
                    break;
                case client_request_e::c_send_message:
                    if(clientState != client_state::in_chat){
                        response_.message_text = "";
                        response_.responce = server_responce_e::s_failure;
                        if(!co_await Write(&socket_, std::move(response_))) {
                            running = false;
                            break;
                        }
                    }
                    else 
                        co_await chatManager->SendMessage(request_.message_text, &socket_);
                    break;
                case client_request_e::c_leave_chat:
                    if(clientState == client_state::in_chat && co_await chatManager->LeaveChat(&socket_)){
                        // Can leave chat, ok state && could to leave the chat
                        clientState = client_state::list_chats;
                        response_.request = client_request_e::c_get_chats;
                        response_.responce = server_responce_e::s_success;
                        response_.message_text = co_await chatManager->ChatList();
                        if(!co_await Write(&socket_, std::move(response_))) {
                            running = false;
                            break;
                        }
                    }
                    else{
                        response_.message_text = "";
                        response_.responce = server_responce_e::s_failure;
                        if(!co_await Write(&socket_, std::move(response_))) {
                            running = false;
                            break;
                        }
                    }
                    //no messages about leaving
                    break;
                default:
                    response_.message_text = "Wrong request";
                    response_.request = client_request_e::c_wrong_request;
                    response_.responce = server_responce_e::s_failure;
                    if(!co_await Write(&socket_, std::move(response_))) {
                        running = false;
                        break;
                    }
                }
        }
    }while(running);
    //disconnection
    socket_.close();
    co_await chatManager->Disconnect(&socket_);
    // request_handler_->handleDisconnection(this);
}



// struct Chat{
//     Chat(net::io_context& io, const std::filesystem::path& chat_path) : strand(net::make_strand(io)), path(chat_path){}

//     void AddUser(Session* session_ptr, std::string username);
//     void SendMessage(std::string message, Session* session_ptr);
//     bool ContainsUser(Session* session_ptr);

//     void DeleteUser(Session* session_ptr);

//     net::strand<net::io_context::executor_type> strand;
//     std::unordered_map<Session*, std::streamoff> messages_offset; // Session to offset in chat file
//     std::unordered_map<Session*, std::string> usernames; // Session to username
//     std::filesystem::path path; // Path to the file
// };

// struct User{
//     std::string name;
//     std::string chat_name;
// };

// class ChatManager{
// public:
//     ChatManager(net::io_context& io, std::string path) : io_(io), chats_strand_(net::make_strand(io)), usernames_strand_(net::make_strand(io)), path_of_chats(path.data()){
//         namespace fs = std::filesystem;
//         if(!fs::exists(path_of_chats) || !fs::is_directory(path_of_chats)) fs::create_directory(path_of_chats);
//         for (const auto& entry : fs::directory_iterator(path_of_chats)) {
//             if (entry.is_regular_file() && fs::path(entry).extension() == ".chat") {
//                 chats_.insert({fs::path(entry).stem().string(), Chat(io, entry.path())});
//             }
//         }
//     }

//     bool SetName(std::string name, Session* session_ptr);
//     std::string ChatList();
//     void UpdateChatList(Session* session_ptr);
//     void ConnectChat(std::string chat_name, Session* session_ptr);
//     bool CreateChat(std::string name);
//     bool LeaveChat(Session* session_ptr);
//     void SendMessage(std::string message, Session* session_ptr);

//     void Disconnect(Session* session_ptr);
    
// private:

//     std::unordered_map<std::string, Chat> chats_; // чаты с юзернеймами
//     std::unordered_map<Session*, User> users_; // мапа: Сессия к самому юзеру
//     net::io_context& io_;
//     net::strand<net::io_context::executor_type> chats_strand_, usernames_strand_;
//     std::filesystem::path path_of_chats;

//     User* IdentifyUser(Session* session_ptr){
//         User* usr;
//         net::dispatch(usernames_strand_, [this, &usr, shared = session_ptr->GetSharedThis()]{
//             if(!users_.contains(shared.get())) usr = nullptr;
//             else usr = &users_.at(shared.get());
//         });
//         return usr;
//     }
    
// };

class Server : public std::enable_shared_from_this<Server> {
public:
    explicit Server(net::io_context& ioc, const tcp::endpoint& endpoint, std::string chats_path)
        : ioc_(ioc)
        // Обработчики асинхронных операций acceptor_ будут вызываться в своём strand
        , acceptor_(net::make_strand(ioc))
        // , handler_(std::move(server::ChatManager(ioc, chats_path)))
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
    // RequestHandler handler_;

    std::shared_ptr<Server> GetSharedThis();
    void DoAccept();
    void OnAccept(sys::error_code ec, tcp::socket socket);
    void AsyncRunSession(tcp::socket&& socket);
};

}  // namespace server
