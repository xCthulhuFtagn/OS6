#include "server.h"
#include "logger.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <ctime>

#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>

#include <boost/scoped_ptr.hpp>

namespace server {

std::string StringifyRequest(client_request_e r){
    switch(r){
        case 0: return "c_set_name"s;
        case 1: return "c_create_chat"s;
        case 2: return "c_connect_chat"s;
        case 3: return "c_send_message"s;
        case 4: return "c_leave_chat"s;
        case 5: return "c_disconnect"s;
        case 6: return "c_receive_message"s;
        case 7: return "c_wrong_request"s;
        case 8: return "c_get_chats"s;
        default: return "wrong input"s;
    }
}

std::string StringifyResponse(server_response_e r){
    switch(r){
        case 0: return "s_success"s;
        case 1: return "s_failure"s;
        default: return "wrong input"s;
    }
}

net::awaitable<void> Write(std::shared_ptr<tcp::socket> socket, server_data_t&& response){
    size_t message_length = response.message_text.size();
    boost::scoped_ptr<server_data_t> safe_response(new server_data_t(response));

    boost::json::value data = {
        {"response", 
            {   
                {"client_request_e", StringifyRequest(safe_response->request)},
                {"server_response_e", StringifyResponse(safe_response->response)},
                {"message", safe_response->message_text}
            }
        },
        {"socket", int(socket->native_handle())}
    };
    logger::Logger::GetInstance().Log("Wrote response"sv, data);

    auto [ec, bytes] = co_await net::async_write(
        *socket, 
        boost::array<boost::asio::mutable_buffer, 4>({
            net::buffer((void*)&safe_response->request, sizeof(client_request_e)),
            net::buffer((void*)&safe_response->response, sizeof(server_response_e)),
            net::buffer((void*)&message_length, sizeof(size_t)),
            net::buffer(safe_response->message_text)
        }),
        // boost::asio::transfer_all(), // maybe needed
        net::experimental::as_tuple(net::use_awaitable)
    );

    if(ec) {
        boost::json::value data = {
            {"socket", int(socket->native_handle())},
            {"error", ec.what()}
        };
        logger::Logger::GetInstance().Log("Write error"sv, data);

        throw ec;
    }

}

// Chat methods

boost::asio::awaitable<void> Chat::AddUser(std::shared_ptr<tcp::socket> socket, std::string username){
    co_await net::dispatch(strand, boost::asio::use_awaitable);

    boost::json::value data = {
        {"socket", int(socket->native_handle())},
        {"username", username},
        {"chat_name", path.stem().c_str()}
    };
    logger::Logger::GetInstance().Log("User added to chat"sv, data);

    messages_offset.insert({socket, 0});

    std::fstream chat_file(path);
    chat_file.seekp(0, chat_file.end);
    auto file_size = chat_file.tellp();

    server_data_t resp;

    std::streamoff off = 0;
    chat_file.seekg(off, chat_file.beg);

    while (off != file_size) {
        resp.response = server_response_e::s_success;
        resp.request = client_request_e::c_receive_message; // now we change the request that we are serving because we did connect to chat
        std::getline(chat_file, resp.message_text, '\r');
        off = chat_file.tellp();
        co_await Write(socket, std::move(resp));
        // invoking saved method to send all unread chat messages to connected user
        // std::cout << "[off = " << off << ", file_size = " << file_size << "]\n";
    }

    messages_offset[socket] = off;
    usernames[socket] = username;
}

boost::asio::awaitable<void> Chat::DeleteUser(std::shared_ptr<tcp::socket> socket){
    co_await net::dispatch(strand, boost::asio::use_awaitable);

    std::string username;
    if(usernames.contains(socket)) username = usernames.at(socket);

    boost::json::value data = {
        {"socket", int(socket->native_handle())},
        {"username", username},
        {"chat_name", path.stem().c_str()}
    };
    logger::Logger::GetInstance().Log("User deleted from chat"sv, data);
    
    messages_offset.erase(socket);
    usernames.erase(socket);

}

boost::asio::awaitable<void> Chat::SendMessage(std::shared_ptr<tcp::socket> socket, std::string message){
    co_await net::dispatch(strand, boost::asio::use_awaitable);

    boost::json::value data = {
        {"socket", int(socket->native_handle())},
        {"username", usernames.at(socket)},
        {"chat_name", path.stem().c_str()},
        {"message", message}
    };
    logger::Logger::GetInstance().Log("User sent message to chat"sv, data);

    if(messages_offset.contains(socket)){
        std::fstream chat_file;
        
        chat_file.open(path, std::fstream::out | std::fstream::app);

        chat_file << usernames[socket] << "\t";
        
        time_t curr_time;
        tm * curr_tm;
        std::string timedate;
        timedate.reserve(100);
        time(&curr_time);
        curr_tm = localtime(&curr_time);
        strftime(timedate.data(), 99, "%T %D", curr_tm);

        chat_file << timedate.c_str() << std::endl << message << "\r";
    
        auto file_size = chat_file.tellp();

        chat_file.close();

        chat_file.open(path, std::fstream::in | std::fstream::app);


        for(auto& [usr_socket, off] : messages_offset){
            chat_file.seekg(off, chat_file.beg);
            while(off != file_size){
                server_data_t resp;
                resp.request = client_request_e::c_receive_message;
                resp.response = server_response_e::s_success;
                std::getline(chat_file, resp.message_text, '\r');
                off = chat_file.tellp();
                try{
                    co_await Write(usr_socket, std::move(resp));
                } catch(sys::error_code& ec){
                    if(usr_socket == socket) throw ec; 
                    // if our socket is broken - that's a problem
                    // else - let it be destroyed afterwards by it's session
                }
            }
        }

        chat_file.close();
    } 
}

boost::asio::awaitable<bool> Chat::ContainsUser(std::shared_ptr<tcp::socket> socket){
    co_await net::dispatch(strand, boost::asio::use_awaitable);

    std::string username;
    if(usernames.contains(socket)) username = usernames.at(socket); 
    boost::json::value data = {
        {"socket", int(socket->native_handle())},
        {"username", username},
        {"chat_name", path.stem().c_str()}
    };
    logger::Logger::GetInstance().Log("Checked if user is in the chat"sv, data);

    co_return messages_offset.contains(socket);
}

// // ChatManager methods

boost::asio::awaitable<bool> ChatManager::SetName(std::string name, std::shared_ptr<tcp::socket> socket){
    bool result = true;
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
    for(auto [socket_ptr, user] : users_){
        if(user.name == name){
            result = false;
            break;
        }
    }

    if(result) {
        users_.insert({socket, {name, ""}});
    }

    boost::json::value data = {
        {"socket", int(socket->native_handle())},
        {"username", name},
        {"result", int(result)} // maybe change to another type
    };
    logger::Logger::GetInstance().Log("Name requested by a user"sv, data);

    co_return result;
}

boost::asio::awaitable<std::string> ChatManager::ChatList(){
    co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);

    std::string list;
    for(auto chat : chats_) list += chat.first + "\n";

    boost::json::value data = {
        {"chat list", list}
    };
    logger::Logger::GetInstance().Log("Formed a chatlist"sv, data);

    co_return list;
}

boost::asio::awaitable<void> ChatManager::UpdateChatList(std::shared_ptr<tcp::socket> socket){
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);

    server_data_t resp;
    std::string list = co_await ChatList();

    boost::json::value data = {
        {"chat list", list}
    };
    logger::Logger::GetInstance().Log("Chat list updated"sv, data);

    for(auto [user_socket, user] : users_){
        if(user.chat_name.empty()){
            resp.message_text = list;
            resp.request = c_get_chats;
            resp.response = s_success;
            try{
                co_await Write(user_socket, std::move(resp));
            } catch(sys::error_code& ec){
                if(user_socket == socket) throw ec;
                // our socket - our problem, else - nah
            }
        }
    }
}

boost::asio::awaitable<void> ChatManager::Disconnect(std::shared_ptr<tcp::socket> socket){
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
    
    std::string name;
    if(users_.contains(socket)) name = users_.at(socket).name; 

    boost::json::value data = {
        {"socket", int(socket->native_handle())},
        {"username", name}
    };
    logger::Logger::GetInstance().Log("User disconnected"sv, data);

    if(users_.contains(socket)) {
        User usr = users_.at(socket);
        co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);

        //synchronized deletion of the user listing in chat through chat's method
        if(chats_.contains(usr.chat_name)) {
            co_await chats_.at(usr.chat_name).DeleteUser(socket); // throws exception because ???
        }
        //synchronized deletion of the user from the list of users in the chat manager
        co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
        users_.erase(socket);
    }
}

boost::asio::awaitable<void> ChatManager::ConnectChat(std::string chat_name, std::shared_ptr<tcp::socket> socket){ //Sends everything the client needs, including success or failure notification
    using namespace std::literals;
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);

    std::string name;
    if(users_.contains(socket)) name = users_.at(socket).name; 

    boost::json::value data = {
        {"socket", int(socket->native_handle())},
        {"username", name},
        {"chat name", chat_name}
    };
    logger::Logger::GetInstance().Log("User connected to chat"sv, data);

    if(users_.contains(socket)) {
        User usr = users_.at(socket);
        co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);
        
        server_data_t resp;
        resp.message_text = chat_name;
        resp.request = client_request_e::c_connect_chat;

        if(!chats_.contains(chat_name)) {
            resp.response = server_response_e::s_failure;
            co_await Write(socket, std::move(resp)); // invoking saved method to inform client that the chat failed to connect
            co_return;
        } 
        
        resp.response = server_response_e::s_success;
        co_await Write(socket, std::move(resp)); // invoking saved method to inform client that the chat is connected RN
        //keep the state because AddUser method will be the last one to send the message

        co_await chats_.at(chat_name).AddUser(socket, usr.name);
        
        //synchronized update of user status
        co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
            
        if(users_.contains(socket)) users_.at(socket).chat_name = chat_name;

    }
}

boost::asio::awaitable<bool> ChatManager::CreateChat(std::string chat_name){
    co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);

    bool result = true;
    if(chats_.contains(chat_name) || chat_name == ""){
        result = false;
    }
    if(result) chats_.insert({chat_name, {io_, path_of_chats / (chat_name + ".chat")}});

    boost::json::value data = {
        {"chat name", chat_name},
        {"result", int(result)}
    };
    logger::Logger::GetInstance().Log("Chat creation request"sv, data);

    co_return result;
}

boost::asio::awaitable<bool> ChatManager::LeaveChat(std::shared_ptr<tcp::socket> socket){
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);

    bool result;

    if(users_.contains(socket)){
        User usr = users_.at(socket);
        co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);
        if(!chats_.contains(usr.chat_name) || 
            !co_await chats_.at(usr.chat_name).ContainsUser(socket)){
            result = false;
        }
        else{
            co_await chats_.at(usr.chat_name).DeleteUser(socket);
            co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);
            if(users_.contains(socket)) users_.at(socket).chat_name.clear();
            result = true;
        }
    }

    std::string name;
    if(users_.contains(socket)) name = users_.at(socket).name; 

    boost::json::value data = {
        {"socket", int(socket->native_handle())},
        {"username", name},
        {"result", int(result)}
    };
    logger::Logger::GetInstance().Log("Leave chat request"sv, data);

    co_return result;
}

 boost::asio::awaitable<void> ChatManager::SendMessage(std::string message, std::shared_ptr<tcp::socket> socket){ // this one actually sends messages by itself
    co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);

    std::string name;
    if(users_.contains(socket)) name = users_.at(socket).name; 

    boost::json::value data = {
        {"socket", int(socket->native_handle())},
        {"username", name},
        {"message", message}
    };
    logger::Logger::GetInstance().Log("Send message request"sv, data);

    if(users_.contains(socket)){
        User usr = users_.at(socket);

        co_await net::dispatch(chats_strand_, boost::asio::use_awaitable);
        if(chats_.contains(usr.chat_name)) co_await chats_.at(usr.chat_name).SendMessage(socket, message);
    }
}

// boost::asio::awaitable<User*> ChatManager::IdentifyUser(tcp::socket* socket){
//     co_await net::dispatch(usernames_strand_, boost::asio::use_awaitable);

//     boost::json::value data = {
//         {"socket", int(socket->native_handle())},
//     };
//     logger::Logger::GetInstance().Log("Identify user request"sv, data);

//     User* usr;
//     if(!users_.contains(socket)) usr = nullptr;
//     else usr = &users_.at(socket);
//     co_return usr;
// }


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
        boost::json::value data = {
            {"error", ec.what()}
        };
        logger::Logger::GetInstance().Log("Accept error"sv, data);
    }

    // Для того, чтобы подключение произошло
    // нужно прочитать сначала данные для подтверждения подключения
    

    // Асинхронно обрабатываем сессию
    AsyncRunSession(std::move(socket));

    // Принимаем новое соединение
    DoAccept();
}

void Server::AsyncRunSession(tcp::socket&& socket) {
    boost::asio::co_spawn(ioc_, Session(std::move(socket), &chat_manager_), boost::asio::detached);
}

// Session
boost::asio::awaitable<void> Session(tcp::socket&& socket, ChatManager* chatManager){


    bool running = true;
    session_state sessionState = session_state::reading;
    client_state clientState = client_state::no_name;
    client_data_t request_;
    server_data_t response_;
    size_t message_length;


    std::shared_ptr<tcp::socket> socket_ = std::make_shared<tcp::socket>(std::move(socket));

    boost::json::value data = {
        {"socket", int(socket_->native_handle())}
    };
    logger::Logger::GetInstance().Log("Session started"sv, data);

    try{
        while(true){
            auto [ec, bytes] = co_await 
            boost::asio::async_read(
                *socket_, 
                boost::array<boost::asio::mutable_buffer, 2>({
                    boost::asio::buffer((void*)&request_.request, sizeof(request_.request)),
                    boost::asio::buffer((void*)&message_length, sizeof(message_length))
                }),
                // boost::asio::transfer_all(), // maybe needed
                boost::asio::experimental::as_tuple(boost::asio::use_awaitable)
            );
            if (!ec) {
                request_.message_text.resize(message_length);
                auto [ec2, bytes2] = co_await
                boost::asio::async_read(
                    *socket_, 
                    boost::asio::buffer(request_.message_text),
                    // boost::asio::transfer_all(), // maybe needed
                    boost::asio::experimental::as_tuple(boost::asio::use_awaitable)
                );
                if(!ec2) sessionState = session_state::handling;
                else {
                    //log here
                    boost::json::value data = {
                        {"socket", int(socket_->native_handle())},
                        {"error", ec2.what()}
                    };
                    logger::Logger::GetInstance().Log("Read error"sv, data);
                    throw ec2;
                }
            }
            else {
                //log here
                boost::json::value data = {
                    {"socket", int(socket_->native_handle())},
                    {"error", ec.what()}
                };
                logger::Logger::GetInstance().Log("Read error"sv, data);
                throw ec;
            }
            // std::cout << "Received message: {request: " << request_.request << ", message: \"" << request_.message_text << "\"}" << std::endl;
            response_.request = request_.request;
            switch(request_.request){
                case client_request_e::c_set_name:
                    if(clientState != client_state::no_name){
                        response_.message_text = "Cannot set name twice!";
                        response_.response = server_response_e::s_failure;
                        co_await Write(socket_, std::move(response_));
                    } else {
                        if(co_await chatManager->SetName(request_.message_text, socket_)){
                            clientState = client_state::list_chats;
                            response_.message_text = "";
                            response_.response = server_response_e::s_success;
                            co_await Write(socket_, std::move(response_));

                            response_.request = client_request_e::c_get_chats;
                            response_.response = server_response_e::s_success;
                            response_.message_text = co_await chatManager->ChatList();
                            

                            co_await Write(socket_, std::move(response_));

                        } else {
                            response_.message_text = "Username is already in use";
                            response_.response = server_response_e::s_failure;
                            co_await Write(socket_, std::move(response_));
                        }
                    }
                    break;
                case client_request_e::c_create_chat:
                    if(clientState != client_state::list_chats){
                        response_.message_text = "Cannot create chats, wrong state";
                        response_.response = server_response_e::s_failure;
                        co_await Write(socket_, std::move(response_));
                    }
                    else{
                        if(!co_await chatManager->CreateChat(request_.message_text)){
                            response_.message_text = "Chat with such name already exists";
                            response_.response = server_response_e::s_failure;
                            co_await Write(socket_, std::move(response_));
                        }
                        else{
                            // send everyone out of chat new list
                            co_await chatManager->UpdateChatList(socket_);
                        }
                    }
                    break;
                case client_request_e::c_connect_chat:
                    if(clientState != client_state::list_chats){
                        response_.message_text = "Cannot connect chat, wrong state!";
                        response_.response = server_response_e::s_failure;
                        co_await Write(socket_, std::move(response_));
                    }
                    else {
                        co_await chatManager->ConnectChat(request_.message_text, socket_);
                        clientState = client_state::in_chat;
                    }
                    break;
                case client_request_e::c_send_message:
                    if(clientState != client_state::in_chat){
                        response_.message_text = "";
                        response_.response = server_response_e::s_failure;
                        co_await Write(socket_, std::move(response_));
                    }
                    else 
                        co_await chatManager->SendMessage(request_.message_text, socket_);
                    break;
                case client_request_e::c_leave_chat:
                    if(clientState == client_state::in_chat && co_await chatManager->LeaveChat(socket_)){
                        // Can leave chat, ok state && could to leave the chat
                        clientState = client_state::list_chats;
                        response_.request = client_request_e::c_get_chats;
                        response_.response = server_response_e::s_success;
                        response_.message_text = co_await chatManager->ChatList();
                        co_await Write(socket_, std::move(response_));
                    }
                    else{
                        response_.message_text = "";
                        response_.response = server_response_e::s_failure;
                        co_await Write(socket_, std::move(response_));
                    }
                    //no messages about leaving
                    break;
                default:
                    response_.message_text = "Wrong request";
                    response_.request = client_request_e::c_wrong_request;
                    response_.response = server_response_e::s_failure;
                    co_await Write(socket_, std::move(response_));
                }
        }
    } 
    catch(std::exception ex){
        boost::json::value data = {
            {"exception", ex.what()}
        };
        logger::Logger::GetInstance().Log("Exception occured"sv, data);
    }
    catch(...){}
    // catch(beast::error_code ec){ //read\write errors
    //     std::cout << "Boost error occured: " << ec.what() << std::endl;
    // }
    //disconnection
    // std::cout << "Session ended!" << std::endl;
    co_await chatManager->Disconnect(socket_);

    data = {
        {"socket", int(socket_->native_handle())}
    };
    logger::Logger::GetInstance().Log("Session ended"sv, data);
    
    socket_->close();
}

}  // namespace server
