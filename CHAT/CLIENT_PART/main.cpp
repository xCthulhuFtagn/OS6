#include <string.h>
#include <malloc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <dirent.h>
#include <pthread.h>

#include "client_interface.h"

int chatfd;
int sockfd;
int finished = 1;

pthread_cond_t condFinished;
pthread_mutex_t chat_mutex;

void* read_routine(void* input){
    server_data_t* resp;
    while(read(sockfd, resp, sizeof(server_data_t)) > 0){
        
    }
}
void* write_routine(void* input){
    client_data_t* message;
    switch(message->request){
    case c_connect_chat:
        send_mess_to_server(message);
        pthread_cond_wait(&condFinished, &chat_mutex);
        pthread_mutex_lock(&chat_mutex);
        chatfd = open("chat", O_WRONLY, O_CREAT | O_TRUNC);
        
        pthread_mutex_unlock(&chat_mutex);
    case c_create_chat:

    case c_disconnect:

    case c_get_available_chats:

    case c_leave_chat:

    case c_send_message:

    }
}
void* graphic_part(void* input){}

int main(int argc, char *argv[])
{
    char client_name[32];
    if(!client_starting()) exit(EXIT_FAILURE);
    struct sockaddr_in address;
    pthread_t *client_threads;
    fd_set readfds;
    char host[256];
    struct hostent *hostinfo;
    if(pthread_cond_init(&condFinished, NULL) < 0){
        perror("Could not initialize condition variable");
        exit(EXIT_FAILURE);
    }
    gethostname(host, 255);
    if(!(hostinfo = gethostbyname(host))){
        fprintf(stderr, "cannot get info for server host: %s\n", host);
        exit(EXIT_FAILURE);
    }
    client_data_t* sent = (client_data_t*)malloc(sizeof(client_data_t));
    server_data_t* resp = (server_data_t*)malloc(sizeof(server_data_t));
    struct sockaddr_in address;
    pthread_t *client_threads;
    //fd_set readfds;

    char host[256];
    struct hostent *hostinfo;
    gethostname(host, 255);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = hostinfo->h_addrtype; // input of machine's adress

    bind(sockfd, (struct sockaddr*) &address, sizeof(address));

    //put first data and connection here

    pthread_t threads[3]; // 0 - read, 1 - write, 2 - graphics

    pthread_create(threads, NULL, read_routine, NULL);
    pthread_create(threads + 1, NULL, write_routine, NULL);
    pthread_create(threads + 2, NULL, graphic_part, NULL);

    pthread_cond_destroy(&condFinished);
    free(resp);
    free(sent);
}
