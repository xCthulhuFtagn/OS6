#include "server_interface.h"
#include "server_routines.h"
#include "logger.h"
#include <iostream>
#include <queue>
#include <chrono>

extern int disco_pipe[2];
extern int list_chats_pipe[2];
extern int username_enter_pipe[2];

extern std::unordered_map<std::string, chat_info> chats;
extern std::unordered_map<int, std::string> user_data;
extern std::unordered_set<std::string> used_usernames;
// std::unordered_map<int, std::chrono::system_clock::time_point> requests;

void chat_message(int go_in_chat_pipe_input, std::string chat_name) {
    client_data_t client_data;
    server_data_t server_data;

    int chat_epoll_fd = epoll_create1(0), chatfd;
    if (chat_epoll_fd < 0) {
        boost::json::value data = {
            {"error", std::error_code{errno, std::generic_category()}.message()},
            {"chat", chat_name}
        };
        logger::Logger::GetInstance().Log("Failed to create epoll"sv, data);
        return;
    }
    std::fstream chat_file(chat_name + ".chat");
    if (!chat_file) {
        boost::json::value data = {
            {"error", std::error_code{errno, std::generic_category()}.message()},
            {"chat", chat_name}
        };
        logger::Logger::GetInstance().Log("Failed to open chat's file"sv, data);
        return;
    }
    std::vector<epoll_event> vec_of_events(10);
    std::unordered_map<int, std::streamoff> messages_offset;
    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLET;

    time_t curr_time;
	tm * curr_tm;
    size_t off = 0;

    while (true) {
        // changing hashmap of events
        int read_bytes;
        while ((read_bytes = read(go_in_chat_pipe_input, &ev.data.fd, sizeof(int))) > 0) {
            boost::json::value data = {
                {"chat", chat_name},
                {"bytes received", read_bytes},
                {"socket", ev.data.fd}
            };
            logger::Logger::GetInstance().Log("Client entered chat routine"sv, data);
            // vec_of_events.push_back(ev);
            messages_offset[ev.data.fd] = 0;
            if(epoll_ctl(chat_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev)){
                boost::json::value data = {
                    {"error", std::error_code{errno, std::generic_category()}.message()},
                    {"socket", ev.data.fd},
                    {"chat", chat_name}
                };
                logger::Logger::GetInstance().Log("Failed to add socket from epoll"sv, data);
            }
        }

        if (read_bytes < 0 && errno != EAGAIN && errno != EINTR) {
            boost::json::value data = {
                {"error", std::error_code{errno, std::generic_category()}.message()},
                {"chat", chat_name}
            };
            logger::Logger::GetInstance().Log("Pipe failed"sv, data);
            close(chat_epoll_fd);
            chat_file.close();
            return;
        }
        // wait for messages
        char timedate_string[100] = { 0 };
        int n = epoll_wait(chat_epoll_fd, vec_of_events.data(), vec_of_events.size(), 25);
        if (n < 0 && errno != EINVAL && errno != EINTR) {
            boost::json::value data = {
                {"error", std::error_code{errno, std::generic_category()}.message()},
                {"chat", chat_name}
            };
            logger::Logger::GetInstance().Log("Epoll wait failed"sv, data);
            close(chat_epoll_fd);
            return;
        } else if (n > 0) {
            time(&curr_time);
            curr_tm = localtime(&curr_time);
            strftime(timedate_string, sizeof(timedate_string) - 1, "%T %D", curr_tm);
            for (off = 0; off < n; ++off) {
                int err;
                int fd = vec_of_events[off].data.fd;
                while ((err = read_request_from_client(&client_data, fd)) > 0) {
                    //depend on type of message do what to need to
                    switch (client_data.request)
                    {
                        case c_leave_chat:
                        {
                            if(epoll_ctl(chat_epoll_fd, EPOLL_CTL_DEL, fd, &ev) == -1){
                                boost::json::value data = {
                                    {"error", std::error_code{errno, std::generic_category()}.message()},
                                    {"socket", fd},
                                    {"chat", chat_name}
                                };
                                logger::Logger::GetInstance().Log("Failed to remove socket from epoll"sv, data);
                            }
                            messages_offset.erase(fd);
                            chats[chat_name].subs.erase(fd); //unsubscribing
                            std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                            if(write(list_chats_pipe[1], &fd, sizeof(int)) < 0){
                                // perror("Failed to send client's fd to no_chat routine");
                                boost::json::value data = {
                                    {"error", std::error_code{errno, std::generic_category()}.message()},
                                    {"socket", fd},
                                    {"chat", chat_name}
                                };
                                logger::Logger::GetInstance().Log("Failed to send client's fd to no_chat routine"sv, data);
                            }
                            boost::json::value data = {
                                {"chat", chat_name},
                                {"socket", fd}
                            };
                            logger::Logger::GetInstance().Log("User left chat"sv, data);
                        }
                            goto OUT_OF_CYCLE;
                        case c_disconnect:
                        {
                            boost::json::value data = {
                                {"chat", chat_name},
                                {"socket", fd}
                            };
                            logger::Logger::GetInstance().Log("Disconnecting user"sv, data);
                            if(epoll_ctl(chat_epoll_fd, EPOLL_CTL_DEL, fd, &ev) == -1){
                                boost::json::value data = {
                                    {"error", std::error_code{errno, std::generic_category()}.message()},
                                    {"socket", fd},
                                    {"chat", chat_name}
                                };
                                logger::Logger::GetInstance().Log("Failed to remove socket from epoll"sv, data);
                            }
                            messages_offset.erase(fd);
                            chats[chat_name].subs.erase(fd); //unsubscribing
                            std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                            write(disco_pipe[1], &fd, sizeof(int));
                        }
                            goto OUT_OF_CYCLE;
                        case c_send_message:
                        {
                            boost::json::value data = {
                                {"chat", chat_name},
                                {"socket", fd},
                                {"message", client_data.message_text}
                            };
                            logger::Logger::GetInstance().Log("Sending message"sv, data);
                            chat_file << user_data[fd] << '\t';
                            chat_file.write(timedate_string, strlen(timedate_string));
                            chat_file << std::endl << client_data.message_text << "\r";
                        }
                            break;
                        default:
                            // fprintf(stderr, "Client %d sent unacceptable message\n", fd);
                            boost::json::value data = {
                                {"chat", chat_name},
                                {"socket", fd},
                                {"request", StringifyRequest(client_data.request)},
                                {"message", client_data.message_text}
                            };
                            logger::Logger::GetInstance().Log("Client sent unacceptable data"sv, data);
                            goto OUT_OF_CYCLE;
                    }
                }
                OUT_OF_CYCLE:
                if (err == 0) {
                    epoll_ctl(chat_epoll_fd, EPOLL_CTL_DEL, fd, &ev);
                    messages_offset.erase(fd);
                    chats[chat_name].subs.erase(fd); //unsubscribing
                    std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                    write(disco_pipe[1], &fd, sizeof(int));
                }
            }
        }
        { // view for chat_locker
            std::lock_guard chat_locker(chats[chat_name].mtx);
            if(chats[chat_name].subs.empty()){
                // fprintf(stderr, "closing routine for chat: %s\n", chat_name.c_str());
                boost::json::value data = {
                    {"chat name",   chat_name}
                };
                logger::Logger::GetInstance().Log("Closing routine for chat"sv, data);
                // close(go_in_chat_pipe_input);
                close(chats[chat_name].pipe.out);
                close(chats[chat_name].pipe.in);
                close(chat_epoll_fd);
                chat_file.close();
                chats[chat_name].pipe.in = -1;
                chats[chat_name].pipe.out = -1;
                return;
            }
        }
        if (messages_offset.size() != vec_of_events.size())
            vec_of_events.resize(messages_offset.size());
        // sending data to all clients
        chat_file.seekp(0, chat_file.end);
        auto file_size = chat_file.tellp();
        server_data.request = c_receive_message;

        std::unordered_set<int> closed_sockets;

        for (auto& [fd, off] : messages_offset) {
            chat_file.seekg(off, chat_file.beg);
            while (off != file_size) {
                //maybe check for chat_file.bad()
                std::getline(chat_file, server_data.message_text, '\r');
                off = chat_file.tellp();
                if(!send_resp_to_client(&server_data, fd)) closed_sockets.insert(fd);
            }
        }

        for(auto fd : closed_sockets){ // getting rid of sockets that were closed during being written into
            boost::json::value data = {
                {"chat", chat_name},
                {"socket", fd}
            };
            logger::Logger::GetInstance().Log("Disconnecting user"sv, data);
            if(epoll_ctl(chat_epoll_fd, EPOLL_CTL_DEL, fd, &ev) == -1){
                boost::json::value data = {
                    {"error", std::error_code{errno, std::generic_category()}.message()},
                    {"socket", fd},
                    {"chat", chat_name}
                };
                logger::Logger::GetInstance().Log("Failed to remove socket from epoll"sv, data);
            }
            messages_offset.erase(fd);
            chats[chat_name].subs.erase(fd); //unsubscribing
            std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
            write(disco_pipe[1], &fd, sizeof(int));
        }
    }
}

void list_of_chats(int end_to_read_from) {
    int no_chat_epoll_fd = epoll_create1(0);
    if (no_chat_epoll_fd < 0) {
        // perror("epoll create");
        boost::json::value data = {
            {"error", std::error_code{errno, std::generic_category()}.message()}
        };
        logger::Logger::GetInstance().Log("list of chats epoll create error"sv, data);
        return;
    }
    std::vector<epoll_event> vec_of_events(10); //empty at start
    std::unordered_set<int> clients_without_chat;
    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLET;

    server_data_t resp;
    client_data_t received;
    int new_socket, bytes;
    while(true){
        while ((bytes = read(end_to_read_from, &new_socket, sizeof(int))) > 0) {
            boost::json::value data = {
                {"Bytes received", bytes},
                {"New socket", new_socket}
            };
            logger::Logger::GetInstance().Log("Client entered list_of_chats routine"sv, data);
            if(send_available_chats(new_socket)){ // if socket did not close while it was written into
                ev.data.fd = new_socket;
                clients_without_chat.insert(new_socket);
                epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
            }
            else{
                boost::json::value data = {
                    {"New socket", new_socket}
                };
                logger::Logger::GetInstance().Log("Could not send available chats"sv, data);
                std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                write(disco_pipe[1], &new_socket, sizeof(int));
            }
        }
        if(bytes < 0 && errno != EAGAIN && errno != EINTR){
            // perror("list of chats pipe error");
            boost::json::value data = {
                {"error", std::error_code{errno, std::generic_category()}.message()}
            };
            logger::Logger::GetInstance().Log("list of chats pipe error"sv, data);
        }
        int n = epoll_wait(no_chat_epoll_fd, vec_of_events.data(), vec_of_events.size(), 100);
        if (n < 0 && errno != EINVAL && errno != EINTR) {
            boost::json::value data = {
                {"error", std::error_code{errno, std::generic_category()}.message()}
            };
            logger::Logger::GetInstance().Log("Epoll wait failed list_of_chats"sv, data);
            return;
        } else if (n > 0) {
            for (int i = 0; i < n; ++i) {
                int client_sockfd = vec_of_events[i].data.fd, err;
                if ((err = read_request_from_client(&received, client_sockfd)) > 0) {
                    switch(received.request){
                        case c_disconnect:
                        c_disconnect_list_chats:
                            epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);
                            clients_without_chat.erase(client_sockfd);
                            std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                            write(disco_pipe[1], &client_sockfd, sizeof(int));
                            break;
                        case c_create_chat:
                            {
                            resp.request = c_create_chat;
                            int check = creat((received.message_text + ".chat").c_str(), S_IRWXG | S_IRWXO | S_IRWXU);
                            if (errno == EEXIST)
                            {
                                resp.responce = s_failure;
                                resp.message_text = "Chat with such name already exists";
                                if(!send_resp_to_client(&resp, client_sockfd)) goto c_disconnect_list_chats;
                            }
                            else if (check < 0){
                                resp.responce = s_failure;
                                resp.message_text = "Server error: could not create chat, try again later";
                                boost::json::value data = {
                                    {"chat", received.message_text},
                                    {"error", std::error_code{errno, std::generic_category()}.message()},
                                };
                                logger::Logger::GetInstance().Log("Chat creation error"sv, data);
                                if(!send_resp_to_client(&resp, client_sockfd)) goto c_disconnect_list_chats;
                            }
                            else
                            { // if ok -> add the chat to the list of available ones + send list of them
                                chats[received.message_text].subs = {};
                                chats[received.message_text].pipe.in = -1;
                                chats[received.message_text].pipe.out = -1;
                                boost::json::value data = {
                                    {"chat", received.message_text}
                                };
                                logger::Logger::GetInstance().Log("Chat created"sv, data);
                                std::unordered_set<int> closed_sockets;
                                for (int socket : clients_without_chat){
                                    if(!send_available_chats(socket)){ // if client disconnects while being written into
                                        closed_sockets.insert(socket);
                                    }
                                }
                                for(auto socket : closed_sockets){ // handle closed sockets
                                    epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_DEL, socket, &ev);
                                    clients_without_chat.erase(socket);
                                    std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                                    write(disco_pipe[1], &socket, sizeof(int));
                                }

                            }
                            break;
                            }
                        case c_connect_chat:
                        {
                            resp.request = c_connect_chat;
                            bool start_ok = true;
                            if (chats.count(received.message_text) == 0)
                            {
                                resp.responce = s_failure;
                                resp.message_text = "Can't connect to a non-existant chat";
                                break;
                            }
                            //if chat was unused -> setup
                            chats[received.message_text].mtx.lock();
                            if(chats[received.message_text].pipe.in = -1){
                                // close(chats[received.message_text].pipe.out);
                                int tmp[2];
                                if(pipe(tmp) < 0) {
                                    // perror("could not open pipe to chat");
                                    boost::json::value data = {
                                        {"error", std::error_code{errno, std::generic_category()}.message()},
                                        {"chat", received.message_text},
                                    };
                                    logger::Logger::GetInstance().Log("Could not open pipe to chat"sv, data);
                                    start_ok = false;
                                }
                                else{
                                    chats[received.message_text].pipe.in = tmp[0];
                                    chats[received.message_text].pipe.out = tmp[1];
                                    fcntl(chats[received.message_text].pipe.in, F_SETFL, fcntl(chats[received.message_text].pipe.in, F_GETFL) | O_NONBLOCK);
                                    std::thread(chat_message, chats[received.message_text].pipe.in, received.message_text).detach();
                                }
                            }
                            // Adding a subscriber to the chat
                            if(start_ok){
                                chats.at(received.message_text).subs.insert(client_sockfd);
                                resp.responce = s_success;
                                resp.message_text = received.message_text;
                                clients_without_chat.erase(client_sockfd);
                                if(!send_resp_to_client(&resp, client_sockfd)) goto c_disconnect_list_chats;
                                
                                epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);
                                std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                                write(chats[received.message_text].pipe.out, &client_sockfd, sizeof(int));
                                boost::json::value data = {
                                    {"chat", received.message_text},
                                    {"socket", client_sockfd}
                                };
                                logger::Logger::GetInstance().Log("Client connected to chat"sv, data);
                            }
                            else{
                                resp.responce = s_failure;
                                resp.message_text = "";
                                if(!send_resp_to_client(&resp, client_sockfd)) goto c_disconnect_list_chats;
                            }
                            chats[received.message_text].mtx.unlock();
                            break;
                        }
                        default:
                            resp.request = c_wrong_request;
                            // fprintf(stderr, "wrong request from client : %d\n", received.request);
                            boost::json::value data = {
                                {"chat", received.message_text},
                                {"socket", client_sockfd}
                            };
                            logger::Logger::GetInstance().Log("Wrong request from client"sv, data);
                            resp.responce = s_failure;
                            resp.message_text =  "";
                            if(!send_resp_to_client(&resp, client_sockfd)) goto c_disconnect_list_chats;
                    }
                }
                if (err == 0) {
                    epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);
                    clients_without_chat.erase(client_sockfd);
                    std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                    write(disco_pipe[1], &client_sockfd, sizeof(int));
                }
            }
        }
    }
}

void user_name_enter(int end_to_read_from, int end_to_write_to){
    int no_name_epoll_fd = epoll_create1(0);
    if (no_name_epoll_fd < 0) {
        // perror("epoll create");
        boost::json::value data = {
            {"error", std::error_code{errno, std::generic_category()}.message()}
        };
        logger::Logger::GetInstance().Log("Username enter epoll create error"sv, data);
        return;
    }
    std::vector<epoll_event> vec_of_events(10); //empty at start
    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLET;
    server_data_t resp;
    client_data_t received;
    int new_socket, bytes;
    resp.message_text = "";
    while(true){
        while((bytes = read(end_to_read_from, &new_socket, sizeof(int))) > 0) {
            ev.data.fd = new_socket;
            // vec_of_events.push_back(ev);
            epoll_ctl(no_name_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        }
        if(bytes < 0 && errno != EAGAIN){
            // perror("name pipe error");
            boost::json::value data = {
                {"error", std::error_code{errno, std::generic_category()}.message()}
            };
            logger::Logger::GetInstance().Log("Username enter name pipe error"sv, data);
        }
        int n = epoll_wait(no_name_epoll_fd, vec_of_events.data(), vec_of_events.size(), 100);
        if (n < 0 && errno != EINVAL && errno != EINTR) {
            // perror("epoll wait");
            boost::json::value data = {
                {"error", std::error_code{errno, std::generic_category()}.message()}
            };
            logger::Logger::GetInstance().Log("Username enter epoll wait error"sv, data);
            return;
        } else if (n > 0) {
            resp.request = c_set_name;
            for (int i = 0; i < n; ++i) {
                int client_sockfd = vec_of_events[i].data.fd;
                resp.responce = s_failure;
                int err = read_request_from_client(&received, client_sockfd);
                if(received.request == c_disconnect || err == 0){
                    epoll_ctl(no_name_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);
                    std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                    write(disco_pipe[1], &client_sockfd, sizeof(int));
                    boost::json::value data = {
                        {"socket", client_sockfd}
                    };
                    logger::Logger::GetInstance().Log("User disconnected"sv, data);
                    continue;
                }
                if(used_usernames.count(received.message_text) == 0) {
                    user_data[client_sockfd] = received.message_text;
                    used_usernames.insert(received.message_text);
                    resp.responce = s_success;
                    epoll_ctl(no_name_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);

                    if(!send_resp_to_client(&resp, client_sockfd)) write(disco_pipe[1], &client_sockfd, sizeof(int)); // failed to write -> socket is deleted from system

                    std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
                    write(end_to_write_to, &client_sockfd, sizeof(int));

                    boost::json::value data = {
                        {"socket", client_sockfd},
                        {"name", received.message_text}
                    };
                    logger::Logger::GetInstance().Log("User registered"sv, data);
                }
                else if(!send_resp_to_client(&resp, client_sockfd)) write(disco_pipe[1], &client_sockfd, sizeof(int)); // failed to write -> socket is deleted from system

            }
        }
    }
}

void disconnect(){
    int read_bytes, fd;
    while (true) {
        while ((read_bytes = read(disco_pipe[0], &fd, sizeof(int))) > 0) {
            boost::json::value data = {
                {"socket", fd},
                {"name", user_data[fd]}
            };
            logger::Logger::GetInstance().Log("User was disconnected"sv, data);
            end_resp_to_client(fd);
            used_usernames.erase(user_data[fd]);
            user_data.erase(fd);
        }
        if (read_bytes < 0 && errno != EAGAIN && errno != EINTR) {
            boost::json::value data = {
                {"error", std::error_code{errno, std::generic_category()}.message()}
            };
            logger::Logger::GetInstance().Log("Pipe read error disconnected"sv, data);
        }
    }
}
