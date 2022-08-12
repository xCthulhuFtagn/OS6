#include "server_interface.h"
#include <errno.h> //extra
#include <dirent.h>
#include <sys/socket.h>

std::unordered_map<std::string, std::unordered_set<int>> chats;

static int server_fd = -1;
static pid_t my_pid = 0;
static char client_pipe_name[PATH_MAX + 1] = {'\0'};
static int client_fd = -1;
static int client_write_fd = -1;

int server_starting(){
    #if DEBUG_TRACE
         printf("%d :- server_starting()\n", getpid());
    #endif
    DIR* cur_dir = opendir(".");
    if(!cur_dir){
        perror("Could not open the current directory");
        return 0;
    }
    dirent *file;
    while((file = readdir(cur_dir))) chats[file->d_name] = {};
    return 1;
}

void server_ending(){
    #if DEBUG_TRACE
        printf("%d :- server_ending()\n", getpid());
    #endif
}

int read_request_from_client(client_data_t* received, int sockfd){
    // int return_code = 0;
    // int read_bytes;
    #if DEBUG_TRACE
        printf("%D :- read_request_from_client()\n", getpid());
    #endif
    return recv(sockfd, (void*)received, sizeof(client_data_t), MSG_WAITALL);
}

int send_resp_to_client(const server_data_t* resp, int sockfd){
    int write_bytes;
    #if DEBUG_TRACE
        printf("%d : - send_resp_to_client()\n", getpid());
    #endif
    return send(sockfd, (void*)resp, sizeof(server_data_t), 0);
}

void end_resp_to_client(int sockfd){
    #if DEBUG_TRACE
        printf("%d :- end_resp_to_client()\n", getpid());
    #endif

    if(close(sockfd) < 0){
        perror("Could not close clients socket");
    }
}
