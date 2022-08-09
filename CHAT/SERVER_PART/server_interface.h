#pragma once

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unordered_set>
#include <unordered_map>
#include <string>

#define MAX_MESS_LEN 256
#define MAX_CHAT_NAME_LEN 32
#define MAX_NAME_LEN 32

std::unordered_map<std::string, std::unordered_set<int>> chats;

typedef enum {
    c_get_available_chats = 0,
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

int server_starting();
void server_ending();
int read_request_from_client(client_data_t* received, int sockfd);
int send_resp_to_client(const server_data_t* resp, int sockfd);
void end_resp_to_client(int sockfd);
