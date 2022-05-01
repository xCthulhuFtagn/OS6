#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cd_data.h> //extra

#define SERVER_PIPE "/tmp/server_pipe"
#define CLIENT_PIPE "/tmp/client_%d_pipe"

#define MAX_MESS_LEN 256
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
    s_resp_end
} server_responce_e;

typedef struct {
    client_request_e request;
    char message_text[MAX_MESS_LEN + 1];
} client_data_t;

typedef struct {
    server_responce_e responce;
    char message_text[MAX_MESS_LEN + 1];
} server_data_t;

int server_starting(void);
void server_ending(void);
int read_request_from_client(client_data_t* rec_ptr, int sockfd);
//int start_resp_to_client(const client_data_t mess_to_send, int sockfd);
int send_resp_to_client(const server_data_t* mess_to_send, int sockfd);
void end_resp_to_client(int sockfd);