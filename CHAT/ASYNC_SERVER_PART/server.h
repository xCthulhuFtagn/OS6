#pragma once

#include <ios>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <filesystem>
//
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace server {
// Разместите здесь реализацию http-сервера, взяв её из задания по разработке асинхронного сервера

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

enum class client_request_e {
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

class SessionBase {
public: 
    // Запрещаем копирование и присваивание объектов SessionBase и его наследников
    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;

    void Run();
protected:
    using HttpRequest = http::request<http::string_body>;
    explicit SessionBase(tcp::socket&& socket)
    : socket_(std::move(socket)) {}

    void Write(server_data_t&& response);
private:

    void Read();
    void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read);
    void OnWrite(beast::error_code ec, [[maybe_unused]] std::size_t bytes_written);
    void Close();

    // Обработку запроса делегируем подклассу
    virtual void HandleRequest(client_data_t&& request) = 0;

    virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;

    boost::asio::ip::tcp::socket socket_;
    client_data_t request_;
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

    void HandleRequest(client_data_t && request) override {
        // Захватываем умный указатель на текущий объект Session в лямбде,
        // чтобы продлить время жизни сессии до вызова лямбды.
        // Используется generic-лямбда функция, способная принять response произвольного типа
        request_handler_(std::move(request), [self = this->shared_from_this()](auto&& response) {
            self->Write(std::move(response));
        });
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
    std::unordered_map<std::string, tcp::socket*> users;
    std::unordered_map<std::string, std::streamoff> messages_offset;
};

class ChatManager{
public:
    ChatManager(net::io_context&, std::string);

    bool ConnectChat(const std::string&, const std::string&, tcp::socket*);
    bool CreateChat(const std::string&);
    bool LeaveChat(const std::string&, const std::string&);
    void SendMessage(const std::string&, const std::string&, const std::string&);
    
private:
    void OnMessageSent(boost::system::error_code&, std::size_t);

    std::unordered_map<std::string, Chat> chats_; // последовательное обращение к отдельным элементам
    net::io_context& io_;
    net::strand<net::io_context::executor_type> chats_strand_, usernames_strand_;
    std::filesystem::path path_of_chats;
};

template <typename RequestHandler>
void ServeRequest(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    // При помощи decay_t исключим ссылки из типа RequestHandler,
    // чтобы Listener хранил RequestHandler по значению
    using MyListener = Listener<std::decay_t<RequestHandler>>;

    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace server
