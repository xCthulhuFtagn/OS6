//#include "cd_data.h" // no need to include cause cliserv.h includes it now
#include "cliserv.h"
#include <errno.h> //extra

static int server_fd = -1;
static pid_t my_pid = 0;
static char client_pipe_name[PATH_MAX + 1] = {'\0'};
static int client_fd = -1;
static int client_write_fd = -1;

int server_starting(void){
    #if DEBUG_TRACE
         printf("%d :- server_starting()\n", getpid());
    #endif

    unlink(SERVER_PIPE);
    if(mkfifo(SERVER_PIPE, 0777) == -1){
        fprintf(stderr, "Server startup error, no FIFO created\n");
        return 0;
    }

    if((server_fd = open(SERVER_PIPE, O_RDONLY)) == -1){
        if(errno == EINTR) return 0;
        fprintf(stderr, "Server startup error, no FIFO opened\n");
        return 0;
    }
    return 1;
}

void server_ending(void){
    #if DEBUG_TRACE
        printf("%d :- server_ending()\n", getpid());
    #endif
    (void) close(server_fd);
    (void) unlink(SERVER_PIPE);
}

int read_request_from_client(message_db_t* rec_ptr){
    int return_code = 0;
    int read_bytes;
    #if DEBUG_TRACE
        printf("%D :- read_request_from_client()\n", getpid());
    #endif

    if(server_fd != -1){
        read_bytes = read(server_fd, rec_ptr, sizeof(*rec_ptr));
        if(read_bytes == 0){
            (void)close(server_fd);
            if((server_fd = open(SERVER_PIPE, O_RDONLY)) == -1){
                if(errno != EINTR){
                    fprintf(stderr, "Server error, FIFO open failed\n");
                }
                return 0;
            }
            read_bytes = read(server_fd, rec_ptr, sizeof(*rec_ptr));
        }
        if(read_bytes == sizeof(*rec_ptr)) return_code = 1;
    }
    return return_code;
}

int start_resp_to_client(const message_db_t mess_to_send){
    #if DEBUG_TRACE
        printf("%d :- start_resp_to_client()\n", getpid());
    #endif

    (void)sprintf(client_pipe_name, CLIENT_PIPE, mess_to_send.client_pid);
    if((client_fd = open(client_pipe_name, O_WRONLY)) == -1) return 0;
    return 1;
}

int send_resp_to_client(const message_db_t mess_to_send){
    int write_bytes;
    #if DEBUG_TRACE
        printf("%d : - send_resp_to_client()\n", getpid());
    #endif
    if(client_fd == -1) return 0;
    write_bytes = write(client_fd, &mess_to_send, sizeof(mess_to_send));
    if(write_bytes != sizeof(mess_to_send)) return 0;
    return 1;
}

void end_resp_to_client(void){
    #if DEBUG_TRACE
        printf("%d :- end_resp_to_client()\n", getpid());
    #endif

    if(client_fd != -1){
        (void)close(client_fd);
        client_fd = -1;
    }
}

int client_starting(void){
    #if DEBUG_TRACE
        printf("%d :- client_starting()\n", getpid());
    #endif

    my_pid = getpid();
    if((server_fd = open(SERVER_PIPE, O_WRONLY)) == -1){
        fprintf(stderr, "Server not running\n");
        return 0;
    }

    (void)sprintf(client_pipe_name, CLIENT_PIPE, my_pid);
    (void)unlink(client_pipe_name);
    if(mkfifo(client_pipe_name, 0777) == -1){
        fprintf(stderr, "Unable to create client pipe%s\n", client_pipe_name);
        return 0;
    }
    return 1;
}

void client_ending(void){
    #if DEBUG_TRACE
        printf("%d :- client_ending()\n");
    #endif
    
    if(client_write_fd != -1) (void)close(client_write_fd);
    if(client_fd != -1) (void)close(client_fd);
    if(server_fd != -1) (void)close(server_fd);
    (void)unlink(client_pipe_name);
}

int send_mess_to_server(message_db_t mess_to_send){
    int write_bytes;

    #if DEBUG_TRACE
        printf("%d :- send_mess_to_server()\n", getpid());
    #endif
    if(server_fd == -1) return 0;
    mess_to_send.client_pid = my_pid;
    write_bytes = write(server_fd, &mess_to_send, sizeof(mess_to_send));
    if(write_bytes != sizeof(mess_to_send)) return 0;
    return 1;
}

int start_resp_from_server(void){
    #if DEBUG_TRACE
        printf("%d :- start_resp_from_server()\n", getpid());
    #endif

    if(client_pipe_name[0] == '\0') return 0;
    if(client_fd != -1) return 1;

    client_fd = open(client_pipe_name, O_RDONLY);
    if(client_fd != -1){
        client_write_fd = open(client_pipe_name, O_WRONLY);
        if(client_write_fd != -1) return 1;
        (void)close(client_fd);
        client_fd = -1;
    }
    return 0;
}

int read_resp_from_server(message_db_t *rec_ptr){
    int read_bytes;
    int return_code = 0;

    #if DEBUG_TRACE
        printf("%d :- read_resp_from_server()\n", getpid());
    #endif

    if(!rec_ptr) return 0;
    if(client_fd == -1) return 0;
    read_bytes = read(client_fd, rec_ptr, sizeof(*rec_ptr));
    if(read_bytes == sizeof(*rec_ptr)) return_code = 1;
    return return_code;
}

void end_resp_from_server(void){
    #if DEBUG_TRACE
        printf("%d :- end_resp_from_server()\n", getpid());
    #endif

    // в реализации канала эта функция пустая
}