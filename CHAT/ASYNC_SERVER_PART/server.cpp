#include "server.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <ctime>

namespace server {

void ReportError(beast::error_code ec, std::string_view what) {
    using namespace std::literals;
    std::cout << what << ": "sv << ec.message() << std::endl;
}

// Sessiom methods

Session::~Session(){
    request_handler_->handleDisconnection(this);
    std::cout << "The session is down" << std::endl;
}

void Session::Run() {
    // Вызываем метод Read, используя executor объекта socket_.
    // Таким образом вся работа со socket_ будет выполняться, используя его executor
    net::dispatch(socket_.get_executor(), beast::bind_front_handler(&Session::Read, GetSharedThis()));
}

void Session::WriteKeepState(server_data_t && response){
    write_message_length = response.message_text.size();
    auto safe_response = std::make_shared<server_data_t>(std::move(response));
    boost::asio::async_write(socket_, 
        boost::array<boost::asio::mutable_buffer, 4>({
            boost::asio::buffer((void*)&safe_response->request, sizeof(client_request_e)),
            boost::asio::buffer((void*)&safe_response->responce, sizeof(server_responce_e)),
            boost::asio::buffer((void*)&write_message_length, sizeof(size_t)),
            boost::asio::buffer(safe_response->message_text)
        }),
        boost::asio::transfer_all(), 
        [self = GetSharedThis()](beast::error_code ec, std::size_t bytes_written) {}
    );
}

void Session::Write(server_data_t&& response) {
    write_message_length = response.message_text.size();
    auto safe_response = std::make_shared<server_data_t>(std::move(response));
    boost::asio::async_write(socket_, 
        boost::array<boost::asio::mutable_buffer, 4>({
            boost::asio::buffer((void*)&safe_response->request, sizeof(client_request_e)),
            boost::asio::buffer((void*)&safe_response->responce, sizeof(server_responce_e)),
            boost::asio::buffer((void*)&write_message_length, sizeof(size_t)),
            boost::asio::buffer(safe_response->message_text)
        }),
        boost::asio::transfer_all(), 
        [self = GetSharedThis(), safe_response](beast::error_code ec, std::size_t bytes_written) {
            self->OnWrite(ec, bytes_written);
        }
    );
}

std::shared_ptr<Session> Session::GetSharedThis() {
    return this->shared_from_this();
}

void Session::Read(){
    boost::asio::async_read(socket_, boost::array<boost::asio::mutable_buffer, 2>({
        boost::asio::buffer((void*)&request_.request, sizeof(request_.request)),
        boost::asio::buffer((void*)&read_message_length, sizeof(read_message_length))
    }),
    boost::asio::transfer_all(), beast::bind_front_handler(&Session::ReadMessage, GetSharedThis()));
}

void Session::ReadMessage(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) { 
    using namespace std::literals;
    if(ec) return ReportError(ec, "read"sv);
    request_.message_text.resize(read_message_length);
    boost::asio::async_read(socket_, boost::asio::buffer(request_.message_text), boost::asio::transfer_all(), 
        [self = GetSharedThis()] (beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
            self->HandleRequest(std::move(self->request_));
        }
    );
}

void Session::OnWrite(beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    using namespace std::literals;
    if (ec) return ReportError(ec, "write"sv);
    // Считываем следующий запрос
    Read();
}

void Session::HandleRequest(client_data_t && request) {
    request_handler_->handle(std::move(request), state_, GetSharedThis());
}

// Chat methods

void Chat::AddUser(Session* session_ptr, std::string username){
    net::dispatch(strand, [&session_ptr, &username, this]{

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
            if(off != file_size) session_ptr->WriteKeepState(std::move(resp));
            else session_ptr->Write(std::move(resp));
            // invoking saved method to send all unread chat messages to connected user
        }

        messages_offset[session_ptr] = off;
        usernames[session_ptr] = username;

    });
}

void Chat::DeleteUser(Session* session_ptr){
    net::dispatch(strand, [&session_ptr, this](){
        messages_offset.erase(session_ptr);
        usernames.erase(session_ptr);
    });
}

void Chat::SendMessage(std::string message, Session* session_ptr){
    net::dispatch(strand, [&message, session_ptr, this]{
        // std::fstream chat_file(usr->chat_name + ".chat");
        if(messages_offset.contains(session_ptr)){
            std::fstream chat_file(path);

            chat_file << usernames[session_ptr] << "\t";
            
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
            for(auto& [usr_ptr, off] : messages_offset){
                chat_file.seekg(off, chat_file.beg);
                while(off != file_size){
                    server_data_t resp;
                    resp.request = client_request_e::c_receive_message;
                    std::getline(chat_file, resp.message_text, '\r');
                    off = chat_file.tellp();
                    if(usr_ptr != session_ptr || off != file_size) usr_ptr->WriteKeepState(std::move(resp));
                    else usr_ptr->Write(std::move(resp));
                }
            }
        } 
    });       
}

bool Chat::ContainsUser(Session* session_ptr){
    bool result;
    net::dispatch(strand, [&session_ptr, &result, this]{
        result = messages_offset.count(session_ptr);
    });
    return result;
}

// ChatManager methods

bool ChatManager::SetName(std::string name, Session* session_ptr){
    bool result;
    net::dispatch(usernames_strand_, [this, &name, &result, session_ptr]{
        for(auto [user_ptr, user] : users_){
            if(user.name == name){
                result = false;
                return;
            }
        }
        users_.insert({session_ptr, {name, ""}});
        result = true;
    });

    if(result) std::cout << name << " was registered!\n";
    else std::cout << name << " could not be registered!\n";

    return result;
}

std::string ChatManager::ChatList(){
    std::string list;
    net::dispatch(chats_strand_, [this, &list]{
        for(auto chat : chats_) list += chat.first + "\n";
    });
    return list;
}

void ChatManager::UpdateChatList(Session* session_ptr){
    net::dispatch(usernames_strand_, [session_ptr, this]{
        server_data_t resp;
        std::string list = ChatList();
        for(auto [user_ptr, user] : users_){
            if(user.chat_name.empty()){
                resp.message_text = list;
                resp.request = c_get_chats;
                resp.responce = s_success;
                if(user_ptr != session_ptr) user_ptr->WriteKeepState(std::move(resp)); // non-initiators just receive data
                else user_ptr->Write(std::move(resp)); // chat creator changes state back to reading
            }
        }
    });
}

void ChatManager::Disconnect(Session* session_ptr){
    net::dispatch(chats_strand_, [&session_ptr, this]{
        //synchronized deletion of the user listing in chat through chat's method
        if(User* usr = IdentifyUser(session_ptr)){
            if(chats_.contains(usr->chat_name)) {
                chats_.at(usr->chat_name).DeleteUser(session_ptr); // throws exception because ???
            }
            //synchronized deletion of the user from the list of users in the chat manager
            net::dispatch(usernames_strand_, [&]{
                users_.erase(session_ptr);
            });
        }
    });
}

void ChatManager::ConnectChat(std::string chat_name, Session* session_ptr){ //Sends everything the client needs, including success or failure notification
    using namespace std::literals;
    if(User* usr = IdentifyUser(session_ptr)){
        net::dispatch(chats_strand_, 
            [&chat_name, &session_ptr, &usr, this]{
                server_data_t resp;
                resp.message_text = "";
                resp.request = client_request_e::c_connect_chat;

                if(!chats_.contains(chat_name)) {
                    resp.responce = server_responce_e::s_failure;
                    session_ptr->Write(std::move(resp)); // invoking saved method to inform client that the chat failed to connect
                    return;
                } 
                
                resp.responce = server_responce_e::s_success;
                session_ptr->WriteKeepState(std::move(resp)); // invoking saved method to inform client that the chat is connected RN
                //keep the state because AddUserMethod will be the last one to send the message

                chats_.at(chat_name).AddUser(session_ptr, usr->name);
                
                //synchronized update of user status
                net::dispatch(usernames_strand_, [&chat_name, &session_ptr, this]{
                    if(users_.contains(session_ptr)) users_.at(session_ptr).chat_name = chat_name;
                });

            }
        );
    }     
}

bool ChatManager::CreateChat(std::string name){
    bool result;
    net::dispatch(chats_strand_, 
        [&result, &name, this]{
            if(chats_.contains(name) || name == ""){
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

bool ChatManager::LeaveChat(Session* session_ptr){
    bool result = false;
    if(User* usr = IdentifyUser(session_ptr)){
        net::dispatch(chats_strand_, 
            [&result, this, &usr, &session_ptr]{
                if(!chats_.contains(usr->chat_name) || 
                    !chats_.at(usr->chat_name).ContainsUser(session_ptr)){
                    result = false;
                }
                else{
                    chats_.at(usr->chat_name).DeleteUser(session_ptr);
                    net::dispatch(usernames_strand_, [this, &session_ptr]{
                        if(users_.contains(session_ptr)) users_.at(session_ptr).chat_name.clear();
                    });
                    result = true;
                }
            }
        );
    }
    return result;
}

void ChatManager::SendMessage(std::string message, Session* session_ptr){ // this one actually sends messages by itself
    User* usr = IdentifyUser(session_ptr);
    net::dispatch(chats_strand_, 
        [this, usr, &message, session_ptr]{
            if(chats_.contains(usr->chat_name)) chats_.at(usr->chat_name).SendMessage(message, session_ptr);
        }
    );
}

// RequestHandler methods

void RequestHandler::handle(client_data_t&& req, client_state& state, std::shared_ptr<Session> shared_session_ptr) {
    // Обработать запрос request и отправить ответ, используя ptr нв сессию
    auto session_ptr = shared_session_ptr.get();
    server_data_t resp;
    resp.request = req.request;

    switch(req.request){
    case client_request_e::c_set_name:
        if(state != client_state::no_name){
            resp.message_text = "Cannot set name twice!";
            resp.responce = server_responce_e::s_failure;
            session_ptr->Write(std::move(resp));
        } else {
            if(chatManager.SetName(req.message_text, session_ptr)){
                state = client_state::list_chats;
                resp.message_text = "";
                resp.responce = server_responce_e::s_success;
                session_ptr->WriteKeepState(std::move(resp));

                resp.request = client_request_e::c_get_chats;
                resp.responce = server_responce_e::s_success;
                resp.message_text = chatManager.ChatList();
                

                session_ptr->Write(std::move(resp));

            } else {
                resp.message_text = "Username is already in use";
                resp.responce = server_responce_e::s_failure;
                session_ptr->Write(std::move(resp));
            }
        }
        break;
    case client_request_e::c_create_chat:
        if(state != client_state::list_chats){
            resp.message_text = "Cannot create chats, wrong state";
            resp.responce = server_responce_e::s_failure;
            session_ptr->Write(std::move(resp));
        }
        else{
            if(!chatManager.CreateChat(req.message_text)){
                resp.message_text = "Chat with such name already exists";
                resp.responce = server_responce_e::s_failure;
                session_ptr->Write(std::move(resp));
            }
            else{
                // send everyone out of chat new list
                chatManager.UpdateChatList(session_ptr);
            }
        }
        break;
    case client_request_e::c_connect_chat:
        if(state != client_state::list_chats){
            resp.message_text = "Cannot connect chat, wrong state!";
            resp.responce = server_responce_e::s_failure;
        }
        else {
            chatManager.ConnectChat(req.message_text.data(), session_ptr);
            state = client_state::in_chat;
        }
        break;
    case client_request_e::c_send_message:
        if(state != client_state::in_chat){
            resp.message_text = "";
            resp.responce = server_responce_e::s_failure;
        }
        else chatManager.SendMessage(req.message_text.data(), session_ptr);
        break;
    case client_request_e::c_leave_chat:
        if(state == client_state::in_chat && chatManager.LeaveChat(session_ptr)){
            // Can leave chat, ok state && could to leave the chat
            state = client_state::list_chats;
            resp.request = client_request_e::c_get_chats;
            resp.responce = server_responce_e::s_success;
            resp.message_text = chatManager.ChatList();
            session_ptr->Write(std::move(resp));
        }
        //no messages about leaving
        break;
    default:
        resp.message_text = "Wrong request";
        resp.request = client_request_e::c_wrong_request;
        resp.responce = server_responce_e::s_failure;
        session_ptr->Write(std::move(resp));
    }
}

void RequestHandler::handleDisconnection(Session* session_ptr){
    chatManager.Disconnect(session_ptr);
}

// Server methods

void Server::Run(){
    DoAccept();
}

std::shared_ptr<Server> Server::GetSharedThis(){
    return this->shared_from_this();
}

void Server::DoAccept() {
    // Метод socket::async_accept создаст сокет и передаст его передан в OnAccept
    acceptor_.async_accept(
        // Передаём последовательный исполнитель, в котором будут вызываться обработчики
        // асинхронных операций сокета
        net::make_strand(ioc_),
        // При помощи bind_front_handler создаём обработчик, привязанный к методу OnAccept
        // текущего объекта.
        beast::bind_front_handler(&Server::OnAccept, GetSharedThis()));
}

void Server::OnAccept(sys::error_code ec, tcp::socket socket) {
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

void Server::AsyncRunSession(tcp::socket&& socket) {
    std::make_shared<Session>(std::move(socket), &handler_)->Run();
}

}  // namespace server
