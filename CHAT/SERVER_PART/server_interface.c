#include "server_interface.h"
#include <errno.h> //extra
#include <dirent.h>
#include <sys/socket.h>

static int server_fd = -1;
static pid_t my_pid = 0;
static char client_pipe_name[PATH_MAX + 1] = {'\0'};
static int client_fd = -1;
static int client_write_fd = -1;

int server_starting(list_of_chats* list){
    #if DEBUG_TRACE
         printf("%d :- server_starting()\n", getpid());
    #endif
    //check if the chat_sub file exists
    int chats = open("chat_sub", O_RDONLY | O_CREAT);
    if(chats < 0){
        perror("Could not open chat_sub");
        return 0;
    }
    int size;
    char buf[MAX_CHAT_NAME_LEN + 1];
    for(list_of_chats* runner = list; (size = read(chats, buf, sizeof(list->name_of_chat))) > 0; runner = runner->next){
        if(size < MAX_CHAT_NAME_LEN + 1) printf(perror, "Did not read full buffer of chat name");
        addChat(list, buf);
    }
    return 1;
}

void server_ending(list_of_chats* list){
    #if DEBUG_TRACE
        printf("%d :- server_ending()\n", getpid());
    #endif
    remove("chat_sub");
    int chats = open("chat_sub", O_WRONLY | O_CREAT | O_TRUNC);
    int size;
    for(list_of_chats* runner = list; (size = write(chats, (void*)runner, sizeof(list->name_of_chat))) > 0; runner = runner->next){
        if (size != sizeof(list->name_of_chat)) exit("MEGABRUH!!!");
    }
    delChatLis(list);
}

int read_request_from_client(client_data_t* received, int sockfd){
    int return_code = 0;
    int read_bytes;
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
    return write(sockfd, (void*)resp, sizeof(server_data_t));
}

void end_resp_to_client(int sockfd){
    #if DEBUG_TRACE
        printf("%d :- end_resp_to_client()\n", getpid());
    #endif

    if(close(sockfd) < 0){
        perror("Could not close chat file");
    }
}
