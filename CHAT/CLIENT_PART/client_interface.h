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
#define MAX_CHAT_NAME_LEN 32

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

int client_starting(void);
void client_ending(void);
int send_mess_to_server(client_data_t* rec_ptr);
int start_resp_from_server(void);
int read_resp_from_server(server_data_t * rec_ptr);
void end_resp_from_server(void);
void newMessage(char*);