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

#define ERR_TEXT_LEN 80


typedef enum {
    s_create_new_database = 0,
    s_get_cdc_entry,
    s_get_cdt_entry,
    s_add_cdc_entry,
    s_add_cdt_entry,
    s_del_cdc_entry,
    s_del_cdt_entry,
    s_find_cdc_entry
} client_request_e;

typedef enum {
    r_success = 0,
    r_failure,
    r_find_no_more
} server_responce_e;

typedef struct {
    pid_t client_pid;
    client_request_e request;
    server_responce_e responce;
    cdc_entry cdc_entry_data;
    cdt_entry cdt_entry_data;
    char error_text[ERR_TEXT_LEN + 1];
} message_db_t;

int server_starting(void);
void server_ending(void);
int read_request_from_client(message_db_t* rec_ptr);
int start_resp_to_client(const message_db_t mess_to_send);
int send_resp_to_client(const message_db_t mess_to_send);
void end_resp_to_client(void);

int server_starting(void);
void server_ending(void);
int read_request_from_client(message_db_t* rec_ptr);
int start_resp_from_server(void);
int read_resp_from_server(message_db_t * rec_ptr);
void end_resp_from_server(void);