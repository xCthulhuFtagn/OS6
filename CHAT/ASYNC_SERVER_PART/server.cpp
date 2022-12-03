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

ChatManager::ChatManager(net::io_context& io, std::string path) : io_(io), chats_strand_(net::make_strand(io)), usernames_strand_(net::make_strand(io)), path_of_chats(path.data()){
    namespace fs = std::filesystem;
    for (const auto& entry : fs::directory_iterator(path_of_chats)) {
        if (entry.is_regular_file() && fs::path(entry).extension() == ".chat") {
            Chat new_chat = Chat(io);
            chats_.insert({fs::path(entry).stem().string(), new_chat});
        }
    }
}

void ChatManager::OnMessageSent(boost::system::error_code& ec, std::size_t size){
    if(ec){

    }
    else{

    }
}

bool ChatManager::ConnectChat(const std::string& chat_name, const std::string& user_name, tcp::socket* user_socket){
    bool result;
    net::dispatch(chats_strand_, 
        [&result, &chat_name, &user_name, chats = &chats_, &user_socket, this]{

            if(!chats->contains(chat_name) || (*chats).at(chat_name).users.contains(user_name)) {
                result = false;
                return;
            } 
            
            (*chats).at(chat_name).users[user_name] = user_socket;

            std::shared_ptr<server_data_t> resp;
            resp->request = client_request_e::c_receive_message;
            resp->responce = server_responce_e::s_success;

            std::fstream chat_file(chat_name + ".chat");
            chat_file.seekp(0, chat_file.end);
            auto file_size = chat_file.tellp();

            std::streamoff off = 0;
            chat_file.seekg(off, chat_file.beg);
            while (off != file_size) {
                std::getline(chat_file, resp->message_text, '\r');
                off = chat_file.tellp();
                chats->at(chat_name).users[user_name]->async_send(net::buffer(resp.get(), sizeof(server_data_t)), 
                    [this](beast::error_code ec, std::size_t bytes_written){
                        this->OnMessageSent(ec, bytes_written);
                    });
            }
            (*chats).at(chat_name).messages_offset[user_name] = off;
            
            result = true;
            return; 
        }
    );        
    return result;
}

bool ChatManager::CreateChat(const std::string& name){
    bool result;
    net::dispatch(chats_strand_, 
        [&result, &name, chats = &chats_, io = &io_]{
            if(chats->contains(name)){
                result = false;
                return;
            }
            Chat new_chat = Chat(*io);
            chats->insert({name, new_chat});
            result = true;
            return;
        }
    );
    return result;
}

bool ChatManager::LeaveChat(const std::string& chat_name, const std::string& user_name){
    bool result;
    net::dispatch(chats_strand_, 
        [&result, &chat_name, chats = &chats_, &user_name]{
            if(!chats->contains(chat_name) || chats->at(chat_name).users.contains(user_name)){
                result = false;
                return;
            }

            chats->at(chat_name).users.erase(user_name);
            chats->at(chat_name).messages_offset.erase(user_name);
            result = true;
            return;
        }
    );
    return result;
}

void ChatManager::SendMessage(const std::string& chat_name, const std::string& message, const std::string& user_name){ // this one actually sends messages by itself
        net::dispatch(chats_.at(chat_name).strand, 
            [&chat_name, &message, &user_name, chats = &chats_, this]{
                if(chats->contains(chat_name)){
                    std::fstream chat_file(chat_name + ".chat");

                    if(chats->at(chat_name).users.contains(user_name)){

                        chat_file << user_name << "\t";
                        
                        time_t curr_time;
                        tm * curr_tm;
                        std::string timedate;
                        timedate.reserve(100);
                        time(&curr_time);
                        curr_tm = localtime(&curr_time);
                        strftime(timedate.data(), 99, "%T %D", curr_tm);

                        chat_file.write(timedate.c_str(), timedate.length());
                        chat_file << std::endl << message << "\r";

                    }
                    auto file_size = chat_file.tellp();
                    for(auto& [name, off] : chats->at(chat_name).messages_offset){
                        chat_file.seekg(off, chat_file.beg);
                        while(off != file_size){
                            std::shared_ptr<server_data_t> safe_resp = std::make_shared<server_data_t>();
                            std::getline(chat_file, safe_resp->message_text, '\r');
                            off = chat_file.tellp();
                            chats->at(chat_name).users[name]->async_send(net::buffer(safe_resp.get(), sizeof(server_data_t)), 
                            [this](beast::error_code ec, std::size_t bytes_written){
                                this->OnMessageSent(ec, bytes_written);
                            });
                        }
                    }
                }

            }
        );
}
}  // namespace server
