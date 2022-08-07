#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>  /* "Главный" по прослушке */
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <pthread.h>
#include <netdb.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <semaphore.h>
#ifndef STRING
#define STRING
#include <string.h>
#endif //STRING

#include "server_interface.h"

#include <vector>
#include <thread>
#include <future>
#include <tuple>
#include <chrono>
#include <atomic>
#include <unordered_set>

static int server_running = 1;
pthread_mutex_t create_mutex;
pthread_mutex_t chat_mutex;
pthread_mutex_t upd_list_mutex;

void catch_signals(){
    server_running = 0;
}

struct init{
    int desc;
    char name[32];
};

void routine(int client_sockfd){
    client_data_t *received = new client_data_t;
    server_data_t *resp = new server_data_t;
    char connected_chat[MAX_CHAT_NAME_LEN + 1];
    while(read_request_from_client(received, client_sockfd)){
        switch(received->request){
            case c_disconnect:
                end_resp_to_client(client_sockfd);
            case c_get_available_chats:
            {
                resp->responce = s_success;
                for(list_of_chats* chat = lc; chat != NULL; chat = chat->next){
                    strncpy(resp->message_text, chat->name_of_chat, MAX_CHAT_NAME_LEN);
                    if(send_resp_to_client(resp, client_sockfd) < 0) perror("Responce failed");
                }
                resp->responce = s_resp_end;
                break;
            }
            case c_create_chat:
            {
                pthread_mutex_lock(&create_mutex);
                if(strlen(received->message_text) > MAX_CHAT_NAME_LEN){
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Chat's name is too big");
                    break;
                }
                creat(received->message_text, S_IRWXG | S_IRWXO | S_IRWXU);
                pthread_mutex_lock(&upd_list_mutex);
                if(errno == EEXIST) {
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Chat with such name already exists");
                } 
                else if(!AddChat(lc, received->message_text)){
                    perror("Could not add chat to list of chats");
                    if(remove(received->message_text) < 0){
                        perror("Could not remove the chat that could not be added to list");
                    }
                    strcpy(resp->message_text, "Server error: cannot create chat");
                }
                else resp->responce = s_success;
                pthread_mutex_unlock(&create_mutex);
                pthread_mutex_unlock(&upd_list_mutex);
                break;
            }
            case c_connect_chat:
            {
                pthread_mutex_lock(&upd_list_mutex);
                list_of_chats* needed;
                if(!(needed = findChat(lc, received->message_text))){
                    lc = addChat(lc, needed);
                } 
                addSub(needed, client_sockfd);
                strncmp(connected_chat, received->message_text, MAX_CHAT_NAME_LEN);
                resp->responce = s_success;
                pthread_mutex_unlock(&upd_list_mutex);
                //sending it to client
                struct stat st;
                stat(received->message_text, &st);
                write(client_sockfd, &st.st_size, sizeof(off_t));
                //and so on
                resp->responce = s_success;
                pthread_mutex_lock(&chat_mutex);
                int chatfd;
                if((chatfd = open(received->message_text, O_RDONLY)) > 0){
                    for(size_t i = 0; i < st.st_size; i += 256){
                        if(read(chatfd, resp->message_text, 256) < 0){
                            perror("Failed to read from chat");
                            resp->responce = s_failure;
                            strcpy(resp->message_text, "Server error: could not read chat");
                            break;
                        }
                        if(send_resp_to_client(resp, client_sockfd) < 0){
                            perror("Failed to transmit whole chat");
                            resp->responce = s_failure;
                            strcpy(resp->message_text, "Server error: failed to transmit whole chat");
                            break;
                        }
                        resp->responce = s_resp_end;
                    }
                }
                else {
                    perror("Could not open chat on demand");
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Server error: could not open requested chat");
                }
                pthread_mutex_unlock(&chat_mutex);
            }
            case c_leave_chat:
            {
                pthread_mutex_lock(&upd_list_mutex);
                list_of_chats* needed = findChat(lc, received->message_text);
                if (needed != NULL) {
                    delSub(needed->subs, client_sockfd);
                    connected_chat[0] = '\0';
                    resp->responce = s_success;
                }
                else{
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Server error: could not leave chat");
                }
                pthread_mutex_unlock(&upd_list_mutex);
            }
            case c_send_message:
            {
                pthread_mutex_lock(&chat_mutex);
                int chatfd = open(connected_chat, O_WRONLY);
                pthread_mutex_lock(&upd_list_mutex);
                if(chatfd > 0){
                    write(chatfd, client_name, 32);
                    write(chatfd, received->message_text, MAX_MESS_LEN + 1);
                    if(close(chatfd) < 0) perror("Could not close connected chat");
                    list_of_chats* tmp;
                    if((tmp = findChat(lc, connected_chat)) != NULL){
                        for(list_of_subscribers* i = tmp->subs; i != NULL; i= i->next){
                            if(client_sockfd != i->socket){
                                resp->responce = s_new_message;
                                strcpy(resp->message_text, client_name);
                                send_resp_to_client(resp, i->socket);
                                strcpy(resp->message_text, received->message_text);
                                send_resp_to_client(resp, i->socket);
                            }
                        } 
                    }
                    resp->responce = s_success;
                }else{
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Can't write this message: no chat is opened");
                }
                pthread_mutex_unlock(&chat_mutex);
                pthread_mutex_unlock(&upd_list_mutex);
                break;
            }
            default:
                fprintf(stderr, "Wrong request from client : %d\n", received->request);
                resp->responce = s_failure;
                strcpy(resp->message_text, "WRONG REQUEST");
        }
        if(send_resp_to_client(resp, client_sockfd) < 0) perror("Responce failed");
    }
    free(received);
    free(resp);
    return;
}

void accept_connections(int listen_socket, int epollfd, std::vector<epoll_event>& events, std::mutex& mtx) {
    struct epoll_event ev;
    struct sockaddr_in client_address;
    socklen_t address_length = sizeof(sockaddr_in);
    int connection_socket;
    while ((connection_socket = accept4(listen_socket, (struct sockaddr*) &client_address, &address_length, SOCK_NONBLOCK)) != -1) {
        if (connection_socket == -1) {
            perror("New connection");
            return;
        }
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = connection_socket;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connection_socket, &ev) == -1) {
            perror("Setting in poll new connection");
            return;
        }
        mtx.lock();
        events.push_back(ev);
        mtx.unlock();
    }
    if (!(errno & (EAGAIN | EWOULDBLOCK))) {
        perror("Accept failure");
        return;
    }
}

int main(int argc, char* argv[]){
    using namespace std::chrono_literals;
    struct sockaddr_in server_address;
    if(!server_starting(lc)) exit(EXIT_FAILURE);
    int listen_socket;
    if(pthread_mutex_init(&chat_mutex, NULL) != 0){
        perror("Could not initialize chat_mutex");
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_init(&create_mutex, NULL) != 0){
        perror("Could not initialize create_mutex");
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_init(&upd_list_mutex, NULL) != 0){
        perror("Could not initialize create_mutex");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in address;
    pthread_t *client_threads;
    //fd_set readfds;

    char host[256];
    struct hostent *hostinfo;
    gethostname(host, 255);
    if(!(hostinfo = gethostbyname(host))){
        fprintf(stderr, "cannot get info for server host: %s\n", host);
        pthread_mutex_destroy(&chat_mutex);
        pthread_mutex_destroy(&create_mutex);
        pthread_mutex_destroy(&upd_list_mutex);
        exit(EXIT_FAILURE);
    }
    /* creates an UN-named socket inside the kernel and returns
	 * an integer known as socket descriptor
	 * This function takes domain/family as its first argument.
	 * For Internet family of IPv4 addresses we use AF_INET
	 */
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    address.sin_port = htons(5000); // TO DO: CHANGE TO CONSTANT
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = in_addr_t { (uint)hostinfo->h_addrtype }; // input of machine's adress (CHANGE TO CONSTANT)
    /* The call to the function "bind()" assigns the details specified
	 * in the structure address to the socket created in the step above
	 */
    if (bind(listen_socket, (struct sockaddr*)& server_address, sizeof(server_address)) == -1) {
        perror("Bind error");
        close(listen_socket);
        return EXIT_FAILURE;
    }

    if (listen(listen_socket, 1000) == -1) {
        perror("Listen error");
        shutdown(listen_socket, SHUT_RDWR);
        return EXIT_FAILURE;
    }

    struct epoll_event ev;
    std::vector<struct epoll_event> events(std::vector<epoll_event>(1));
    int epollfd;

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("Epoll create");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_socket;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_socket, &ev) == -1) {
        perror("Setting in poll listen socket");
        exit(EXIT_FAILURE);
    }

    int num = 0, timeout = -1;
    std::mutex mtx;
    for (mtx.lock(); (num = epoll_wait(epollfd, events.data(), events.size(), timeout)) != -1; mtx.lock()) {
        mtx.unlock();
        timeout = -1;
        for (int i = 0; i < num; ++i) {
            if (events[i].data.fd == listen_socket) {
                std::async(
                    std::launch::async,
                    accept_connections,
                    listen_socket,
                    epollfd,
                    std::ref(events),
                    std::ref(mtx)
                );
                timeout = 100;
            } else {
                mtx.lock();
                std::async(
                    std::launch::async,
                    routine,
                    events[i].data.fd
                );
                mtx.unlock();
            }
        }
    }
    perror("Server died and sayed");
    if(pthread_mutex_destroy(&chat_mutex) != 0){
        perror("Could not destroy chat_mutex");
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_destroy(&create_mutex) != 0){
        perror("Could not destroy create_mutex");
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_destroy(&upd_list_mutex) != 0){
        perror("Could not destroy create_mutex");
        exit(EXIT_FAILURE);
    }
    // server_ending(lc);
    exit(EXIT_SUCCESS);
}
