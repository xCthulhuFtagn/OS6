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

#define ERR_TEXT_LEN 32
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
    s_failure
} server_responce_e;

typedef struct {
    client_request_e request;
    //server_responce_e responce;
    int chat_num;
    char message_text[MAX_MESS_LEN + 1];
    char error_text[ERR_TEXT_LEN + 1];
} client_data_t;

int server_starting(void);
void server_ending(void);
int read_request_from_client(client_data_t* rec_ptr);
int start_resp_to_client(const client_data_t mess_to_send);
int send_resp_to_client(const client_data_t mess_to_send);
void end_resp_to_client(void);