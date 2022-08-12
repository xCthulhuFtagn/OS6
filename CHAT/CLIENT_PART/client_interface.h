#ifndef CLIENT_INTERFACE_H
#define CLIENT_INTERFACE_H

#define MAX_MESS_LEN 128
#define MAX_CHAT_NAME_LEN 32
#define MAX_NAME_LEN 32

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

#endif // CLIENT_INTERFACE_H
