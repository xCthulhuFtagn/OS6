#include "server.h"
#include <iostream>

namespace server {

// Разместите здесь реализацию http-сервера, взяв её из задания по разработке асинхронного сервера
void ReportError(beast::error_code ec, std::string_view what) {
    using namespace std::literals;
    std::cerr << what << ": "sv << ec.message() << std::endl;
}

void SessionBase::Run() {
    // Вызываем метод Read, используя executor объекта socket_.
    // Таким образом вся работа со socket_ будет выполняться, используя его executor
    net::dispatch(socket_.get_executor(),
                beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Write(server_data_t&& response) {
    // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
    auto safe_response = std::make_shared<server_data_t>(std::move(response));

    auto self = GetSharedThis();
    socket_.async_write_some(boost::asio::buffer((void*)safe_response.get(), sizeof(server_data_t)),
                        [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                            self->OnWrite(ec, bytes_written);
                        });
}

void SessionBase::Read()  {
    using namespace std::literals;
    // Очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
    request_ = {};
    // socket_.expires_after(30s);
    // Считываем request_ из socket_, используя buffer_ для хранения считанных данных
    socket_.async_read_some(boost::asio::buffer(&request_, sizeof(client_data_t)),
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

void SessionBase::OnWrite(beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    using namespace std::literals;
    if (ec) {
        return ReportError(ec, "write"sv);
    }

    // if (close) {
    //     // Семантика ответа требует закрыть соединение
    //     return Close();
    // }

    // Считываем следующий запрос
    Read();
}

void SessionBase::Close() {
    beast::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_send, ec);
}

}  // namespace server
