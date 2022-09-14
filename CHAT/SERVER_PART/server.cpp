#include "server_interface.h"
#include "server_routines.h"

extern std::unordered_map<std::string, std::pair<std::unordered_set<int>, chat_pipe>> chats;
extern std::unordered_set<std::string> used_usernames;
extern std::unordered_map<int, std::string> user_data;

int disco_pipe[2], list_chats_pipe[2], username_enter_pipe[2];

void chat_message(int go_in_chat_pipe_input, std::string chat_name) {
    client_data_t client_data;
    server_data_t server_data;

    int chat_epoll_fd = epoll_create1(0), chatfd;
    if (chat_epoll_fd < 0) {
        perror("epoll create");
        return;
    }
    std::fstream chat_file(chat_name + ".chat");
    if (!chat_file) {
        perror("chat file");
        return;
    }
    std::vector<epoll_event> vec_of_events;
    std::unordered_map<int, std::streamoff> messages_offset;
    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLET;

    time_t curr_time;
	tm * curr_tm;
    size_t off = 0;

    while (true) {
        // changing hashmap of events
        int read_bytes;
        while ((read_bytes = read(go_in_chat_pipe_input, (void*)(&ev.data.fd), sizeof(int))) > 0) {
            vec_of_events.push_back(ev);
            messages_offset[ev.data.fd] = 0;
            epoll_ctl(chat_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        }
        if (read_bytes == 0) {
            fprintf(stderr, "closing routine for chat: %s\n", chat_name.c_str());
            close(go_in_chat_pipe_input);
            close(chat_epoll_fd);
            chat_file.close();
            return;
        }
        if (read_bytes < 0 && errno != EAGAIN && errno != EINTR) {
            perror("pipe");
            close(chat_epoll_fd);
            chat_file.close();
            return;
        }
        // wait for messages
        char timedate_string[100] = { 0 };
        int n = epoll_wait(chat_epoll_fd, vec_of_events.data(), vec_of_events.size(), 25);
        if (n < 0 && errno != EINVAL && errno != EINTR) {
            perror("epoll wait");
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
                            // server_data.request = c_leave_chat;
                            // server_data.message_text = "";
                            // send_resp_to_client(&server_data, fd);
                            if(write(list_chats_pipe[1], &fd, sizeof(int)) < 0){
                                perror("Failed to send client's fd to no_chat routine");
                            }
                            epoll_ctl(chat_epoll_fd, EPOLL_CTL_DEL, fd, &ev);
                            messages_offset.erase(fd);
                            chats[chat_name].first.erase(fd); //unsubscribing
                            break;
                        case c_disconnect:
                            write(disco_pipe[1], &fd, sizeof(int));
                            epoll_ctl(chat_epoll_fd, EPOLL_CTL_DEL, fd, &ev);
                            messages_offset.erase(fd);
                            chats[chat_name].first.erase(fd); //unsubscribing
                            break;
                        case c_send_message:
                            chat_file << user_data[fd] << '\t';
                            chat_file.write(timedate_string, strlen(timedate_string));
                            chat_file << std::endl << client_data.message_text << "\r";
                            break;
                        default:
                            fprintf(stderr, "Client %d send unacceptable message\n", fd);
                            break;
                    }
                }
                if (err == 0) {
                    epoll_ctl(chat_epoll_fd, EPOLL_CTL_DEL, fd, &ev);
                    write(disco_pipe[1], &fd, sizeof(int));
                    messages_offset.erase(fd);
                }
            }
        }
        if (messages_offset.size() != vec_of_events.size())
            vec_of_events.resize(messages_offset.size());
        // sending data to all clients
        chat_file.seekp(0, chat_file.end);
        auto file_size = chat_file.tellp();
        server_data.request = c_receive_message;
        for (auto& [fd, off] : messages_offset) {
            chat_file.seekg(off, chat_file.beg);
            while (off != file_size) {
                //maybe check for chat_file.bad()
                std::getline(chat_file, server_data.message_text, '\r');
                off = chat_file.tellp();
                send_resp_to_client(&server_data, fd);
            }
        }
    }
    close(chat_epoll_fd);
    chat_file.close();
}

void list_of_chats(int end_to_read_from) {
    int no_chat_epoll_fd = epoll_create1(0);
    if (no_chat_epoll_fd < 0) {
        perror("epoll create");
        return;
    }
    std::vector<epoll_event> vec_of_events; //empty at start
    std::unordered_set<int> clients_without_chat;
    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLET;

    server_data_t resp;
    client_data_t received;
    int new_socket, bytes;
    while(true){
        while ((bytes = read(end_to_read_from, &new_socket, sizeof(int))) > 0) {
            ev.data.fd = new_socket;
            clients_without_chat.insert(new_socket);
            vec_of_events.push_back(ev);
            epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
            send_available_chats(new_socket);
        }
        if(bytes < 0 && errno != EAGAIN && errno != EINTR){
            perror("list of chats pipe error");
        }
        int n = epoll_wait(no_chat_epoll_fd, vec_of_events.data(), vec_of_events.size(), 100);
        if (n < 0 && errno != EINVAL && errno != EINTR) {
            perror("epoll wait");
            return;
        } else if (n > 0) {
            for (int i = 0; i < n; ++i) {
                int client_sockfd = vec_of_events[i].data.fd, err;
                if (err = read_request_from_client(&received, client_sockfd) > 0) {
                    switch(received.request){
                        case c_disconnect:
                            write(disco_pipe[1], &client_sockfd, sizeof(int));
                            clients_without_chat.erase(client_sockfd);
                            epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);
                            break;
                        case c_create_chat:
                            {
                            resp.request = c_create_chat;
                            int check = creat((received.message_text + ".chat").c_str(), S_IRWXG | S_IRWXO | S_IRWXU);
                            if (errno == EEXIST)
                            {
                                resp.responce = s_failure;
                                resp.message_text = "Chat with such name already exists";
                                send_resp_to_client(&resp, client_sockfd);
                            }
                            else if (check < 0){
                                resp.responce = s_failure;
                                resp.message_text = "Server error: could not create chat, try again later";
                                perror("chat creation error");
                                send_resp_to_client(&resp, client_sockfd);
                            }
                            else
                            { // if ok -> add the chat to the list of available ones + send list of them
                                chats[received.message_text] = {};
                                for (int socket : clients_without_chat)
                                    send_available_chats(socket);
                            }
                            break;
                            }
                        case c_connect_chat:
                            resp.request = c_connect_chat;
                            if (chats.count(received.message_text) == 0)
                            {
                                resp.responce = s_failure;
                                resp.message_text = "Can't connect to a non-existant chat";
                                break;
                            }
                            //if chat was unused -> setup
                            if(chats[received.message_text].first.size() == 0){
                                close(chats[received.message_text].second.out);
                                if(pipe((int *)&(chats[received.message_text].second)) < 0) {
                                    perror("could not open pipe to chat");
                                }
                                fcntl(chats[received.message_text].second.in, F_SETFL, fcntl(chats[received.message_text].second.in, F_GETFL) | O_NONBLOCK);
                                std::thread(chat_message, chats[received.message_text].second.in, received.message_text).detach();
                            }
                            // Adding a subscriber to the chat
                            chats.at(received.message_text).first.insert(client_sockfd);
                            resp.responce = s_success;
                            resp.message_text = "";
                            clients_without_chat.erase(client_sockfd);
                            send_resp_to_client(&resp, client_sockfd);
                            write(chats[received.message_text].second.out, &client_sockfd, sizeof(int));
                            epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);
                            break;
                        default:
                            resp.request = c_wrong_request;
                            fprintf(stderr, "wrong request from client : %d\n", received.request);
                            resp.responce = s_failure;
                            resp.message_text =  "";
                            send_resp_to_client(&resp, client_sockfd);
                    }
                }
                if (err == 0) {
                    epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);
                    clients_without_chat.erase(client_sockfd);
                    write(disco_pipe[1], &client_sockfd, sizeof(int));
                }
            }
        }
    }
}

void user_name_enter(int end_to_read_from, int end_to_write_to){
    int no_name_epoll_fd = epoll_create1(0);
    if (no_name_epoll_fd < 0) {
        perror("epoll create");
        return;
    }
    std::vector<epoll_event> vec_of_events; //empty at start
    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLET;
    server_data_t resp;
    client_data_t received;
    int new_socket, bytes;
    resp.message_text = "";
    while(true){
        while((bytes = read(end_to_read_from, &new_socket, sizeof(int))) > 0) {
            ev.data.fd = new_socket;
            vec_of_events.push_back(ev);
            epoll_ctl(no_name_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        }
        if(bytes < 0 && errno != EAGAIN){
            perror("name pipe error");
        }
        int n = epoll_wait(no_name_epoll_fd, vec_of_events.data(), vec_of_events.size(), 100);
        if (n < 0 && errno != EINVAL && errno != EINTR) {
            perror("epoll wait");
            return;
        } else if (n > 0) {
            resp.request = c_set_name;
            for (int i = 0; i < n; ++i) {
                int client_sockfd = vec_of_events[i].data.fd;
                resp.responce = s_failure;
                int err = read_request_from_client(&received, client_sockfd);
                if(received.request == c_disconnect || err == 0){
                    write(disco_pipe[1], &client_sockfd, sizeof(int));
                    epoll_ctl(no_name_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);
                    continue;
                }
                if(used_usernames.count(received.message_text) == 0) {
                    user_data[client_sockfd] = received.message_text;
                    used_usernames.insert(received.message_text);
                    resp.responce = s_success;
                    send_resp_to_client(&resp, client_sockfd);
                    write(end_to_write_to, &client_sockfd, sizeof(int));
                    epoll_ctl(no_name_epoll_fd, EPOLL_CTL_DEL, client_sockfd, &ev);
                }
                else send_resp_to_client(&resp, client_sockfd);

            }
        }
    }
}

void disconnect(){
    int read_bytes, fd;
    while (true) {
        while ((read_bytes = read(disco_pipe[0], &fd, sizeof(int))) > 0) {
            end_resp_to_client(fd);
            used_usernames.erase(user_data[fd]);
            user_data.erase(fd);
        }
        if (read_bytes < 0 && errno != EAGAIN && errno != EINTR) {
            perror("read disconnected user_fd from pipe");
        }
    }
}


int main(int argc, char *argv[])
{
    using namespace std::chrono_literals;
    if (!server_starting())
        exit(EXIT_FAILURE);
    int listen_socket;
    struct sockaddr_in address;
    // fd_set readfds;

    /* creates an UN-named socket inside the kernel and returns
     * an integer known as socket descriptor
     * This function takes domain/family as its first argument.
     * For Internet family of IPv4 addresses we use AF_INET
     */
    listen_socket = socket(AF_INET, SOCK_STREAM /*| SOCK_NONBLOCK*/, IPPROTO_TCP);
    bzero(&address, sizeof(address));
    address.sin_port = htons(5000);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    /* 
     * The call to the function "bind()" assigns the details specified
     * in the structure address to the socket created in the step above
     */
    if (bind(listen_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        perror("Bind error");
        close(listen_socket);
        return EXIT_FAILURE;
    }

    if (listen(listen_socket, 1000) == -1)
    {
        perror("Listen error");
        shutdown(listen_socket, SHUT_RDWR);
        return EXIT_FAILURE;
    }
    if(pipe(list_chats_pipe) < 0){
        perror("List of chats pipe opening error");
        shutdown(listen_socket, SHUT_RDWR);
        return EXIT_FAILURE;
    }
    if(pipe(disco_pipe) < 0){
        perror("Disconnection pipe opening error");
        shutdown(listen_socket, SHUT_RDWR);
        close(list_chats_pipe[0]);
        close(list_chats_pipe[1]);
        return EXIT_FAILURE;
    }
    if(pipe(username_enter_pipe) < 0){
        perror("Username enter pipe opening error");
        shutdown(listen_socket, SHUT_RDWR);
        close(list_chats_pipe[0]);
        close(list_chats_pipe[1]);
        close(disco_pipe[0]);
        close(disco_pipe[1]);
        return EXIT_FAILURE;
    }
    fcntl(list_chats_pipe[0], F_SETFL, fcntl(list_chats_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(disco_pipe[0], F_SETFL, fcntl(disco_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(username_enter_pipe[0], F_SETFL, fcntl(username_enter_pipe[0], F_GETFL) | O_NONBLOCK);

    std::thread(user_name_enter, username_enter_pipe[0], list_chats_pipe[1]).detach();
    std::thread(list_of_chats, list_chats_pipe[0]).detach();
    std::thread(disconnect).detach();

    struct sockaddr_in client_address;
    socklen_t address_length = sizeof(sockaddr_in);
    int connection_socket;
    while ((connection_socket = accept(listen_socket, (struct sockaddr *)&client_address, &address_length)) != -1)
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = TIMEOUT_MICRO;
        setsockopt(connection_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        write(username_enter_pipe[1], &connection_socket, sizeof(int));
    }
    if (!(errno & (EAGAIN | EWOULDBLOCK)))
    {
        perror("Accept failure");
        return EXIT_FAILURE;
    }
    perror("Server died with status");
    server_ending(); // because why not
    exit(EXIT_SUCCESS);
}
