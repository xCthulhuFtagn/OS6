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
#ifndef STRING
#define STRING
#include <string.h>
#endif //STRING

#include "server_interface.h"

static int server_running = 1;
static void process_command(const client_data_t mess_command);

void catch_signals(){
    server_running = 0;
}

/* structure of chat_sub file:
    ChatName1 subsocket1.1 subsocket1.2 ... subsocket1.n \n
    ...
    ChatNamek subsocketk.1 subsocketk.2 ... subsocketk.n \n
*/

void routine(void* input){
    int sockfd = *((int*)input);
    free(input);
    client_data_t *received = malloc(sizeof(client_data_t));
    server_data_t *resp = malloc(sizeof(server_data_t));
    char connected_chat[MAX_CHAT_NAME_LEN + 1];
    while(read_request_from_client(received, sockfd)){
        switch(received->request){
            case c_disconnect:
                end_resp_to_client(sockfd);
            case c_get_available_chats:
            {
                resp->responce = s_success;
                for(list_of_chats* chat = lc; chat != NULL; chat = chat->next){
                    strncpy(resp->message_text, chat->name_of_chat, MAX_CHAT_NAME_LEN);
                    if(send_resp_to_client(resp, sockfd) < 0) perror("Responce failed");
                }
                resp->responce = s_resp_end;
                break;
            }
            case c_create_chat:
            {
                if(strlen(received->message_text) > MAX_CHAT_NAME_LEN){
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Chat's name is too big");
                    break;
                }
                creat(received->message_text, S_IRWXG | S_IRWXO | S_IRWXU);
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
                break;
            }
            case c_connect_chat:
            {
                list_of_chats* needed;
                if(!(needed = findChat(lc, received->message_text))){
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Server error: cannot create chat");
                } 
                else{
                    //mb check if chat is already in the list in der Zukunft, heh
                    addSub(needed, sockfd);
                    strncmp(connected_chat, received->message_text, MAX_CHAT_NAME_LEN);
                    resp->responce = s_success;
                }
            }
            case c_leave_chat:
            {
                list_of_chats* needed = findChat(lc, received->message_text);
                if (needed != NULL) {
                    delSub(needed->subs, sockfd);
                    connected_chat[0] = '\0';
                }
                else{
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Server error: could not leave chat");
                }
            }
            case c_send_message:
            {
                int chatfd = open(connected_chat, O_WRONLY);
                if(chatfd > 0){
                    write(chatfd, received->message_text, strlen(received->message_text));
                    resp->responce = s_success;
                }else{
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Can't write this message: no chat is opened");
                }
                if(close(chatfd) < 0) perror("Could not close connected chat");
                break;
            }
            default:
                fprintf(perror, "Wrong request from client : %d\n", received->request);
                resp->responce = s_failure;
                strcpy(resp->message_text, "WRONG REQUEST");
        }
        if(send_resp_to_client(resp, sockfd) < 0) perror("Responce failed");
    }
    free(received);
    free(resp);
    return;
}

int main(int argc, char* argv[]){
    if(!server_starting(lc)) exit(EXIT_FAILURE);
    int sockfd;
    struct sockaddr_in address;
    pthread_t *client_threads;
    fd_set readfds;

    char host[256];
    struct hostent *hostinfo;
    gethostname(host, 255);
    if(!(hostinfo = gethostbyname(host))){
        fprintf(stderr, "cannot get info for server host: %s\n", host);
        exit(EXIT_FAILURE);
    }

    sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_ntoa(hostinfo->h_addrtype); // wtf
    bind(sockfd, (struct sockaddr_in*) &address, sizeof(address));
    client_threads = (pthread_t*)malloc(sizeof(pthread_t)*1024);
    int times = 1;
    listen(sockfd, 3);
    //FD_ZERO(&readfds);
    //FD_SET(sockfd, &readfds);
    while(server_running){
        struct sockaddr_in* client_addr;
        client_addr->sin_family = AF_INET;
        if(bind(sockfd, (struct sockaddr*) &client_addr, len(client_addr)) == -1) perror("Bind");
        int *new_client_desc = (int*) malloc(sizeof(int));
        for(int i = 0; i < 3; ++i){
            *new_client_desc = accept(sockfd, client_addr, sizeof(client_addr));
            pthread_create(client_threads + i + 1024*(times-1), NULL, routine, (void*)new_client_desc);
        }
        client_threads = (pthread_t*)realloc((void*)client_threads, 3*times);
        ++times;
    }
    server_ending(lc);
    exit(EXIT_SUCCESS);
}
