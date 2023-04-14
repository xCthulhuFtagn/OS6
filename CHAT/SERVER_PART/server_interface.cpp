#include "server_interface.h"
#include "logger.h"

#include <errno.h> //extra
#include <filesystem>
#include <sys/socket.h>

std::unordered_map<std::string, chat_info> chats;
std::unordered_map<int, std::string> user_data;
std::unordered_set<std::string> used_usernames;

int disco_pipe[2], list_chats_pipe[2], username_enter_pipe[2];


int server_starting(){
    namespace fs = std::filesystem;

    boost::json::value data = { {"pid", getpid()} };
    logger::Logger::GetInstance().Log("Server started"sv, data);

    fs::path path_of_chats(".");
    for (const auto& entry : fs::directory_iterator(path_of_chats)) {
        if (entry.is_regular_file() && fs::path(entry).extension() == ".chat") {
            // chats[fs::path(entry).stem()];
            chats[fs::path(entry).stem()].subs = {};
        }
    }
    return 1;
}

void server_ending(){
    boost::json::value data = {};
    logger::Logger::GetInstance().Log("Server ended"sv, data);
}

std::string StringifyRequest(client_request_e r){
    switch(r){
        case 0: return "c_set_name"s;
        case 1: return "c_create_chat"s;
        case 2: return "c_connect_chat"s;
        case 3: return "c_send_message"s;
        case 4: return "c_leave_chat"s;
        case 5: return "c_disconnect"s;
        case 6: return "c_receive_message"s;
        case 7: return "c_wrong_request"s;
        case 8: return "c_get_chats"s;
        default: return "wrong input"s;
    }
}

std::string StringifyResponse(server_response_e r){
    switch(r){
        case 0: return "s_success"s;
        case 1: return "s_failure"s;
        default: return "wrong input"s;
    }
}

int read_request_from_client(client_data_t* received, int sockfd){
    size_t length;
    int err;
    // #if DEBUG_TRACE
    //     printf("%D :- read_request_from_client()\n", getpid());
    // #endif
    
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

    boost::json::value data = {
        {"request", 
            {   
                {"client_request_e", StringifyRequest(received->request)},
                {"message", received->message_text}
            }
        }
    };
    logger::Logger::GetInstance().Log("Read request from client"sv, data);

    return length + 1;
}

bool send_resp_to_client(const server_data_t* resp, int sockfd){
    #if DEBUG_TRACE
        printf("%d : - send_resp_to_client()\n", getpid());
    #endif
    int written_bytes;
    written_bytes = send(sockfd, (void*)(&resp->request), sizeof(client_request_e), MSG_WAITALL | MSG_NOSIGNAL);
    if(written_bytes < 0) return false;
    written_bytes = send(sockfd, (void*)(&resp->responce), sizeof(server_response_e), MSG_WAITALL | MSG_NOSIGNAL);
    if(written_bytes < 0) return false;
    size_t length = resp->message_text.size();
    written_bytes = send(sockfd, (void*)(&length), sizeof(size_t), MSG_WAITALL | MSG_NOSIGNAL);
    if(written_bytes < 0) return false;
    written_bytes = send(sockfd, (void*)(resp->message_text.c_str()), resp->message_text.size(), MSG_WAITALL | MSG_NOSIGNAL);
    if(written_bytes < 0) return false;

    boost::json::value data = {
        {"response", 
            {   
                {"client_request_e", StringifyRequest(resp->request)},
                {"server_response_e", StringifyResponse(resp->responce)},
                {"message", resp->message_text}
            }
        }
    };
    logger::Logger::GetInstance().Log("Sent response to client"sv, data);

    return true;
}

void end_resp_to_client(int sockfd){
    #if DEBUG_TRACE
        printf("%d :- end_resp_to_client()\n", getpid());
    #endif

    if(close(sockfd) < 0){
        boost::json::value data = {};
        logger::Logger::GetInstance().Log("Sent response to client"sv, data);
    }
    //perror("Could not close clients socket");
}

bool send_available_chats(int sockfd){
    server_data_t resp;
    resp.request = c_get_chats;
    resp.responce = s_success;
    for (auto it = chats.begin(); it != chats.end(); ++it){
        resp.message_text += it->first + "\n";
    }
    return send_resp_to_client(&resp, sockfd);
}
