#pragma once

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h> /* "Главный" по прослушке */
#include <sys/types.h>

#include <netdb.h>    //for gethostname
#include <dirent.h>   //work with file system
#include <errno.h>    //error codes and so on
#include <sys/stat.h> // additional file functions & info
#include <string.h> //working with old style strings

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <mutex>
#include <ctime>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <list>

typedef enum {
    c_set_name = 0,
    c_create_chat,
    c_connect_chat,
    c_send_message,
    c_leave_chat,
    c_disconnect
} client_request_e;

typedef enum {
    s_success = 0,
    s_failure,
    s_new_message
} server_responce_e;

typedef struct {
    client_request_e request;
    std::string message_text;
} client_data_t;

typedef struct {
    server_responce_e responce;
    std::string message_text;
} server_data_t;

typedef struct{
    int in;
    int out;
} chat_pipe;

int server_starting();
void server_ending();
int read_request_from_client(client_data_t* received, int sockfd);
void send_resp_to_client(const server_data_t* resp, int sockfd);
void send_available_chats(int sockfd);
void end_resp_to_client(int sockfd);