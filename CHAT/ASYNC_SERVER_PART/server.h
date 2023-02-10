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
// Разместите здесь реализацию http-сервера, взяв её из задания по разработке асинхронного сервера

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

typedef struct {
    client_request_e request;
    std::vector<char> message_text;
} client_data_t;

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

template <typename SessionSharedPtr>
class RequestHandlerBase{
public:
    virtual void handle(server::client_data_t&& req, server::client_state& state, SessionSharedPtr session_ptr) = 0;
};

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket&& socket, RequestHandlerBase<std::shared_ptr<Session>>* request_handler)
        : socket_(std::move(socket))
        , request_handler_(request_handler) {
            socket_.non_blocking(true);
    }

    ~Session(){
        std::cout << "The session is down" << std::endl;
    }

    void Run() {
        // Вызываем метод Read, используя executor объекта socket_.
        // Таким образом вся работа со socket_ будет выполняться, используя его executor
        net::dispatch(socket_.get_executor(), beast::bind_front_handler(&Session::Read, GetSharedThis()));
    }

    void Write(std::vector<server_data_t>&& responses) {
        auto func = [this] (server_data_t& response, bool use_lambda = false) {
            auto safe_response = std::make_shared<server_data_t>(std::move(response));
            write_message_length = response.message_text.size();
            boost::asio::async_write(socket_, 
                boost::array<boost::asio::mutable_buffer, 4>({
                    boost::asio::buffer((void*)&safe_response->request, sizeof(client_request_e)),
                    boost::asio::buffer((void*)&safe_response->responce, sizeof(server_responce_e)),
                    boost::asio::buffer((void*)&write_message_length, sizeof(size_t)),
                    boost::asio::buffer(safe_response->message_text)
                }),
                boost::asio::transfer_all(), 
                [self = GetSharedThis(), safe_response, use_lambda](beast::error_code ec, std::size_t bytes_written) {
                        if (use_lambda) self->OnWrite(ec, bytes_written);
                }
            );
        };
        // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
        std::for_each(responses.begin(), responses.end() - 1, func);
        func(responses.back(), true);

        // socket_.async_write_some(boost::asio::buffer((void*)&safe_response->request, sizeof(client_request_e)),
        //     [](beast::error_code ec, std::size_t bytes_written) {}
        // );//sending req_type
        // socket_.async_write_some(boost::asio::buffer((void*)&safe_response->responce, sizeof(server_responce_e)),
        //     [](beast::error_code ec, std::size_t bytes_written) {}
        // );//sending resp_type
        // auto size = std::make_shared<size_t>(safe_response->message_text.size());
        // socket_.async_write_some(boost::asio::buffer((void*)size.get(), sizeof(size_t)),
        //     [](beast::error_code ec, std::size_t bytes_written) {}
        // );//sending size of message
        // socket_.async_write_some(boost::asio::buffer((void*)safe_response->message_text.data(), safe_response->message_text.size()),
        //     [self](beast::error_code ec, std::size_t bytes_written) {
        //         self->OnWrite(ec, bytes_written);
        //     }
        // );//sending the message
    }

private:
    boost::asio::ip::tcp::socket socket_;
    client_data_t request_;
    RequestHandlerBase<std::shared_ptr<Session>>* request_handler_;

    unblocked_read_state read_state = REQ_TYPE;
    //size_t off = 0; // сдвиг внутри элемента request
    size_t read_message_length, write_message_length;
    client_state state_ = client_state::no_name;


    std::shared_ptr<Session> GetSharedThis() {
        return this->shared_from_this();
    }

    void Read(){
        //if data was received, then the real Read is called
        // socket_.async_wait(tcp::socket::wait_read, 
        //     boost::bind(&Session::Read, GetSharedThis(), boost::asio::placeholders::error)
        // );
        
        boost::asio::async_read(socket_, boost::array<boost::asio::mutable_buffer, 2>({
            boost::asio::buffer((void*)&request_.request, sizeof(request_.request)),
            boost::asio::buffer((void*)&read_message_length, sizeof(read_message_length))
        }),
        boost::asio::transfer_all(), beast::bind_front_handler(&Session::OnRead, GetSharedThis()));
    }

    void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) { 
        using namespace std::literals;
        if(ec) return ReportError(ec, "read"sv);
        request_.message_text.resize(read_message_length);
        boost::asio::async_read(socket_, boost::asio::buffer(request_.message_text), boost::asio::transfer_all(), 
            [self = GetSharedThis()] (beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
                self->HandleRequest(std::move(self->request_));
            }
        );
    }

    void OnWrite(beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
        using namespace std::literals;
        if (ec) return ReportError(ec, "write"sv);
        // Считываем следующий запрос
        Read();
    }

    void HandleRequest(client_data_t && request) {
        request_handler_->handle(std::move(request), state_, GetSharedThis());
    }

};

struct Chat{
    Chat(net::io_context& io, const std::filesystem::path& chat_path) : strand(net::make_strand(io)), path(chat_path){}
    // Chat() = delete; //

    void AddUser(Session* session_ptr){
        net::dispatch(strand, [&session_ptr, this]{

            messages_offset.insert({session_ptr, 0});

            std::fstream chat_file(path);
            chat_file.seekp(0, chat_file.end);
            auto file_size = chat_file.tellp();

            server_data_t resp;
            resp.responce = server_responce_e::s_success;

            std::streamoff off = 0;
            chat_file.seekg(off, chat_file.beg);

            while (off != file_size) {
                resp.request = client_request_e::c_receive_message; // now we change the request that we are serving because we did connect to chat
                std::getline(chat_file, resp.message_text, '\r');
                off = chat_file.tellp();
                session_ptr->Write(std::move(std::vector{resp})); // invoking saved method to send all unread chat messages to connected user 
            }

            messages_offset[session_ptr] = off;
        });
    }

    void DeleteUser(Session* session_ptr){
        net::dispatch(strand, [&session_ptr, this](){
            messages_offset.erase(session_ptr);
        });
    }

    void SendMessage(Session* session_ptr, std::string message){
        //
    }

    net::strand<net::io_context::executor_type> strand;
    std::unordered_map<Session*, std::streamoff> messages_offset; // 
    std::filesystem::path path;
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

    bool SetName(const std::string& name, Session* session_ptr){
        bool result;
        net::dispatch(usernames_strand_, [this, &name, &result, &session_ptr]{
            if(users_.contains(session_ptr)) result = false;
            else{
                users_.insert({session_ptr, {name, ""}});
                result = true;
            }
        });
        return result;
    }

    std::string ChatList(){
        std::string ans;
        net::dispatch(chats_strand_, [chats = &chats_, &ans]{
            for(auto chat : *chats) ans += chat.first + "\n";
        });
        return ans;
    }

    void Disconnect(Session* session_ptr){
        net::dispatch(chats_strand_, [&session_ptr, this]{
            if(User* usr = IdentifyUser(session_ptr)){
                //synchronized deletion of the user listing in chat through chat's method
                if(chats_.contains(usr->chat_name)) {
                    Chat& cur_chat = chats_.at(usr->chat_name);
                    cur_chat.DeleteUser(session_ptr);
                }
                //synchronized deletion of the user from the list of users in the system
                net::dispatch(usernames_strand_, [&]{
                    users_.erase(session_ptr);
                });
            }
        });
    }

    void ConnectChat(const std::string& chat_name, Session* session_ptr){ //Sends everything the client needs, including success or failure notification
        using namespace std::literals;
        if(User* usr = IdentifyUser(session_ptr)){
            net::dispatch(chats_strand_, 
                [&chat_name, &session_ptr, &usr, this]{
                    server_data_t resp;
                    resp.message_text = "";
                    resp.request = client_request_e::c_connect_chat;

                    if(!chats_.contains(chat_name)) {
                        resp.responce = server_responce_e::s_failure;
                        session_ptr->Write(std::move(std::vector{resp})); // invoking saved method to inform client that the chat failed to connect
                        return;
                    } 
                    
                    resp.responce = server_responce_e::s_success;
                    session_ptr->Write(std::move(std::vector{resp})); // invoking saved method to inform client that the chat is connected RN

                    chats_.at(chat_name).AddUser(session_ptr);
                    
                    //synchronized update of user status
                    net::dispatch(usernames_strand_, [&chat_name, &session_ptr, this]{
                        if(users_.contains(session_ptr)) users_.at(session_ptr).chat_name = chat_name;
                     });

                }
            );
        }     
    }

    bool CreateChat(std::string&& name){
        bool result;
        net::dispatch(chats_strand_, 
            [&result, &name, this]{
                if(chats_.contains(name)){
                    result = false;
                    return;
                }
                chats_.insert({name, {io_, path_of_chats / (name + ".chat")}});
                result = true;
                return;
            }
        );
        return result;
    }

    bool LeaveChat(Session* session_ptr){
        bool result = false;
        if(User* usr = IdentifyUser(session_ptr)){
            net::dispatch(chats_strand_, 
                [&result, this, &usr, &session_ptr]{
                    if(!chats_.contains(usr->chat_name) || !chats_.at(usr->chat_name).messages_offset.contains(session_ptr)){
                        result = false;
                    }
                    else{
                        chats_.at(usr->chat_name).DeleteUser(session_ptr);
                        result = true;
                    }
                }
            );
        }
        return result;
    }

    void SendMessage(const std::string& message, Session* session_ptr){ // this one actually sends messages by itself
        User* usr = IdentifyUser(session_ptr);
        if(usr != nullptr && chats_.contains(usr->chat_name)){
            net::dispatch(chats_.at(usr->chat_name).strand, 
                [&usr, &message, &session_ptr, this]{
                    std::fstream chat_file(usr->chat_name + ".chat");

                    if(chats_.at(usr->chat_name).messages_offset.contains(session_ptr)){
                        chat_file << usr->name << "\t";
                        
                        time_t curr_time;
                        tm * curr_tm;
                        std::string timedate;
                        timedate.reserve(100);
                        time(&curr_time);
                        curr_tm = localtime(&curr_time);
                        strftime(timedate.data(), 99, "%T %D", curr_tm);

                        chat_file.write(timedate.c_str(), timedate.length());
                        chat_file << std::endl << message << "\r";
                    
                        auto file_size = chat_file.tellp();
                        for(auto& [usr_ptr, off] : chats_.at(usr->chat_name).messages_offset){
                            chat_file.seekg(off, chat_file.beg);
                            while(off != file_size){
                                server_data_t resp;
                                resp.request = client_request_e::c_receive_message;
                                std::getline(chat_file, resp.message_text, '\r');
                                off = chat_file.tellp();
                                usr_ptr->Write(std::move(std::vector{resp}));
                            }
                        }
                    }
                }
            );
        }
    } 
    
private:

    std::unordered_map<std::string, Chat> chats_; // чаты с юзернеймами
    std::unordered_map<Session*, User> users_; // мапа: метод отправки к самому юзеру
    net::io_context& io_;
    net::strand<net::io_context::executor_type> chats_strand_, usernames_strand_;
    std::filesystem::path path_of_chats;


    User* IdentifyUser(Session* session_ptr){
        User* usr;
        net::dispatch(usernames_strand_, [this, &usr, &session_ptr]{
            if(users_.contains(session_ptr)) usr = nullptr;
            else usr = &users_.at(session_ptr);
        });
        return usr;
    }
    
};

class RequestHandler : public server::RequestHandlerBase<std::shared_ptr<server::Session>>, std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(server::ChatManager&& new_manager) : chatManager(new_manager) {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    void handle(server::client_data_t&& req, server::client_state& state, std::shared_ptr<server::Session> shared_session_ptr) {
        // Обработать запрос request и отправить ответ, используя ptr нв сессию
        auto session_ptr = shared_session_ptr.get();
        server::server_data_t resp;
        resp.request = req.request;
        switch(req.request){
        case server::client_request_e::c_set_name:
            if(state != server::client_state::no_name){
                resp.message_text = "Cannot set name twice!";
                resp.responce = server::server_responce_e::s_failure;
                session_ptr->Write(std::move(std::vector{resp}));
            } else {
                if(chatManager.SetName(req.message_text.data(), session_ptr)){
                    state = server::client_state::list_chats;
                    resp.message_text = "";
                    resp.responce = server::server_responce_e::s_success;

                    resp.message_text = chatManager.ChatList();
                    server::server_data_t second_resp;
                    second_resp.request = server::client_request_e::c_get_chats;
                    second_resp.responce = server::server_responce_e::s_success;
                    second_resp.message_text = chatManager.ChatList();
                    
                    session_ptr->Write(std::move(std::vector{resp, second_resp}));
                    // then we send available chats!
                    // resp.responce = server::server_responce_e::s_success;
                    // session_ptr->Write(std::move({resp}));
                } else {
                    resp.message_text = "Username is already in use";
                    resp.responce = server::server_responce_e::s_failure;
                    session_ptr->Write(std::move(std::vector{resp}));
                }
            }
            break;
        case server::client_request_e::c_create_chat:
            if(state != server::client_state::list_chats){
                resp.message_text = "Cannot create chats, wrong state";
                resp.responce = server::server_responce_e::s_failure;
            }
            else{
                if(!chatManager.CreateChat(req.message_text.data())){
                    resp.message_text = "File with such name already exists";
                    resp.responce = server::server_responce_e::s_failure;
                }
                else{
                    resp.message_text = chatManager.ChatList();
                    resp.responce = server::server_responce_e::s_success;
                    // then send everyone out of chat new list

                }
            }
            session_ptr->Write(std::move(std::vector{resp}));
            break;
        case server::client_request_e::c_connect_chat:
            if(state != server::client_state::list_chats){
                resp.message_text = "Cannot connect chat, wrong state!";
                resp.responce = server::server_responce_e::s_failure;
            }
            else {
                chatManager.ConnectChat(req.message_text.data(), session_ptr);
                state = server::client_state::in_chat;
            }
            break;
        case server::client_request_e::c_send_message:
            if(state != server::client_state::in_chat){
                resp.message_text = "";
                resp.responce = server::server_responce_e::s_failure;
            }
            else chatManager.SendMessage(req.message_text.data(), session_ptr);
            break;
        case server::client_request_e::c_leave_chat:
            if(state == server::client_state::in_chat && chatManager.LeaveChat(session_ptr)){
                // Can leave chat, ok state && could to leave the chat
                state = server::client_state::in_chat;
            }
            //no messages about leaving
            break;
        case server::client_request_e::c_disconnect:
            chatManager.Disconnect(session_ptr); //throws exception if currentChat does not exist ?????????
            break;
        // case server::client_request_e::c_receive_message:
        //     break;
        // case server::client_request_e::c_get_chats:
        //     break;
        default:
            resp.message_text = "Wrong request";
            resp.request = server::client_request_e::c_wrong_request;
            resp.responce = server::server_responce_e::s_failure;
            session_ptr->Write(std::move(std::vector{resp}));
        }
    }
private:
    server::ChatManager chatManager;
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

    void Run(){
        DoAccept();
    }
    
private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler handler_;

    std::shared_ptr<Server> GetSharedThis(){
        return this->shared_from_this();
    }

    void DoAccept() {
        acceptor_.async_accept(
            // Передаём последовательный исполнитель, в котором будут вызываться обработчики
            // асинхронных операций сокета
            net::make_strand(ioc_),
            // При помощи bind_front_handler создаём обработчик, привязанный к методу OnAccept
            // текущего объекта.
            beast::bind_front_handler(&Server::OnAccept, GetSharedThis()));
    }

    // Метод socket::async_accept создаст сокет и передаст его передан в OnAccept
    void OnAccept(sys::error_code ec, tcp::socket socket) {
        using namespace std::literals;

        if (ec) {
            return ReportError(ec, "accept"sv);
        }

        // Для того, чтобы подключение произошло
        // нужно прочитать сначала данные для подтверждения подключения
        

        // Асинхронно обрабатываем сессию
        AsyncRunSession(std::move(socket));

        // Принимаем новое соединение
        DoAccept();
    }

    void AsyncRunSession(tcp::socket&& socket) {
        std::make_shared<Session>(std::move(socket), &handler_)->Run();
    }
};

}  // namespace server
