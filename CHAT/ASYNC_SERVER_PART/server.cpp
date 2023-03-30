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

// Chat methods

// void Chat::AddUser(Session* session_ptr, std::string username){
//     net::dispatch(strand, [shared = session_ptr->GetSharedThis(), &username, this]{

//         messages_offset.insert({shared.get(), 0});

//         std::fstream chat_file(path);
//         chat_file.seekp(0, chat_file.end);
//         auto file_size = chat_file.tellp();

//         server_data_t resp;
//         resp.responce = server_responce_e::s_success;

//         std::streamoff off = 0;
//         chat_file.seekg(off, chat_file.beg);

//         while (off != file_size) {
//             resp.request = client_request_e::c_receive_message; // now we change the request that we are serving because we did connect to chat
//             std::getline(chat_file, resp.message_text, '\r');
//             off = chat_file.tellp();
//             if(off != file_size) shared->WriteKeepState(std::move(resp), shared.get());
//             else shared->Write(std::move(resp), shared.get());
//             // invoking saved method to send all unread chat messages to connected user
//         }

//         messages_offset[shared.get()] = off;
//         usernames[shared.get()] = username;

//     });
// }

// void Chat::DeleteUser(Session* session_ptr){
//     net::dispatch(strand, [session_ptr, this](){
//         messages_offset.erase(session_ptr);
//         usernames.erase(session_ptr);
//     });
// }

// void Chat::SendMessage(std::string message, Session* session_ptr){
//     net::dispatch(strand, [&message, shared = session_ptr->GetSharedThis(), this]{
//         // std::fstream chat_file(usr->chat_name + ".chat");
//         if(messages_offset.contains(shared.get())){
//             std::fstream chat_file(path);

//             chat_file << usernames[shared.get()] << "\t";
            
//             time_t curr_time;
//             tm * curr_tm;
//             std::string timedate;
//             timedate.reserve(100);
//             time(&curr_time);
//             curr_tm = localtime(&curr_time);
//             strftime(timedate.data(), 99, "%T %D", curr_tm);

//             chat_file.write(timedate.c_str(), timedate.length());
//             chat_file << std::endl << message << "\r";
        
//             auto file_size = chat_file.tellp();
//             for(auto& [usr_ptr, off] : messages_offset){
//                 chat_file.seekg(off, chat_file.beg);
//                 while(off != file_size){
//                     server_data_t resp;
//                     resp.request = client_request_e::c_receive_message;
//                     std::getline(chat_file, resp.message_text, '\r');
//                     off = chat_file.tellp();
//                     if(usr_ptr != shared.get() || off != file_size) usr_ptr->WriteKeepState(std::move(resp), shared.get());
//                     else usr_ptr->Write(std::move(resp), shared.get());
//                 }
//             }
//         } 
//     });       
// }

// bool Chat::ContainsUser(Session* session_ptr){
//     bool result;
//     net::dispatch(strand, [shared = session_ptr->GetSharedThis(), &result, this]{
//         result = messages_offset.contains(shared.get());
//     });
//     return result;
// }

// // ChatManager methods

boost::asio::awaitable<bool> ChatManager::SetName(std::string name, tcp::socket* user_socket){
    bool result = true;
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
    for(auto [socket_ptr, user] : users_){
        if(user.name == name){
            result = false;
            break;
        }
    }

    if(result) {
        std::cout << name << " was registered!\n";
        users_.insert({user_socket, {name, ""}});
    }
    else std::cout << name << " could not be registered!\n";

    co_return result;
}

boost::asio::awaitable<std::string> ChatManager::ChatList(){
    std::string list;
    co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);
    for(auto chat : chats_) list += chat.first + "\n";
    co_return list;
}

boost::asio::awaitable<void> ChatManager::UpdateChatList(){
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);

    server_data_t resp;
    std::string list = co_await ChatList();
    for(auto [socket_ptr, user] : users_){
        if(user.chat_name.empty()){
            resp.message_text = list;
            resp.request = c_get_chats;
            resp.responce = s_success;
            Write(socket_ptr, std::move(resp));
        }
    }
}

boost::asio::awaitable<void> ChatManager::Disconnect(tcp::socket* user_socket){
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);

    if(users_.contains(user_socket)) {
        User usr = users_.at(user_socket);
        co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);

        //synchronized deletion of the user listing in chat through chat's method
        if(chats_.contains(usr.chat_name)) {
            chats_.at(usr.chat_name).DeleteUser(user_socket); // throws exception because ???
        }
        //synchronized deletion of the user from the list of users in the chat manager
        co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
        users_.erase(user_socket);
    }
}

boost::asio::awaitable<void> ChatManager::ConnectChat(std::string chat_name, tcp::socket* user_socket){ //Sends everything the client needs, including success or failure notification
    using namespace std::literals;
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
    if(users_.contains(user_socket)) {
        User usr = users_.at(user_socket);
        co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);
        server_data_t resp;
        resp.message_text = "";
        resp.request = client_request_e::c_connect_chat;

        if(!chats_.contains(chat_name)) {
            resp.responce = server_responce_e::s_failure;
            Write(user_socket, std::move(resp)); // invoking saved method to inform client that the chat failed to connect
            co_return;
        } 
        
        resp.responce = server_responce_e::s_success;
        Write(user_socket, std::move(resp)); // invoking saved method to inform client that the chat is connected RN
        //keep the state because AddUser method will be the last one to send the message

        chats_.at(chat_name).AddUser(user_socket, usr.name);
        
        //synchronized update of user status
        co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
            
        if(users_.contains(user_socket)) users_.at(user_socket).chat_name = chat_name;

    }
}

boost::asio::awaitable<bool> ChatManager::CreateChat(tcp::socket* user_socket){
    bool result = true;
    co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);
    if(chats_.contains(name) || name == ""){
        result = false;
    }
    if(result) chats_.insert({name, {io_, path_of_chats / (name + ".chat")}});
    co_return result;
}

boost::asio::awaitable<bool> ChatManager::LeaveChat(tcp::socket* user_socket){
    bool result = false;
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
    if(users_.contains(user_socket)){
        User usr = users_.at(user_socket);
        co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);
        if(!chats_.contains(usr.chat_name) || 
            !chats_.at(usr.chat_name).ContainsUser(user_socket)){
            result = false;
        }
        else{
            chats_.at(usr.chat_name).DeleteUser(user_socket);
            co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
            if(users_.contains(user_socket)) users_.at(user_socket).chat_name.clear();
            result = true;
        }
    }
    co_return result;
}

 boost::asio::awaitable<void> ChatManager::SendMessage(std::string message, tcp::socket* user_socket){ // this one actually sends messages by itself
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
    if(users_.contains(user_socket)){
        User usr = users_.at(user_socket);

        net::dispatch(chats_strand_, boost::asio::use_awaitable);
        if(chats_.contains(usr.chat_name)) chats_.at(usr.chat_name).SendMessage(message, user_socket);
    }
}

boost::asio::awaitable<User*> ChatManager::IdentifyUser(tcp::socket* user_socket){
    User* usr;
    net::dispatch(usernames_strand_, boost::asio::use_awaitable);
    if(!users_.contains(user_socket)) usr = nullptr;
    else usr = &users_.at(user_socket);
    co_return usr;
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
    boost::asio::co_spawn(ioc_, Session);
}

}  // namespace server
