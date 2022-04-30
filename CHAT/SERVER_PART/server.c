#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "server_interface.h"


static int server_running = 1;
static void process_command(const message_t mess_command);

void catch_signals(){
    server_running = 0;
}

int main(int argc, char* argv[]){
    message_t mess_command;

    
    //add if not able to open the files && sockets
    //mb add it all to server_starting()
    if(!server_starting()) exit(EXIT_FAILURE);

    int sockfd;
    int len;
    struct sockaddr_un adress;
    int result;
    char ch = 'A';

    unlink("server_socket");
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    adress.sun_family = AF_UNIX;
    strcpy(adress.sun_path, "server_socket");
    len = sizeof(adress);
    bind(sockfd, (struct sockaddr*) &adress, len);

    listen(sockfd, 1024);
    while(server_running){
        if(read_request_from_client(&mess_command)){
            process_command(mess_command);
        }else{
            if(server_running) fprintf(stderr, "Server ended - can not read pipe\n");
            server_running = 0;
        }
    }
    server_ending();
    exit(EXIT_SUCCESS);
}
