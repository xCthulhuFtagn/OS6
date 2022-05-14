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

pthread_cond_t condFinished, condStarted;
pthread_mutex_t chat_mutex;
client_data_t* message;
server_data_t* resp;
//char chat[MAX_CHAT_NAME_LEN + 1];

void* read_routine(void* input){
    while(read(sockfd, resp, sizeof(server_data_t)) > 0){
        switch(message->request){
        case c_connect_chat:
        {
            if(read_resp_from_server(resp) < 0){
                perror("Failed to connect chat");
                break;
            }
            pthread_mutex_lock(&chat_mutex);
            //strcpy(chat, message->message_text);
            if((chatfd = open("chat", O_WRONLY | O_TRUNC | O_CREAT | O_APPEND)) < 0){
                pthread_mutex_unlock(&chat_mutex);
                perror("Could not open chat to write");
                break;
            }
            while(resp->responce == s_success || resp->responce == s_new_message){
                if(resp->responce == s_new_message){
                    newMessage(resp->message_text);
                    continue;
                }
                write(chatfd, resp->message_text, strnlen(resp->message_text, 257));
                read_resp_from_server(resp);
            };
            if(resp->responce == s_failure) fprintf(stderr, "%s", resp->message_text);
            if(close(chatfd) < 0) perror("Could not close chat file");
            pthread_mutex_unlock(&chat_mutex);
            break;
        }
        case c_create_chat:
            break;
        case c_get_available_chats:
            break;
        case c_send_message:
            break;
        case c_disconnect:
            break;
        case c_leave_chat:
            break;
        }
    }
    perror("The connection to the host has been ended");
}
void* write_routine(void* input){
    /*switch(message->request){
    case c_connect_chat:
        send_mess_to_server(message);
        pthread_cond_wait(&condFinished, &chat_mutex);
        break;
    case c_create_chat:
        send_mess_to_server(message);
        pthread_cond_wait(&condFinished, &chat_mutex);
        break;
    case c_disconnect:
        send_mess_to_server(message);
        pthread_cond_wait(&condFinished, &chat_mutex);
        break;
    case c_get_available_chats:
        send_mess_to_server(message);
        break;
    case c_leave_chat:
        send_mess_to_server(message);
        break;
    case c_send_message:
        send_mess_to_server(message);
        break;
    }*/
    send_mess_to_server(message);
    pthread_cond_wait(&condFinished, &chat_mutex);
}

int main(int argc, char *argv[])
{
    char client_name[32];
    if(!client_starting()) exit(EXIT_FAILURE);
    struct sockaddr_in address;
    pthread_t *client_threads;
    fd_set readfds;
    char host[256];
    struct hostent *hostinfo;
    if(pthread_cond_init(&condFinished, NULL) < 0 || (pthread_cond_init(&condStarted, NULL) < 0)){
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

    pthread_cond_destroy(&condStarted);
    pthread_cond_destroy(&condFinished);
    free(resp);
    free(sent);
}
