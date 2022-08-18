#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h> /* "Главный" по прослушке */
#include <sys/types.h>

// inet bullshit
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <netdb.h>    //for gethostname
#include <dirent.h>   //work with file system
#include <errno.h>    //error codes and so on
#include <sys/stat.h> // additional file functions & info
// #include <semaphore.h>
#include <string.h> //working with old style strings

#include "server_interface.h"

#include <ctime>
#include <vector>
#include <thread>
#include <future>
// #include <tuple>
#include <chrono>
#include <atomic>
#include <unordered_set>
#include <algorithm>

extern std::unordered_map<std::string, clients> chats; //chats_subs_mutex was -> each el mutex
extern std::unordered_set<std::string> used_usernames;
extern std::unordered_map<int, std::string> user_data;
extern clients without_name, without_chat, disconnected;


std::mutex create_mutex;
std::mutex chats_subs_mutex;

static int server_running = 1;
void catch_signals()
{
    server_running = 0;
}

void chat_message(clients& chat) {
    std::string all_messages;
    client_data_t client_data;
    int chat_epoll_fd = epoll_create1(0);
    if (chat_epoll_fd < 0) {
        perror("epoll create");
        return;
    }
    std::vector<epoll_event> vec_of_events;
    std::vector<size_t> messages_offset;
    epoll_event ev;
    ev.events = EPOLLEXCLUSIVE | EPOLLIN | EPOLLET;

    time_t curr_time;
	tm * curr_tm;
    char timedate_string[100];

    while (true) {
        // changing vector of events
        chat.client_mut.lock();
        size_t off = vec_of_events.size();
        vec_of_events.reserve(chat.client_fds.size());
        messages_offset.resize(chat.client_fds.size());
        for (; off < chat.client_fds.size(); ++off) {
            ev.data.fd = chat.client_fds[off];
            vec_of_events[off] = ev;
            epoll_ctl(chat_epoll_fd, EPOLL_CTL_ADD, chat.client_fds[off], (epoll_event*)vec_of_events.data());
        }
        if (chat.client_fds.empty()) {
            chat.client_mut.unlock();
            return;
        }
        chat.client_mut.unlock();
        // wait for messages
        int n = epoll_wait(chat_epoll_fd, vec_of_events.data(), vec_of_events.size(), 100);
        std::unordered_set<int> to_delete;
        if (n < 0) {
            perror("epoll wait");
            return;
        } else if (n > 0) {
            time(&curr_time);
            curr_tm = localtime(&curr_time);
            strftime(timedate_string, 50, "%T %D", curr_tm);

            for (off = 0; off < n; ++off) {
                int err;
                while ((err = read_request_from_client(&client_data, vec_of_events[off].data.fd)) > 0) {
                    all_messages.append(user_data[vec_of_events[off].data.fd]);
                    all_messages.push_back('\t');
                    all_messages.append(timedate_string);
                    all_messages.push_back('\n');
                    all_messages.append(client_data.message_text);
                    all_messages.push_back('\0');
                }
                if (err == 0) {
                    disconnected.client_mut.lock();
                    disconnected.client_fds.push_back(vec_of_events[off].data.fd);
                    disconnected.client_mut.unlock();
                    to_delete.insert(vec_of_events[off].data.fd);
                }
            }
        }
        //deleting client fds
        if (to_delete.size()) {
            chat.client_mut.lock();
            for (off = 0; off < chat.client_fds.size(); ++off) {
                if (to_delete.find(chat.client_fds[off]) != to_delete.end()) {
                    std::swap(chat.client_fds[off], chat.client_fds.back());
                    chat.client_fds.pop_back();
                }
            }
        }
        chat.client_mut.unlock();
        // TODO : write to file
        // sending data to all sockets
        for (int client : chat.client_fds) {
            //
        }
    }
}

void list_of_chats() {
    int noChat_epoll_fd = epoll_create1(0);
    if (noChat_epoll_fd < 0) {
        perror("epoll create");
        return;
    }
    static std::vector<epoll_event> vec_of_events; //empty at start
    static size_t new_without_chat = 0; // 0 only at start
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    server_data_t resp;
    client_data_t received;
    without_chat.client_mut.lock();
    vec_of_events.reserve(without_chat.client_fds.size());
    for(int i = new_without_chat; i < without_chat.client_fds.size(); ++i) {
        ev.data.fd = without_chat.client_fds[i];
        vec_of_events[i] = ev;
        epoll_ctl(noChat_epoll_fd, EPOLL_CTL_ADD, without_chat.client_fds[i], (epoll_event*)vec_of_events.data());
        get_available_chats(without_chat.client_fds[i]);
    }
    new_without_chat = without_chat.client_fds.size();
    int n = epoll_wait(noChat_epoll_fd, vec_of_events.data(), vec_of_events.size(), 100);
    if (n < 0) {
        perror("epoll wait");
        return;
    } else if (n > 0) {
        for (int i = 0; i < n; ++i) {
            int err;
            while (read_request_from_client(&received, vec_of_events[i].data.fd) > 0) {
                switch(received.request){
                    case c_disconnect:
                    disconnected.client_mut.lock();
                    disconnected.client_fds.push_back(vec_of_events[i].data.fd);
                    without_chat.client_mut.lock();
                    without_chat.client_fds.erase(find(without_chat.client_fds.begin(), without_chat.client_fds.end(), vec_of_events[i].data.fd)); // holy fuck it's nasty
                    break;
                    case c_create_chat:
                        create_mutex.lock();
                        if (received.message_text.size() > MAX_CHAT_NAME_LEN)
                        {
                            resp.responce = s_failure;
                            resp.message_text = "Chat's name is too big";
                            break;
                        }
                        creat(received.message_text.c_str(), S_IRWXG | S_IRWXO | S_IRWXU);
                        chats_subs_mutex.lock();
                        if (errno == EEXIST)
                        {
                            resp.responce = s_failure;
                            resp.message_text = "Chat with such name already exists";
                        }
                        else
                        { // if ok -> add the chat to the list of available ones
                            resp.responce = s_success;
                            chats[received.message_text] = std::make_pair(std::mutex(), std::unordered_set<int>());
                        }
                        create_mutex.unlock();
                        chats_subs_mutex.unlock();
                        break;
                    case c_connect_chat:
                        chats_subs_mutex.lock();
                        if (chats.count(received.message_text) == 0)
                        {
                            resp.responce = s_failure;
                            resp.message_text = "Can't connect to a non-existant chat";
                            break;
                        }
                        connected_chat = std::string(received.message_text);
                        // Adding a subscriber to the chat
                        chats.at(received.message_text).second.insert(client_sockfd);
                        resp.responce = s_success;
                        chats_subs_mutex.unlock();
                        // sending it to client
                        struct stat st;
                        stat(received.message_text.c_str(), &st);
                        write(client_sockfd, &st.st_size, sizeof(off_t));
                        // and so on
                        resp.responce = s_success;
                        chats.at(received.message_text).first.lock();
                        int chatfd;
                        if ((chatfd = open(received.message_text.c_str(), O_RDONLY)) > 0)
                        {
                            char buf[256];
                            for (size_t i = 0; i < st.st_size; i += 256)
                            {
                                if (read(chatfd, buf, 256) < 0)
                                {
                                    perror("Failed to read from chat");
                                    resp.responce = s_failure;
                                    resp.message_text = "Server error: could not read chat";
                                    break;
                                }
                                resp.message_text = buf;
                                send_resp_to_client(&resp, client_sockfd);
                                // if (send_resp_to_client(resp, client_sockfd) < 0)
                                // {
                                //     perror("Failed to transmit whole chat");
                                //     resp->responce = s_failure;
                                //     strcpy(resp->message_text, "Server error: failed to transmit whole chat");
                                //     break;
                                // }
                            }
                            resp.message_text.clear();
                            resp.responce = s_resp_end;
                        }
                        else
                        {
                            perror("Could not open chat on demand");
                            resp.responce = s_failure;
                            resp.message_text = "Server error: could not open requested chat";
                        }
                        //chat_mutex.unlock();
                        break;
                    default:
                        fprintf(stderr, "Wrong request from client : %d\n", received.request);
                        resp.responce = s_failure;
                        resp.message_text =  "WRONG REQUEST";
                }
            }
        }
    }
    without_chat.client_mut.unlock();
}

void user_name_enter(){
    auto resp = new server_data_t;
    auto received = new client_data_t;
    without_name.client_mut.lock();
    for(auto client_sockfd : without_name.client_fds){
        resp->responce = s_failure;
        while(resp->responce == s_failure){
            read_request_from_client(received, client_sockfd);
            if(used_usernames.count(received->message_text) == 0) {
                user_data[client_sockfd] = std::string(received->message_text);
                used_usernames.insert(received->message_text);
                resp->responce = s_success;
            }
            send_resp_to_client(resp, client_sockfd);
        }
    }
    without_chat.client_mut.lock();
    without_chat.client_fds.insert(without_chat.client_fds.end(), without_name.client_fds.begin(), without_name.client_fds.end());
    without_name.client_fds.clear();
    without_name.client_mut.unlock();
    without_chat.client_mut.unlock();
    delete received;
    delete resp;
}

void disconnect(){
    disconnected.client_mut.lock();
    for(auto client_sockfd : disconnected.client_fds){
        end_resp_to_client(client_sockfd);
    }
    disconnected.client_fds.clear();
    disconnected.client_mut.unlock();
}

void unsubscribed_manager(){
    while(true){
        user_name_enter();
        list_of_chats();
        disconnect();
    }
}

void routine(int client_sockfd)
{
    client_data_t *received = new client_data_t;
    server_data_t *resp = new server_data_t;
    std::string connected_chat, user_name;
    // first of all, we need to read clients name
    resp->responce = s_failure;
    while(resp->responce == s_failure){
        read_request_from_client(received, client_sockfd);
        user_name = std::string(received->message_text);
        if(used_usernames.count(user_name) == 0) resp->responce = s_success;
        send_resp_to_client(resp, client_sockfd);
    }
    get_available_chats(client_sockfd);
    // after successfull reading of the name, we can work
    bool subscribed = false;
    while (true)
    { // data processing worker
        read_request_from_client(received, client_sockfd);
        // if(subscribed){
        //     switch(received->request){
        //         case c_disconnect:
        //             end_resp_to_client(client_sockfd);
        //             free(received);
        //             free(resp);
        //             exit(EXIT_SUCCESS);
        //         case c_leave_chat:
        //             chats_subs_mutex.lock();
        //             if (chats.count(received->message_text))
        //             {
        //                 chats.at(received->message_text).erase(client_sockfd);
        //                 resp->responce = s_success;
        //             }
        //             else
        //             {
        //                 resp->responce = s_failure;
        //                 resp->message_text = "Server error: could not leave chat";
        //             }
        //             chats_subs_mutex.unlock();
        //             break;
        //         case c_send_message:
        //             chat_mutex.lock();
        //             int chatfd = open(connected_chat.c_str(), O_WRONLY);
        //             chats_subs_mutex.lock();
        //             if (chatfd > 0)
        //             {
        //                 write(chatfd, user_name.c_str(), 32);
        //                 write(chatfd, received->message_text.c_str(), received->message_text.size());
        //                 if (close(chatfd) < 0)
        //                     perror("Could not close connected chat");
        //                 resp->responce = s_new_message;
        //                 for (auto subs_socket : chats.at(connected_chat))
        //                 {
        //                     if (client_sockfd != subs_socket)
        //                     {
        //                         resp->responce = s_new_message;
        //                         resp->message_text = user_name.c_str();
        //                         send_resp_to_client(resp, subs_socket);
        //                         resp->message_text = received->message_text;
        //                         send_resp_to_client(resp, subs_socket);
        //                     }
        //                 }
        //                 resp->responce = s_success;
        //             }
        //             else
        //             {
        //                 resp->responce = s_failure;
        //                 resp->message_text = "Can't write this message: no chat is opened";
        //             }
        //             chat_mutex.unlock();
        //             chats_subs_mutex.unlock();
        //             break;
        //         default:
        //             fprintf(stderr, "Wrong request from client : %d\n", received->request);
        //             resp->responce = s_failure;
        //             resp->message_text =  "WRONG REQUEST";
        //     }
        // }
        // else{
            switch(received->request){
                case c_disconnect:
                    end_resp_to_client(client_sockfd);
                    exit(EXIT_SUCCESS);
                case c_create_chat:
                    create_mutex.lock();
                    if (received->message_text.size() > MAX_CHAT_NAME_LEN)
                    {
                        resp->responce = s_failure;
                        resp->message_text = "Chat's name is too big";
                        break;
                    }
                    creat(received->message_text.c_str(), S_IRWXG | S_IRWXO | S_IRWXU);
                    chats_subs_mutex.lock();
                    if (errno == EEXIST)
                    {
                        resp->responce = s_failure;
                        resp->message_text = "Chat with such name already exists";
                    }
                    else
                    { // if ok -> add the chat to the list of available ones
                        resp->responce = s_success;
                        chats[received->message_text] = std::make_pair(std::mutex(), std::unordered_set<int>());
                    }
                    create_mutex.unlock();
                    chats_subs_mutex.unlock();
                    break;
                case c_connect_chat:
                    chats_subs_mutex.lock();
                    if (chats.count(received->message_text) == 0)
                    {
                        resp->responce = s_failure;
                        resp->message_text = "Can't connect to a non-existant chat";
                        break;
                    }
                    connected_chat = std::string(received->message_text);
                    // Adding a subscriber to the chat
                    chats.at(received->message_text).second.insert(client_sockfd);
                    resp->responce = s_success;
                    chats_subs_mutex.unlock();
                    // sending it to client
                    struct stat st;
                    stat(received->message_text.c_str(), &st);
                    write(client_sockfd, &st.st_size, sizeof(off_t));
                    // and so on
                    resp->responce = s_success;
                    chats.at(received->message_text).first.lock();
                    int chatfd;
                    if ((chatfd = open(received->message_text.c_str(), O_RDONLY)) > 0)
                    {
                        char buf[256];
                        for (size_t i = 0; i < st.st_size; i += 256)
                        {
                            if (read(chatfd, buf, 256) < 0)
                            {
                                perror("Failed to read from chat");
                                resp->responce = s_failure;
                                resp->message_text = "Server error: could not read chat";
                                break;
                            }
                            resp->message_text = buf;
                            send_resp_to_client(resp, client_sockfd);
                            // if (send_resp_to_client(resp, client_sockfd) < 0)
                            // {
                            //     perror("Failed to transmit whole chat");
                            //     resp->responce = s_failure;
                            //     strcpy(resp->message_text, "Server error: failed to transmit whole chat");
                            //     break;
                            // }
                        }
                        resp->message_text.clear();
                        resp->responce = s_resp_end;
                    }
                    else
                    {
                        perror("Could not open chat on demand");
                        resp->responce = s_failure;
                        resp->message_text = "Server error: could not open requested chat";
                    }
                    chat_mutex.unlock();
                    break;
                default:
                    fprintf(stderr, "Wrong request from client : %d\n", received->request);
                    resp->responce = s_failure;
                    resp->message_text =  "WRONG REQUEST";
            }
        // }
        send_resp_to_client(resp, client_sockfd);
    }
    free(received);
    free(resp);
    return;
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

    struct sockaddr_in client_address;
    socklen_t address_length = sizeof(sockaddr_in);
    int connection_socket;
    while ((connection_socket = accept(listen_socket, (struct sockaddr *)&client_address, &address_length)) != -1)
    {
        without_name.client_mut.lock();
        without_name.client_fds.push_back(connection_socket);
        without_name.client_mut.unlock();
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
