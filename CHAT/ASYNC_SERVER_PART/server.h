#pragma once

#include <ios>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <filesystem>
#include <functional>
#include <any>
//
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/bind.hpp>

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
    std::string message_text;
} client_data_t;

class server_data_t : public std::enable_shared_from_this<server_data_t>{
public:
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

template <typename RequestHandler>
class Session : public std::enable_shared_from_this<Session<RequestHandler>> {
public:
   template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler)
        : socket_(std::move(socket))
        , request_handler_(std::forward<Handler>(request_handler)) {
            socket_.non_blocking(true);
    }

    // ~Session(){
    //   std::cout << "SUSSUS AMOGUS" << std::endl;
    // }

    void Run() {
        // Вызываем метод Read, используя executor объекта socket_.
        // Таким образом вся работа со socket_ будет выполняться, используя его executor
        net::dispatch(socket_.get_executor(),
                    beast::bind_front_handler(&Session::TryRead, GetSharedThis()));
    }

private:
    boost::asio::ip::tcp::socket socket_;
    client_data_t request_;

    unblocked_read_state read_state = REQ_TYPE;
    size_t off = 0; // сдвиг внутри элемента request
    size_t message_length;
    client_state state_ = client_state::no_name;

    std::shared_ptr<Session> GetSharedThis() {
        return this->shared_from_this();
    }

    void TryRead(){
        //if data was received, then the real Read is called
        socket_.async_wait(tcp::socket::wait_read, 
            boost::bind(&Session::Read, GetSharedThis(), boost::asio::placeholders::error)
            //if TryRead is called second time (from Base class) -> virtual function is called -> bruh
        );
    }

    void Read(beast::error_code ec){
        // std::cout << "I am reading!" << std::endl;
        using namespace std::literals;
        if(ec){
            socket_.close(); // if an error occured, then the socket should be closed
            return ReportError(ec, "read"sv);
        }

        switch(read_state){
            case REQ_TYPE:
                request_ = {};
                // Чистим request и читаем тип запроса
                socket_.async_read_some(boost::asio::buffer(&request_.request + off, sizeof(client_request_e) - off),
                    beast::bind_front_handler(&Session::OnRead, GetSharedThis()));
                break;
            case LENGTH:
                // Читаем длину строки сообщения
                socket_.async_read_some(boost::asio::buffer(&message_length + off, sizeof(size_t) - off),
                    beast::bind_front_handler(&Session::OnRead, GetSharedThis()));
                break;
            case MESSAGE:
                // Читаем само сообщение
                socket_.async_read_some(boost::asio::buffer(request_.message_text.data() + off, message_length - off),
                    beast::bind_front_handler(&Session::OnRead, GetSharedThis()));
                break;
        }
    }

    void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) { 
        using namespace std::literals;
        // if (ec == http::error::end_of_stream) {
        //     // Нормальная ситуация - клиент закрыл соединение
        //     return Close();
        // }
        if (ec) {
            socket_.close();
            return ReportError(ec, "read"sv);
        }

        off += bytes_read;
        switch(read_state){
            case REQ_TYPE:
                if(off == sizeof(client_request_e)){
                    read_state = LENGTH;
                    off = 0;
                }
                TryRead();
                break;
            case LENGTH:
                if(off == sizeof(size_t)){
                    read_state = MESSAGE;
                    off = 0;
                }
                TryRead();
                break;
            case MESSAGE:
            if(off == message_length){
                    HandleRequest(std::move(request_));
                    read_state = REQ_TYPE;
                    off = 0;
                }
        }
    }

    void Write(server_data_t&& response) {
        // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
        auto safe_response = std::make_shared<server_data_t>(std::move(response));
        auto self = GetSharedThis();
        socket_.async_write_some(boost::asio::buffer((void*)&safe_response->request, sizeof(client_request_e)),
            [self](beast::error_code ec, std::size_t bytes_written) {
                // self->OnWrite(ec, bytes_written);
            }
        );//sending req_type
        socket_.async_write_some(boost::asio::buffer((void*)&safe_response->responce, sizeof(server_responce_e)),
            [self](beast::error_code ec, std::size_t bytes_written) {
                // self->OnWrite(ec, bytes_written);
            }
        );//sending resp_type
        auto size = std::make_shared<size_t>(safe_response->message_text.size());
        socket_.async_write_some(boost::asio::buffer((void*)size.get(), sizeof(size_t)),
            [self](beast::error_code ec, std::size_t bytes_written) {
                // self->OnWrite(ec, bytes_written);
            }
        );//sending size of message
        socket_.async_write_some(boost::asio::buffer((void*)safe_response->message_text.data(), safe_response->message_text.size()),
            [self](beast::error_code ec, std::size_t bytes_written) {
                self->OnWrite(ec, bytes_written);
            }
        );//sending the message
    }

    void OnWrite(beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
        using namespace std::literals;
        if (ec) return ReportError(ec, "write"sv);

        // Считываем следующий запрос
        TryRead();
    }

    void HandleRequest(client_data_t && request) {
        // Захватываем умный указатель на текущий объект Session в лямбде,
        // чтобы продлить время жизни сессии до вызова лямбды.
        // Используется generic-лямбда функция, способная принять response произвольного типа
        request_handler_(std::move(request), std::move(state_), 
            [self = this->shared_from_this()](auto&& response) {
                self->Write(std::move(response));
            }
        );
    }

    RequestHandler request_handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    template <typename Handler>
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler)
        : ioc_(ioc)
        // Обработчики асинхронных операций acceptor_ будут вызываться в своём strand
        , acceptor_(net::make_strand(ioc))
        , request_handler_(std::forward<Handler>(request_handler)) {
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
    void DoAccept() {
        acceptor_.async_accept(
            // Передаём последовательный исполнитель, в котором будут вызываться обработчики
            // асинхронных операций сокета
            net::make_strand(ioc_),
            // При помощи bind_front_handler создаём обработчик, привязанный к методу OnAccept
            // текущего объекта.
            // Так как Listener — шаблонный класс, нужно подсказать компилятору, что
            // shared_from_this — метод класса, а не свободная функция.
            // Для этого вызываем его, используя this
            // Этот вызов bind_front_handler аналогичен
            // namespace ph = std::placeholders;
            // std::bind(&Listener::OnAccept, this->shared_from_this(), ph::_1, ph::_2)
            beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
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
        std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;
};

struct Chat{
    Chat(net::io_context& io) : strand(net::make_strand(io)){}
    Chat() = delete;
    net::strand<net::io_context::executor_type> strand;
    std::unordered_map<std::string, std::any> users;
    std::unordered_map<std::string, std::streamoff> messages_offset;
};

template <typename Send>
class ChatManager{
public:
    ChatManager(net::io_context& io, std::string path) : io_(io), chats_strand_(net::make_strand(io)), usernames_strand_(net::make_strand(io)), path_of_chats(path.data()){
        namespace fs = std::filesystem;
        if(!fs::exists(path_of_chats) || !fs::is_directory(path_of_chats)) fs::create_directory(path_of_chats);
        for (const auto& entry : fs::directory_iterator(path_of_chats)) {
            if (entry.is_regular_file() && fs::path(entry).extension() == ".chat") {
                Chat new_chat(io);
                chats_.insert({fs::path(entry).stem().string(), new_chat});
            }
        }
    }

    bool SetName(const std::string& name){
        bool result;
        net::dispatch(usernames_strand_, [usedUsernames = &used_usernames, &name, &result]{
            if(usedUsernames->contains(name)) result = false;
            else{
                usedUsernames->insert(name);
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

    void Disconnect(const std::string& name, std::string chat_name = ""){
        net::dispatch(chats_strand_, [chats = &chats_, users = &used_usernames, &name, &chat_name]{
            if(!chat_name.empty()){
                net::dispatch(chats->at(chat_name).strand, [&chats, &name, &chat_name]{
                    chats->at(chat_name).messages_offset.erase(name);
                    chats->at(chat_name).users.erase(name);
                });
            }
            users->erase(name);
        });

    }

    void ConnectChat(const std::string& chat_name, const std::string& user_name, Send& send){ //Sends everything the client needs, including success or failure notification
        net::dispatch(chats_strand_, 
            [&chat_name, &user_name, chats = &chats_, &send, this]{
                server_data_t resp;
                resp.message_text = "";
                resp.request = client_request_e::c_connect_chat;

                if(!chats->contains(chat_name) || (*chats).at(chat_name).users.contains(user_name)) {
                    resp.responce = server_responce_e::s_failure;
                    std::any_cast<Send>(chats->at(chat_name).users[user_name])(std::move(resp)); // invoking saved method to inform client that the chat failed to connect
                    return;
                } 
                
                (*chats).at(chat_name).users[user_name] = send;

                resp.responce = server_responce_e::s_success;
                std::any_cast<Send>(chats->at(chat_name).users[user_name])(std::move(resp)); // invoking saved method to inform client that the chat is connected RN

                std::fstream chat_file(chat_name + ".chat");
                chat_file.seekp(0, chat_file.end);
                auto file_size = chat_file.tellp();

                std::streamoff off = 0;
                chat_file.seekg(off, chat_file.beg);

                while (off != file_size) {
                    resp.request = client_request_e::c_receive_message; // now we change the request that we are serving because we did connect to chat
                    std::getline(chat_file, resp.message_text, '\r');
                    off = chat_file.tellp();
                    std::any_cast<Send>(chats->at(chat_name).users[user_name])(std::move(resp)); // invoking saved method to send all unread chat messages to connected user 
                }
                (*chats).at(chat_name).messages_offset[user_name] = off;
            }
        );        
    }

    bool CreateChat(const std::string& name){
        bool result;
        net::dispatch(chats_strand_, 
            [&result, &name, chats = &chats_, io = &io_]{
                if(chats->contains(name)){
                    result = false;
                    return;
                }
                Chat new_chat(*io);
                chats->insert({name, new_chat});
                result = true;
                return;
            }
        );
        return result;
    }

    bool LeaveChat(const std::string& chat_name, const std::string& user_name){
        bool result;
        net::dispatch(chats_strand_, 
            [&result, &chat_name, chats = &chats_, &user_name]{
                if(!chats->contains(chat_name) || !chats->at(chat_name).users.contains(user_name)){
                    result = false;
                    return;
                }

                chats->at(chat_name).users.erase(user_name);
                chats->at(chat_name).messages_offset.erase(user_name);
                result = true;
                return;
            }
        );
        return result;
    }

    void SendMessage(const std::string& chat_name, const std::string& message, const std::string& user_name){ // this one actually sends messages by itself
        net::dispatch(chats_.at(chat_name).strand, 
            [&chat_name, &message, &user_name, chats = &chats_, this]{
                if(chats->contains(chat_name)){
                    std::fstream chat_file(chat_name + ".chat");

                    if(chats->at(chat_name).users.contains(user_name)){

                        chat_file << user_name << "\t";
                        
                        time_t curr_time;
                        tm * curr_tm;
                        std::string timedate;
                        timedate.reserve(100);
                        time(&curr_time);
                        curr_tm = localtime(&curr_time);
                        strftime(timedate.data(), 99, "%T %D", curr_tm);

                        chat_file.write(timedate.c_str(), timedate.length());
                        chat_file << std::endl << message << "\r";

                    }
                    auto file_size = chat_file.tellp();
                    for(auto& [name, off] : chats->at(chat_name).messages_offset){
                        chat_file.seekg(off, chat_file.beg);
                        while(off != file_size){
                            server_data_t resp;
                            resp.request = client_request_e::c_receive_message;
                            std::getline(chat_file, resp.message_text, '\r');
                            off = chat_file.tellp();
                            std::any_cast<Send>(chats->at(chat_name).users[name])(std::move(resp));
                        }
                    }
                }

            }
        );
    }
    
private:

    std::unordered_map<std::string, Chat> chats_; // чаты с соответствующими объектам сессий функциями отправки, ппц (мб поработать над не напрямую прописанными типами функции)
    net::io_context& io_;
    net::strand<net::io_context::executor_type> chats_strand_, usernames_strand_;
    std::filesystem::path path_of_chats;
    std::unordered_set<std::string> used_usernames;
};

template <typename RequestHandler>
void ServeRequest(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    // При помощи decay_t исключим ссылки из типа RequestHandler,
    // чтобы Listener хранил RequestHandler по значению
    using MyListener = Listener<std::decay_t<RequestHandler>>;

    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace server
