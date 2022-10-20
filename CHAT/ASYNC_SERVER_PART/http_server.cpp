#include "http_server.h"
#include <iostream>

namespace http_server {

// Разместите здесь реализацию http-сервера, взяв её из задания по разработке асинхронного сервера
void ReportError(beast::error_code ec, std::string_view what) {
    using namespace std::literals;
    std::cerr << what << ": "sv << ec.message() << std::endl;
}

void SessionBase::Run() {
    // Вызываем метод Read, используя executor объекта stream_.
    // Таким образом вся работа со stream_ будет выполняться, используя его executor
    net::dispatch(stream_.get_executor(),
                beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

template <typename Body, typename Fields>
void SessionBase::Write(http::response<Body, Fields>&& response) {
    // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
    auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));

    auto self = GetSharedThis();
    http::async_write(stream_, *safe_response,
                        [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                            self->OnWrite(safe_response->need_eof(), ec, bytes_written);
                        });
}

void SessionBase::Read()  {
    using namespace std::literals;
    // Очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
    request_ = {};
    stream_.expires_after(30s);
    // Считываем request_ из stream_, используя buffer_ для хранения считанных данных
    http::async_read(stream_, buffer_, request_,
                        // По окончании операции будет вызван метод OnRead
                        beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        // Нормальная ситуация - клиент закрыл соединение
        return Close();
    }
    if (ec) {
        return ReportError(ec, "read"sv);
    }
    HandleRequest(std::move(request_));
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    using namespace std::literals;
    if (ec) {
        return ReportError(ec, "write"sv);
    }

    if (close) {
        // Семантика ответа требует закрыть соединение
        return Close();
    }

    // Считываем следующий запрос
    Read();
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

void Session::HandleRequest(HttpRequest&& request) override {
    // Захватываем умный указатель на текущий объект Session в лямбде,
    // чтобы продлить время жизни сессии до вызова лямбды.
    // Используется generic-лямбда функция, способная принять response произвольного типа
    request_handler_(std::move(request), [self = this->shared_from_this()](auto&& response) {
        self->Write(std::move(response));
    });
}

Listener::Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler)
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

void Listener::DoAccept() {
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

void Listener::OnAccept(sys::error_code ec, tcp::socket socket) {
    using namespace std::literals;

    if (ec) {
        return ReportError(ec, "accept"sv);
    }

    // Асинхронно обрабатываем сессию
    AsyncRunSession(std::move(socket));

    // Принимаем новое соединение
    DoAccept();
}

void Listener::AsyncRunSession(tcp::socket&& socket) {
    std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
}

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    // При помощи decay_t исключим ссылки из типа RequestHandler,
    // чтобы Listener хранил RequestHandler по значению
    using MyListener = Listener<std::decay_t<RequestHandler>>;

    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace http_server
