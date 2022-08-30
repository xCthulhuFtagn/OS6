#ifndef CLIENT_INTERFACE_H
#define CLIENT_INTERFACE_H
#include <string>
#include <QTcpSocket>
#include <QLayout>
#include <QWidget>
#define MAX_CHAT_NAME_LEN 32
#define MAX_NAME_LEN 32

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

bool SendToServer(QTcpSocket*, client_data_t*);
bool ReadFromServer(QTcpSocket*, server_data_t*);

void SafeCleaning(QLayout*);

#endif // CLIENT_INTERFACE_H
