#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <pthread.h>
#include <netdb.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef STRING
#define STRING
#include <string.h>
#endif //STRING

#include "server_interface.h"

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

void routine(void* input){
    int client_sockfd = ((struct init*)input)->desc;
    char client_name[32];
    strcpy(client_name,((struct init*)input)->name);
    free(input);
    client_data_t *received = malloc(sizeof(client_data_t));
    server_data_t *resp = malloc(sizeof(server_data_t));
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
                fprintf(perror, "Wrong request from client : %d\n", received->request);
                resp->responce = s_failure;
                strcpy(resp->message_text, "WRONG REQUEST");
        }
        if(send_resp_to_client(resp, client_sockfd) < 0) perror("Responce failed");
    }
    free(received);
    free(resp);
    return;
}

int main(int argc, char* argv[]){
    if(!server_starting(lc)) exit(EXIT_FAILURE);
    int listen_sockfd;
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
    listen_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //adress.sin_port = htons(5000); //хз как там настраивается порт
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_ntoa(hostinfo->h_addrtype); // input of machine's adress
    /* The call to the function "bind()" assigns the details specified
	 * in the structure address to the socket created in the step above
	 */
    bind(listen_sockfd, (struct sockaddr_in*) &address, sizeof(address));
    client_threads = (pthread_t*)malloc(sizeof(pthread_t)*1024);
    int times = 1;
    listen(listen_sockfd, 3);
    //FD_ZERO(&readfds);
    //FD_SET(listen_sockfd, &readfds);
    while(server_running){
        struct sockaddr_in* client_addr;
        client_addr->sin_family = AF_INET;
        if(bind(listen_sockfd, (struct sockaddr*) &client_addr, len(client_addr)) == -1) perror("Bind");
        struct init *client = (int*) malloc(sizeof(int));
        for(int i = 0; i < 3; ++i){
            /* In the call to accept(), the server is put to sleep and when for an incoming
            * client request, the three way TCP handshake* is complete, the function accept()
            * wakes up and returns the socket descriptor representing the client socket.
            */
            client->desc = accept(listen_sockfd, client_addr, sizeof(client_addr));
            if(read(client->desc,client->name, 32) < 0){
                perror("Could not read client's name from socket");
                server_data_t resp = {s_failure, "Failed to save user's name"};
                send_resp_to_client(&resp, client->desc);
            }
            pthread_create(client_threads + i + 3*(times-1), NULL, routine, (void*)client);
        }
        client_threads = (pthread_t*)realloc((void*)client_threads, 3*times);
        ++times;
    }
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
    server_ending(lc);
    exit(EXIT_SUCCESS);
}
