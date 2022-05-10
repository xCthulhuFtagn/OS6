#ifndef SERVINT
#define SERVINT

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "list.h"

#define MAX_MESS_LEN 256
#define MAX_CHAT_NAME_LEN 32

static list_of_chats* lc = NULL;

typedef enum {
    c_get_available_chats,
    c_create_chat,
    c_connect_chat,
    c_send_message,
    c_leave_chat,
    c_disconnect
} client_request_e;

typedef enum {
    s_success = 0,
    s_failure,
    s_resp_end,
    s_new_message
} server_responce_e;

typedef struct {
    client_request_e request;
    char message_text[MAX_MESS_LEN + 1];
} client_data_t;

typedef struct {
    server_responce_e responce;
    char message_text[MAX_MESS_LEN + 1];
} server_data_t;

int server_starting(list_of_chats* list);
void server_ending(list_of_chats* list);
int read_request_from_client(client_data_t* received, int sockfd);
//int start_resp_to_client(const client_data_t mess_to_send, int sockfd);
int send_resp_to_client(const server_data_t* resp, int sockfd);
void end_resp_to_client(int sockfd);

#endif