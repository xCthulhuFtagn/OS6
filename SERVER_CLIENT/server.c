#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

//#include "cd_data.h" // cliserv includes it
#include "cliserv.h"

int save_errno;
static int server_running = 1;
static void process_command(const message_db_t mess_command);

void catch_signals(){
    server_running = 0;
}

int main(int argc, char* argv[]){
    struct sigaction new_action, old_action;
    message_db_t mess_command;
    int database_init_type = 0;

    new_action.sa_handler = catch_signals;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    if((sigaction(SIGINT, &new_action, &old_action) != 0) ||
        (sigaction(SIGHUP, &new_action, &old_action) != 0) ||
        (sigaction(SIGTERM, &new_action, &old_action) != 0)){
            fprintf(stderr, "Server startup error, signal catching failed\n");
            exit(EXIT_FAILURE);
    }
    if(argc > 1){
        argv++;
        if(strncmp("-i", *argv, 2) == 0) database_init_type = 1;
    }
    if(!database_initialize(database_init_type)){
        fprintf(stderr, "Server error:-\
                        could not initialize database\n");
        exit(EXIT_FAILURE);
    }
    if(!server_starting()) exit(EXIT_FAILURE);
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
