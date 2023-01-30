#pragma once
#include "server.h"
#include <iostream>

namespace handler {
namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;

template <typename Send>
class RequestHandler {
public:
    explicit RequestHandler(server::ChatManager<Send>&& new_manager) : chatManager(new_manager) {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    void operator()(server::client_data_t&& req, server::client_state&& state, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send
        server::server_data_t resp;
        resp.request = req.request;
        switch(req.request){
        case server::client_request_e::c_set_name:
            if(state != server::client_state::no_name){
                resp.message_text = "Cannot set name twice!";
                resp.responce = server::server_responce_e::s_failure;
                send(std::move(resp));
            }
            else{
                if(chatManager.SetName(req.message_text)){
                    userName = req.message_text;
                    resp.message_text = "";
                    resp.responce = server::server_responce_e::s_success;
                    send(std::move(resp));
                    std::cout << "NAME SET" << std::endl;
                    // then we send available chats!
                    resp.message_text = chatManager.ChatList();
                    resp.responce = server::server_responce_e::s_success;
                    send(std::move(resp));
                }
                else{
                    resp.message_text = "Username is already in use";
                    resp.responce = server::server_responce_e::s_failure;
                    send(std::move(resp));
                }
            }
            break;
        case server::client_request_e::c_create_chat:
            if(state != server::client_state::list_chats){
                resp.message_text = "Cannot create chats, wrong state";
                resp.responce = server::server_responce_e::s_failure;
            }
            else{
                if(!chatManager.CreateChat(req.message_text)){
                    resp.message_text = "File with such name already exists";
                    resp.responce = server::server_responce_e::s_failure;
                }
                else{
                    resp.message_text = "";
                    resp.responce = server::server_responce_e::s_success;
                }
            }
            send(std::move(resp));
            break;
        case server::client_request_e::c_connect_chat:
            if(state != server::client_state::list_chats){
                resp.message_text = "Cannot connect chat, wrong state!";
                resp.responce = server::server_responce_e::s_failure;
            }
            else chatManager.ConnectChat(req.message_text, userName, send);
            break;
        case server::client_request_e::c_send_message:
            if(state != server::client_state::in_chat){
                resp.message_text = "";
                resp.responce = server::server_responce_e::s_failure;
            }
            else chatManager.SendMessage(currentChat, req.message_text, userName);
            break;
        case server::client_request_e::c_leave_chat:
            if(state != server::client_state::in_chat){
                resp.message_text = "Cannot leave chat, wrong state!";
                resp.responce = server::server_responce_e::s_failure;
            }
            else if(chatManager.LeaveChat(currentChat, userName)){
                resp.message_text = "Failed to leave the chat";
                resp.responce = server::server_responce_e::s_failure;
            }
            else {
                resp.message_text = "";
                resp.responce = server::server_responce_e::s_success;
                currentChat = "";
            }
            send(std::move(resp));
            break;
        case server::client_request_e::c_disconnect:
            chatManager.Disconnect(userName, currentChat); //throws exception if currentChat does not exist
            break;
        // case server::client_request_e::c_receive_message:
        //     break;
        // case server::client_request_e::c_get_chats:
        //     break;
        default:
            resp.message_text = "Wrong request";
            resp.request = server::client_request_e::c_wrong_request;
            resp.responce = server::server_responce_e::s_failure;
            send(std::move(resp));
        }
    }
private:
    server::ChatManager<Send> chatManager;
    std::string currentChat = "";
    std::string userName;
};

}  // namespace handler
