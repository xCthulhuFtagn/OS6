#include <string.h>
#include <malloc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <dirent.h>
#include <pthread.h>
#include <queue>

#include "client_interface.h"

int chatfd;
int sockfd;

pthread_cond_t condFinished, condStarted;
pthread_mutex_t chat_mutex, read_mutex;
std::queue<server_data_t> responces;
std::queue<client_data_t> requests;
char **available_chats;
short cnt = 0, num_reallocs = 0;

void* read_routine(void* input){
    server_data_t *resp;
    while(recv(sockfd, resp, sizeof(server_data_t)) > 0){
        responces.push(resp);
        /*
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
            if(close(chatfd) < 0) perror("Could not close chat file");
            pthread_mutex_unlock(&chat_mutex);
            break;
        }
        case c_create_chat:
            if(resp->responce == s_failure) fprintf(stderr, "%s", resp->message_text);
            break;
        case c_get_available_chats:
            while(resp->responce = s_success || resp->responce = s_new_message){
                if(resp->responce == s_new_message){
                    newMessage(resp->message_text);
                    continue;
                }
                cnt = (cnt + 1) % 3;
                if(cnt == 0) {
                    available_chats = realloc(available_chats, sizeof(available_chats) + 3 * sizeof(char*));
                    ++num_reallocs;
                    for (auto i = 0; i < 3; ++i) available_chats[num_reallocs * 3 + i] = malloc(MAX_CHAT_NAME_LEN);
                }
                strcpy(available_chats[num_reallocs * 3 + cnt], resp->message_text);
            }
            break;
        // case c_send_message:
        //     break;
        // case c_disconnect:
        //     break;
        case c_leave_chat:
            break;
        }
        if(resp->responce == s_failure) fprintf(stderr, "%s", resp->message_text);
        */

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
    while(true){
        if(!requests.empty()){
            send_mess_to_server(requests.pop());
        }
    }
}

int main(int argc, char *argv[])
{
    char client_name[32];
    available_chats = malloc(3 * (char *));
    for (auto i = 0; i < 3; ++i) available_chats[i] = malloc(MAX_CHAT_NAME_LEN);
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
