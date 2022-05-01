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

}

void server_ending(void){
    #if DEBUG_TRACE
        printf("%d :- server_ending()\n", getpid());
    #endif
    
}

int read_request_from_client(client_data_t* rec_ptr, int sockfd){
    int return_code = 0;
    int read_bytes;
    #if DEBUG_TRACE
        printf("%D :- read_request_from_client()\n", getpid());
    #endif
    
}

int send_resp_to_client(const server_data_t* mess_to_send, int sockfd){
    int write_bytes;
    #if DEBUG_TRACE
        printf("%d : - send_resp_to_client()\n", getpid());
    #endif
    
}

void end_resp_to_client(int sockfd){
    #if DEBUG_TRACE
        printf("%d :- end_resp_to_client()\n", getpid());
    #endif

    if(close(sockfd) < 0){
        perror("Could not close chat file");
    }
    
}
