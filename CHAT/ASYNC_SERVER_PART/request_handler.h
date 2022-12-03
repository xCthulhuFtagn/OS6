#pragma once
#include "server.h"
#include <iostream>

namespace handler {
namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;

enum class client_state{
    no_name = 0,
    list_chats,
    in_chat
};

class RequestHandler {
public:
    explicit RequestHandler() {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Send>
    void operator()(server::client_data_t req, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send
        server::server_data_t resp;
        switch(req.request){
        case server::client_request_e::c_set_name:
            if(state != client_state::no_name){
                //error
                resp.message_text = "";
                resp.responce = server::server_responce_e::s_failure;
            }
            else{
                //
            }
            break;
        case server::client_request_e::c_create_chat:
            if(state != client_state::list_chats){
                resp.message_text = "";
                resp.responce = server::server_responce_e::s_failure;
            }
            else{

            }
            break;
        case server::client_request_e::c_connect_chat:
            if(state != client_state::list_chats){
                resp.message_text = "";
                resp.responce = server::server_responce_e::s_failure;
            }
            else{

            }
            break;
        case server::client_request_e::c_send_message:
            if(state != client_state::in_chat){
                resp.message_text = "";
                resp.responce = server::server_responce_e::s_failure;
            }
            else{

            }
            break;
        case server::client_request_e::c_leave_chat:
            if(state != client_state::in_chat){
                resp.message_text = "";
                resp.responce = server::server_responce_e::s_failure;
            }
            else{

            }
            break;
        case server::client_request_e::c_disconnect:
            break;
        case server::client_request_e::c_receive_message:
            break;
        case server::client_request_e::c_wrong_request:
            break;
        case server::client_request_e::c_get_chats:
            break;
        default:
            resp.message_text = "";
            resp.responce = server::server_responce_e::s_failure;
        }
        resp.request = req.request;
        send(resp);
    }
private:
    client_state state = client_state::no_name;
};

}  // namespace handler
