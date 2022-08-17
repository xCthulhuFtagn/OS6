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

#include <vector>
#include <thread>
#include <future>
// #include <tuple>
#include <chrono>
// #include <atomic>
#include <unordered_set>

extern std::unordered_map<std::string, std::unordered_set<int>> chats;
extern std::unordered_set<std::string> used_usernames;
extern std::unordered_map<int, std::string> user_data;

static int server_running = 1;
std::mutex create_mutex;
std::mutex chat_mutex;
std::mutex chats_subs_mutex;

void catch_signals()
{
    server_running = 0;
}

void chat_routine() {
    std::string all_messages;
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll create");
        return;
    }
    std::vector<epoll_event> vec_of_events;
    epoll_event ev;
    while (epoll_wait(epoll_fd, vec_of_events.data(), vec_of_events.size(), -1) > 0) {
    // получение данных с сокетов
        //
    // запись в файл
    // отправка данных по всем сокетам
    }
    perror("epolling events");
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
    bool disconnected = false, subscribed = false;
    while (!disconnected)
    { // data processing worker
        read_request_from_client(received, client_sockfd);
        if(subscribed){
            switch(received->request){
                case c_disconnect:
                    end_resp_to_client(client_sockfd);
                    disconnected = true;
                    break;
                case c_leave_chat:
                    chats_subs_mutex.lock();
                    if (chats.count(received->message_text))
                    {
                        chats.at(received->message_text).erase(client_sockfd);
                        resp->responce = s_success;
                    }
                    else
                    {
                        resp->responce = s_failure;
                        resp->message_text = "Server error: could not leave chat";
                    }
                    chats_subs_mutex.unlock();
                    break;
                case c_send_message:
                    chat_mutex.lock();
                    int chatfd = open(connected_chat.c_str(), O_WRONLY);
                    chats_subs_mutex.lock();
                    if (chatfd > 0)
                    {
                        write(chatfd, user_name.c_str(), 32);
                        write(chatfd, received->message_text.c_str(), received->message_text.size());
                        if (close(chatfd) < 0)
                            perror("Could not close connected chat");
                        resp->responce = s_new_message;
                        for (auto subs_socket : chats.at(connected_chat))
                        {
                            if (client_sockfd != subs_socket)
                            {
                                resp->responce = s_new_message;
                                resp->message_text = user_name.c_str();
                                send_resp_to_client(resp, subs_socket);
                                resp->message_text = received->message_text;
                                send_resp_to_client(resp, subs_socket);
                            }
                        }
                        resp->responce = s_success;
                    }
                    else
                    {
                        resp->responce = s_failure;
                        resp->message_text = "Can't write this message: no chat is opened";
                    }
                    chat_mutex.unlock();
                    chats_subs_mutex.unlock();
                    break;
                default:
                    fprintf(stderr, "Wrong request from client : %d\n", received->request);
                    resp->responce = s_failure;
                    resp->message_text =  "WRONG REQUEST";
            }
        }
        else{
            switch(received->request){
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
                        chats[received->message_text] = {};
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
                    chats.at(received->message_text).insert(client_sockfd);
                    resp->responce = s_success;
                    chats_subs_mutex.unlock();
                    // sending it to client
                    struct stat st;
                    stat(received->message_text.c_str(), &st);
                    write(client_sockfd, &st.st_size, sizeof(off_t));
                    // and so on
                    resp->responce = s_success;
                    chat_mutex.lock();
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
            }
        send_resp_to_client(resp, client_sockfd);
    }
    free(received);
    free(resp);
    return;
}

void accept_connections(int listen_socket, int epollfd, std::vector<epoll_event> &events, std::mutex &mtx)
{ // I/O-Worker
    struct epoll_event ev;
    struct sockaddr_in client_address;
    socklen_t address_length = sizeof(sockaddr_in);
    int connection_socket;
    while ((connection_socket = accept4(listen_socket, (struct sockaddr *)&client_address, &address_length, SOCK_NONBLOCK)) != -1)
    {
        if (connection_socket == -1)
        {
            perror("New connection");
            return;
        }
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = connection_socket;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connection_socket, &ev) == -1)
        {
            perror("Setting in poll new connection");
            return;
        }
        mtx.lock();
        events.push_back(ev);
        mtx.unlock();
    }
    if (!(errno & (EAGAIN | EWOULDBLOCK)))
    {
        perror("Accept failure");
        return;
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

    struct epoll_event ev;
    int epollfd;

    epollfd = epoll_create1(0);
    if (epollfd == -1)
    {
        perror("Epoll create");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_socket;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_socket, &ev) == -1)
    {
        perror("Setting in poll listen socket");
        exit(EXIT_FAILURE);
    }

    std::vector<struct epoll_event> events(1, ev);

    int num = 0, timeout = -1;
    std::mutex mtx;
    for (mtx.lock(); (num = epoll_wait(epollfd, events.data(), events.size(), timeout)) != -1; mtx.lock())
    {
        mtx.unlock();
        timeout = -1;
        for (int i = 0; i < num; ++i)
        {
            if (events[i].data.fd == listen_socket)
            {
                std::async(
                    std::launch::async,
                    accept_connections,
                    listen_socket,
                    epollfd,
                    std::ref(events),
                    std::ref(mtx));
                timeout = 100;
            }
            else
            {
                int tmp = events[i].data.fd;
                mtx.lock();
                std::async(
                    std::launch::async,
                    routine,
                    tmp);
                mtx.unlock();
            }
        }
    }
    perror("Server died with status");
    server_ending(); // because why not
    exit(EXIT_SUCCESS);
}
