#pragma once
#include "server.h"
#include <iostream>

namespace handler {
namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;

class RequestHandler {
public:
    explicit RequestHandler() {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Send>
    void operator()(server::client_data_t req, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send
        server::server_data_t resp;
        
        send(resp);
    }
};

}  // namespace http_handler
