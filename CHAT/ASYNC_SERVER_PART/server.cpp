#include "server.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <ctime>

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
                beast::bind_front_handler(&SessionBase::ReadFirst, GetSharedThis()));
}

void SessionBase::Write(server_data_t&& response) {
    // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
    auto safe_response = std::make_shared<server_data_t>(std::move(response));

    auto self = GetSharedThis();
    socket_.async_write_some(boost::asio::buffer((void*)&safe_response->request, sizeof(client_request_e)),
        [self](beast::error_code ec, std::size_t bytes_written) {
            self->OnWrite(ec, bytes_written);
        }
    );//sending req_type
    socket_.async_write_some(boost::asio::buffer((void*)&safe_response->responce, sizeof(server_responce_e)),
        [self](beast::error_code ec, std::size_t bytes_written) {
            self->OnWrite(ec, bytes_written);
        }
    );//sending resp_type
    auto size = std::make_shared<size_t>(safe_response->message_text.size());
    socket_.async_write_some(boost::asio::buffer((void*)size.get(), sizeof(size_t)),
        [self](beast::error_code ec, std::size_t bytes_written) {
            self->OnWrite(ec, bytes_written);
        }
    );//sending size of message
    socket_.async_write_some(boost::asio::buffer((void*)safe_response->message_text.data(), safe_response->message_text.size()),
        [self](beast::error_code ec, std::size_t bytes_written) {
            self->OnWrite(ec, bytes_written);
        }
    );
}

void SessionBase::ReadFirst()  {
    using namespace std::literals;
    // Очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
    request_ = {};

    auto self = GetSharedThis();

    //Читаем запрос
    socket_.async_read_some(boost::asio::buffer(&request_.request, sizeof(client_request_e)),
    // Привязываем обработчиком результата второй этап чтения
    beast::bind_front_handler(&SessionBase::ReadSecond, GetSharedThis()));
}

void SessionBase::ReadSecond(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read){
    if(bytes_read == 0){
        OnRead(boost::beast::error_code{}, 0);
    }
    else{
        size_t len;
        // Читаем длину строки сообщения
        socket_.async_read_some(boost::asio::buffer(&len, sizeof(size_t)),
            beast::bind_front_handler([&](beast::error_code ec, [[maybe_unused]] std::size_t bytes_read){
                request_.message_text.reserve(len);
                // Читаем саму строку сообщения
                socket_.async_read_some(boost::asio::buffer(request_.message_text.data(), len), beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
            })
        );
    }
}

// void SessionBase::ReadThird(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read){
//     request_.message_text.reserve(*len);
//     socket_.async_read_some(boost::asio::buffer(request_.message_text.data(), *len),
//     beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
// }

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) { 
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        // Нормальная ситуация - клиент закрыл соединение
        return Close();
    }
    if (ec) {
        return ReportError(ec, "read"sv);
    }
    if(bytes_read > 0) 
        HandleRequest(std::move(request_));
    else ReadFirst();
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
    ReadFirst();
}

void SessionBase::Close() {
    beast::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_send, ec);
}

}  // namespace server
