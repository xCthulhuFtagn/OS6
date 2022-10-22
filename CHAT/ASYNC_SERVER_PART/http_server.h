#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
//
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace http_server {
// Разместите здесь реализацию http-сервера, взяв её из задания по разработке асинхронного сервера

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

typedef enum {
    c_set_name = 0,
    c_create_chat,
    c_connect_chat,
    c_send_message,
    c_leave_chat,
    c_disconnect,
    c_receive_message,
    c_wrong_request,
    c_get_chats
} client_request_e;

typedef enum {
    s_success = 0,
    s_failure,
    // s_new_message
} server_responce_e;

typedef struct {
    client_request_e request;
    std::string message_text;
} client_data_t;

typedef struct {
    client_request_e request;
    server_responce_e responce;
    std::string message_text;
} server_data_t;

void ReportError(beast::error_code ec, std::string_view what);

class SessionBase {
public: 
    // Запрещаем копирование и присваивание объектов SessionBase и его наследников
    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;

    void Run();
protected:
    using HttpRequest = http::request<http::string_body>;
    explicit SessionBase(tcp::socket&& socket)
    : stream_(std::move(socket)) {}

    template <typename Body, typename Fields>
    void Write(http::response<Body, Fields>&& response);
private:

    void Read();
    void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read);
    void OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written);
    void Close();

    // Обработку запроса делегируем подклассу
    virtual void HandleRequest(HttpRequest&& request) = 0;

    virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;

    // tcp_stream содержит внутри себя сокет и добавляет поддержку таймаутов
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    HttpRequest request_;
};

template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
public:
   template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler)
        : SessionBase(std::move(socket))
        , request_handler_(std::forward<Handler>(request_handler)) {
    }

private:
    std::shared_ptr<SessionBase> GetSharedThis() override {
        return this->shared_from_this();
    }

    void HandleRequest(HttpRequest&& request) override;

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
        , request_handler_(std::forward<Handler>(request_handler));

    void Run(){
        DoAccept();
    }
private:
    void DoAccept();

    // Метод socket::async_accept создаст сокет и передаст его передан в OnAccept
    void OnAccept(sys::error_code ec, tcp::socket socket);

    void AsyncRunSession(tcp::socket&& socket);

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;
};

class Chat :: enable_shared_from_this<Chat>{
public:
    Chat(std::string name, net::io_context& io) : chat_name(name), io_(io){}
    void Enter();
    void SendMessage();
    void Leave();

private:
    void OnEnter();
    void OnLeave();
    std::vector<tcp::socket> subs_;
    std::string name_;
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_{net::make_strand(io_)};
};

class ChatManager{
public:
    ChatManager(net::io_context&); 

    std::shared_ptr<Chat> ConnectChat(std::string);
    std::shared_ptr<Chat> CreateChat(std::string);
    bool LeaveChat(std::string);
    
private:
    std::unordered_map<std::string, std::weak_ptr<Chat>> chats_; // последовательное обращение к отдельным элементам (weak_ptr::expired?)
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_{net::make_strand(io_)};
};

template <typename RequestHandler>
void ServeRequest(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler);

}  // namespace http_server
