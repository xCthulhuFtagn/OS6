#include "server_interface.h"
#include <errno.h> //extra
#include <filesystem>
#include <sys/socket.h>

std::unordered_map<std::string, std::unordered_set<int>> chats;
std::unordered_map<int, std::string> user_data;
std::unordered_set<std::string> used_usernames;

static int server_fd = -1;
static pid_t my_pid = 0;
static char client_pipe_name[PATH_MAX + 1] = {'\0'};
static int client_fd = -1;
static int client_write_fd = -1;

int server_starting(){
    namespace fs = std::filesystem;
    #if DEBUG_TRACE
         printf("%d :- server_starting()\n", getpid());
    #endif
    fs::path path_of_chats(".");
    for (const auto& entry : fs::directory_iterator(path_of_chats)) {
        if (entry.is_regular_file() && fs::path(entry).extension() == ".chat") {
            chats[fs::path(entry).filename()] = {};
        }
    }
    return 1;
}

void server_ending(){
    #if DEBUG_TRACE
        printf("%d :- server_ending()\n", getpid());
    #endif
}

int read_request_from_client(client_data_t* received, int sockfd){
    size_t length, off;
    int err;
    #if DEBUG_TRACE
        printf("%D :- read_request_from_client()\n", getpid());
    #endif
    err = 0, off = 0;
    do {
        off += err;
        err = recv(sockfd, (void*)(received->request) + off, sizeof(client_request_e) - off, MSG_DONTWAIT);
    } while (errno == 0 && off != sizeof(size_t));
    if (errno != 0 && errno != EAGAIN)
        return -1;
    if (errno == EAGAIN)
        return -EAGAIN;
    err = 0, off = 0;
    do {
        off += err;
        err = recv(sockfd, (void*)(&length) + off, sizeof(size_t) - off, MSG_DONTWAIT);
    } while (errno == 0 && off != sizeof(size_t));
    if (errno != 0 && errno != EAGAIN)
        return -1;
    if (errno == EAGAIN)
        return -EAGAIN;
    received->message_text.resize(length);
    err = 0, off = 0;
    do {
        off += err;
        recv(sockfd, (void*)(received->message_text.data()) + off, length - off, MSG_DONTWAIT);
    } while (errno == 0 && off != sizeof(size_t));
    if (errno != 0 && errno != EAGAIN)
        return -1;
    if (errno == EAGAIN)
        return -EAGAIN;
    return err;
}

void send_resp_to_client(const server_data_t* resp, int sockfd){
    #if DEBUG_TRACE
        printf("%d : - send_resp_to_client()\n", getpid());
    #endif
    int written_bytes;
    written_bytes = send(sockfd, (void*)(&resp->responce), sizeof(server_responce_e), MSG_WAITALL);
    size_t length = resp->message_text.size();
    written_bytes = send(sockfd, (void*)(&length), sizeof(size_t), MSG_WAITALL);
    written_bytes = send(sockfd, (void*)(resp->message_text.c_str()), resp->message_text.size(), MSG_WAITALL);
}

void end_resp_to_client(int sockfd){
    #if DEBUG_TRACE
        printf("%d :- end_resp_to_client()\n", getpid());
    #endif

    if(close(sockfd) < 0){
        perror("Could not close clients socket");
    }
}

void get_available_chats(int sockfd){
    server_data_t resp;
    resp.responce = s_success;
    for (auto chat : chats){
        resp.message_text += chat.first + "\n";
    }
    send_resp_to_client(&resp, sockfd);
}
