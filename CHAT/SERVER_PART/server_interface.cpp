#include "server_interface.h"
#include <errno.h> //extra
#include <filesystem>
#include <sys/socket.h>

std::unordered_map<std::string, std::pair<std::pair<std::unordered_set<int>, chat_pipe>, chat_pipe>> chats;
std::unordered_map<int, std::string> user_data;
std::unordered_set<std::string> used_usernames;

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

// int recv_part_of_request(void* object, size_t length, int sockfd){
//     size_t off = 0;
//     int err;
//     while (recv(sockfd, object + off, length - off, 0)) {
//         if (errno == EAGAIN) continue;
//         if (err == -1) return -1;
//         off += err;
//     }
//     return 0;
// }

// int recv_part_of_request(void* object, size_t length, int sockfd){
//     size_t off = 0;
//     int err;
//     while (recv(sockfd, object + off, length - off, 0)) {
//         if (errno == EAGAIN) continue;
//         if (err == -1) return -1;
//         off += err;
//     }
//     return 0;
// }

int read_request_from_client(client_data_t* received, int sockfd){
    size_t length;
    int err;
    #if DEBUG_TRACE
        printf("%D :- read_request_from_client()\n", getpid());
    #endif
    err = recv(sockfd, (void*)(&(received->request)), sizeof(client_request_e), MSG_WAITALL);
    if (err != sizeof(client_request_e))
        return err;
    err = recv(sockfd, (void*)(&length), sizeof(size_t), MSG_WAITALL);
    if (err != sizeof(size_t))
        return err;
    received->message_text.resize(length);
    err = recv(sockfd, (void*)(received->message_text.data()), length, MSG_WAITALL);
    if (err != length)
        return err;
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

void send_available_chats(int sockfd){
    server_data_t resp;
    resp.responce = s_success;
    for (auto chat : chats){
        resp.message_text += chat.first + "\n";
    }
    send_resp_to_client(&resp, sockfd);
}
