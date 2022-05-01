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

#include "server_interface.h"

static int server_running = 1;
static void process_command(const client_data_t mess_command);

void catch_signals(){
    server_running = 0;
}

void routine(void* input){
    int sockfd = *((int*)input), chatfd_write = -1;
    free(input);
    client_data_t *received = malloc(sizeof(client_data_t));
    server_data_t *resp = malloc(sizeof(server_data_t));
    while(read(sockfd, received, sizeof(received)) > 0){
        switch(received->request){
            case c_disconnect:
                if(chatfd_write > 0) {
                    if(close(chatfd_write) < 0) perror("Chatfd_write could not be closed");
                }
                end_resp_to_client(sockfd);
            case c_get_available_chats:
            {
                DIR *dir_ptr;
                if(!(dir_ptr = opendir("."))){//not NULL
                    fprintf(stderr, "Could not read chats: opendir() failed\n");
                    resp->responce = s_failure;
                    strcpy(resp->responce, "Chats are unavailable");
                    send_resp_to_client(resp, sockfd);
                    break;
                }
                struct dirent *chat;
                int err = 0;
                resp->responce = s_success;
                while(chat = readdir(dir_ptr)){
                    strcpy(resp->message_text, chat->d_name, 32);
                    send_resp_to_client(resp, sockfd);
                }
                closedir(dir_ptr);
                resp->responce = s_resp_end;
                send_resp_to_client(resp, sockfd);
                break;
            }
            case c_create_chat:
                if(strlen(received->message_text) > 32){
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Chat's name is too big");
                    send_resp_to_client(resp, sockfd);
                    break;
                }
                creat(received->message_text, S_IRWXG | S_IRWXO | S_IRWXU);
                /*
                //don't know how to check the error
                if(EEXIST) {
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Chat with such name already exists");
                } else */resp->responce = s_success;

                send_resp_to_client(resp, sockfd);
                break;
            case c_connect_chat:
            {
                DIR *dir_ptr;
                if(!(dir_ptr = opendir("."))){//not NULL
                    perror("Opendir() failed");
                    resp->responce = s_failure;
                    strcpy(resp->responce, "Chats are unavailable");
                    send_resp_to_client(resp, sockfd);
                    break;
                }
                struct dirent *chat;
                int found = 0;
                while(chat = readdir(dir_ptr)){
                    if(strncmp(chat->d_name, received->message_text, 32)){
                        found = 1;
                        break;
                    }
                }
                if(closedir(dir_ptr) == 0){
                    if(found){
                        int chatfd;
                        if((chatfd = open(received->message_text, O_RDONLY) > 0)){
                            if(chatfd_write = open(received->message_text, O_WRONLY) < 0){
                                close(chatfd);
                                resp->responce = s_failure;
                                strcpy(resp->responce, "Server error: cannot open chat");
                                send_resp_to_client(resp, sockfd);
                                break; //
                            }
                            int status;
                            resp->responce = s_success;
                            while((status = read(chatfd, resp->message_text, MAX_MESS_LEN + 1)) > 0){
                                if(status < 0){
                                    resp->responce = s_failure;
                                    strcpy(resp->message_text, "Server error: could not read the whole chat");
                                    break;
                                }
                                int extra = 256 - strlen(received->message_text);
                                if(extra < 256) lseek(chatfd, -extra, SEEK_CUR);
                                send_resp_to_client(resp, sockfd);
                            }
                            if(status == 0) resp->responce = s_resp_end;
                            send_resp_to_client(resp, sockfd);
                        }else{
                            resp->responce = s_failure;
                            strcpy(resp->responce, "Server error: cannot open chat");
                            send_resp_to_client(resp, sockfd);
                        }
                    }else{
                        resp->responce = s_failure;
                        strcpy(resp->message_text, "Such chat does not exist");
                        send_resp_to_client(resp, sockfd);
                    }
                }else{
                    perror("Could not close directory");
                }
                break;
            }
            case c_leave_chat:
                if(chatfd_write > 0){
                    close(chatfd_write);
                    resp->responce = s_success;
                }else{
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Cannot exit non-opened chat");
                }
                send_resp_to_client(resp, sockfd);
                break;
            case c_send_message:
            //message division through \0
                if(chatfd_write > 0){
                    write(chatfd_write, received->message_text, strlen(received->message_text));
                    resp->responce = s_success;
                }else{
                    resp->responce = s_failure;
                    strcpy(resp->message_text, "Can't write this message: no chat is opened");
                }
                send_resp_to_client(resp, sockfd);
                break;
            default:
                fprintf(perror, "Wrong request from client : %d\n", received->request);
                resp->responce = s_failure;
                strcpy(resp->message_text, "WRONG REQUEST");
                send_resp_to_client(resp, sockfd);
        }
    }
    free(received);
    free(resp);
    return;
}

int main(int argc, char* argv[]){
    if(!server_starting()) exit(EXIT_FAILURE);
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
    server_ending();
    exit(EXIT_SUCCESS);
}
