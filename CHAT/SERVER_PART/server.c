#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <pthread.h>
//#include <arpa/inet.h>
#include <netdb.h>

#include "server_interface.h"

static int server_running = 1;
static void process_command(const client_data_t mess_command);

void catch_signals(){
    server_running = 0;
}

void routine(void* input){
    int socket_desc = *((int*)input);
    free(input);
    client_data_t received;
    while(read(socket_desc, &received, sizeof(received)) > 0){
        switch(received.request){
            case c_disconnect:
                break;
            case c_get_available_chats:
                break;
            case c_connect_chat:
                break;
            case c_send_message:
                break;
            default:
                fprintf(perror, "Wrong request from client : %d\n", received.request);
        }
    }
}

int main(int argc, char* argv[]){
    if(!server_starting()) exit(EXIT_FAILURE);
    client_data_t mess_command;
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
