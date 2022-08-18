#include "server_interface.h"

extern std::unordered_map<std::string, std::pair<std::unordered_set<int>, chat_pipe>> chats;
extern std::unordered_set<std::string> used_usernames;
extern std::unordered_map<int, std::string> user_data;

static int server_running = 1;
int disco_pipe;
void catch_signals()
{
    server_running = 0;
}

void chat_message(int go_in_chat_pipe_input, int disconnect_pipe_output, int exit_chat_pipe_output) {
    std::string all_messages;
    client_data_t client_data;
    server_data_t server_data;

    int chat_epoll_fd = epoll_create1(0);
    if (chat_epoll_fd < 0) {
        perror("epoll create");
        return;
    }
    std::vector<epoll_event> vec_of_events;
    std::unordered_map<int, size_t> messages_offset;
    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLET;

    time_t curr_time;
	tm * curr_tm;
    char timedate_string[100];
    size_t off = 0;

    while (true) {
        // changing vector of events
        int read_bytes;
        while ((read_bytes = read(go_in_chat_pipe_input, (void*)ev.data.fd, sizeof(int))) > 0) {
            vec_of_events.push_back(ev);
            messages_offset[ev.data.fd] = 0;
            epoll_ctl(chat_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
        }
        // wait for messages
        int n = epoll_wait(chat_epoll_fd, vec_of_events.data(), vec_of_events.size(), 25);
        if (n < 0) {
            perror("epoll wait");
            close(chat_epoll_fd);
            return;
        } else if (n > 0) {
            time(&curr_time);
            curr_tm = localtime(&curr_time);
            strftime(timedate_string, sizeof(timedate_string) - 1, "%T %D\0", curr_tm);

            for (off = 0; off < n; ++off) {
                int err;
                int& fd = vec_of_events[off].data.fd;
                while ((err = read_request_from_client(&client_data, fd)) > 0) {
                    //depend on type of message do what to need to
                    switch (client_data.request)
                    {
                        case c_leave_chat:
                            write(exit_chat_pipe_output, &fd, sizeof(int));
                            epoll_ctl(chat_epoll_fd, EPOLL_CTL_DEL, fd, &ev);
                            messages_offset.erase(fd);
                            break;
                        case c_disconnect:
                            write(disconnect_pipe_output, &fd, sizeof(int));
                            epoll_ctl(chat_epoll_fd, EPOLL_CTL_DEL, fd, &ev);
                            messages_offset.erase(fd);
                            break;
                        case c_send_message:
                            all_messages.append(user_data[fd]);
                            all_messages.push_back('\t');
                            all_messages.append(timedate_string);
                            all_messages.push_back('\n');
                            all_messages.append(client_data.message_text);
                            all_messages.push_back('\0');
                            break;
                        default:
                            fprintf(stderr, "Client %d send unacceptable message\n", fd);
                            break;
                    }
                }
                if (err == 0)
                    write(disconnect_pipe_output, &fd, sizeof(int));
            }
        }
        // TODO : write to file
        // sending data to all sockets
        if (messages_offset.size() != vec_of_events.size())
            vec_of_events.resize(messages_offset.size());
        for (auto& [fd, off] : messages_offset) {
            server_data.responce = s_new_message;
            server_data.message_text = all_messages.substr(off);
            send_resp_to_client(&server_data, fd);
            off = all_messages.size();
            ++n;
        }
    }
    close(chat_epoll_fd);
}

void list_of_chats(int end_to_read_from) {
    int no_chat_epoll_fd = epoll_create1(0);
    if (no_chat_epoll_fd < 0) {
        perror("epoll create");
        return;
    }
    std::vector<epoll_event> vec_of_events; //empty at start
    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLET;

    server_data_t resp;
    client_data_t received;
    int new_socket, bytes;
    while(true){
        while ((bytes = read(end_to_read_from, &new_socket, sizeof(int))) > 0) {
            ev.data.fd = new_socket;
            vec_of_events.push_back(ev);
            epoll_ctl(no_chat_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
            get_available_chats(new_socket);
        }
        if(bytes < 0){
            perror("List of chats pipe error");
        }
        int n = epoll_wait(no_chat_epoll_fd, vec_of_events.data(), vec_of_events.size(), 100);
        if (n < 0) {
            perror("epoll wait");
            return;
        } else if (n > 0) {
            for (int i = 0; i < n; ++i) {
                int client_sockfd = vec_of_events[i].data.fd;
                while (read_request_from_client(&received, client_sockfd) > 0) {
                    switch(received.request){
                        case c_disconnect:
                        end_resp_to_client(client_sockfd);
                        break;
                        case c_create_chat:
                            creat(received.message_text.c_str(), S_IRWXG | S_IRWXO | S_IRWXU);
                            if (errno == EEXIST)
                            {
                                resp.responce = s_failure;
                                resp.message_text = "Chat with such name already exists";
                            }
                            else
                            { // if ok -> add the chat to the list of available ones
                                resp.responce = s_success;
                                chats[received.message_text] = {};
                            }
                            break;
                        case c_connect_chat:
                            if (chats.count(received.message_text) == 0)
                            {
                                resp.responce = s_failure;
                                resp.message_text = "Can't connect to a non-existant chat";
                                break;
                            }
                            //if chat was unused -> setup
                            if(chats[received.message_text].first.size() == 0){
                                int tmp_pipe[2];
                                if(pipe(tmp_pipe) < 0) {
                                    perror("Could not open pipe to chat");
                                }
                                chats[received.message_text].second.in = tmp_pipe[1];
                                chats[received.message_text].second.out = tmp_pipe[0];
                                std::thread(chat_message, tmp_pipe[1], disco_pipe[1], end).detach();//auto chat_thread = 
                            }
                            // Adding a subscriber to the chat
                            chats.at(received.message_text).first.insert(client_sockfd);
                            resp.responce = s_success;
                            // sending status to client
                            struct stat st;
                            stat(received.message_text.c_str(), &st);
                            write(client_sockfd, &st.st_size, sizeof(off_t));
                            // sending sockfd to chat
                            write(chats[received.message_text].second.in, &client_sockfd, sizeof(int));
                            break;
                        default:
                            fprintf(stderr, "Wrong request from client : %d\n", received.request);
                            resp.responce = s_failure;
                            resp.message_text =  "WRONG REQUEST";
                    }
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
    while(true){
        while(bytes = read(end_to_read_from, &new_socket, sizeof(int))) {
            ev.data.fd = new_socket;
            vec_of_events.push_back(ev);
            epoll_ctl(no_name_epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
            get_available_chats(new_socket);
        }
        if(bytes < 0){
            perror("Name pipe error");
        }
        int n = epoll_wait(no_chat_epoll_fd, vec_of_events.data(), vec_of_events.size(), 100);
        if (n < 0) {
            perror("epoll wait");
            return;
        } else if (n > 0) {
            for (int i = 0; i < n; ++i) {
                int client_sockfd = vec_of_events[i].data.fd;
                resp.responce = s_failure;
                read_request_from_client(received, client_sockfd);
                if(used_usernames.count(received.message_text) == 0) {
                    // user_data[client_sockfd] = received.message_text;
                    used_usernames.insert(received.message_text);
                    resp.responce = s_success;
                    write(end_to_write_to, &client_sockfd, sizeof(int));
                    it = no_name_sockets.erase(it);
                }
                send_resp_to_client(resp, client_sockfd);
            }
        }
    }
}

void disconnect(int user_name_delete, int list_of_chat_delete){
    int read_bytes, fd;
    while (true) {
        while ((read_bytes = read(user_name_delete, &fd, sizeof(int))) > 0) {
            close(fd);
            used_usernames.erase(user_data[fd]);
            user_data.erase(fd);
        }
        if (read_bytes < 0)
            perror("Read disconnected user from user name");
        while ((read_bytes = read(list_of_chat_delete, &fd, sizeof(int))) > 0) {
            close(fd);
            used_usernames.erase(user_data[fd]);
            user_data.erase(fd);
        }
        if (read_bytes < 0)
            perror("Read disconnected user from list of chats");
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
    listen_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    auto param = 1;
    setsockopt(listen_socket, SOL_SOCKET, SOCK_NONBLOCK, (void *)&param, sizeof(int));
    bzero(&address, sizeof(address));
    address.sin_port = htons(5000); // TO DO: CHANGE TO CONSTANT
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1"); // input of machine's adress (CHANGE TO CONSTANT)
    /* The call to the function "bind()" assigns the details specified
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

    if(pipe(disco_pipe) < 0){
        perror("Disconnection pipe error");
        shutdown(listen_socket, SHUT_RDWR);
        return EXIT_FAILURE;
    }

    struct sockaddr_in client_address;
    socklen_t address_length = sizeof(sockaddr_in);
    int connection_socket;
    while ((connection_socket = accept(listen_socket, (struct sockaddr *)&client_address, &address_length)) != -1)
    {
        //
    }
    if (!(errno & (EAGAIN | EWOULDBLOCK)))
    {
        perror("Accept failure");
        return;
    }
    perror("Server died with status");
    server_ending(); // because why not
    exit(EXIT_SUCCESS);
}
