//#include "cd_data.h" // no need to include cause cliserv.h includes it now
#include "server_interface.h"
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

int read_request_from_client(message_t* rec_ptr){
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

int start_resp_to_client(const message_t mess_to_send){
    #if DEBUG_TRACE
        printf("%d :- start_resp_to_client()\n", getpid());
    #endif

    (void)sprintf(client_pipe_name, CLIENT_PIPE, mess_to_send.client_pid);
    if((client_fd = open(client_pipe_name, O_WRONLY)) == -1) return 0;
    return 1;
}

int send_resp_to_client(const message_t mess_to_send){
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
