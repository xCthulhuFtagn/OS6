#include "server_interface.h"
#include "server_routines.h"
#include "logger.h"

extern std::unordered_map<std::string, chat_info> chats;
extern std::unordered_set<std::string> used_usernames;
extern std::unordered_map<int, std::string> user_data;

extern int disco_pipe[2];
extern int list_chats_pipe[2];
extern int username_enter_pipe[2];

int main(int argc, char *argv[])
{
    LOG_INIT()

    using namespace std::chrono_literals;
    if (!server_starting())
        exit(EXIT_FAILURE);
    int listen_socket;
    struct sockaddr_in address;
    // fd_set readfds;

    /* creates an UN-named socket inside the kernel and returns
     * an integer known as socket descriptor
     * This function takes domain/family as its first argument.
     * For Internet family of IPv4 addresses we use AF_INET
     */
    listen_socket = socket(AF_INET, SOCK_STREAM /*| SOCK_NONBLOCK*/, IPPROTO_TCP);
    bzero(&address, sizeof(address));
    address.sin_port = htons(5000);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    /* 
     * The call to the function "bind()" assigns the details specified
     * in the structure address to the socket created in the step above
     */
    if (bind(listen_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        boost::json::value data = {};
        logger::Logger::GetInstance().Log("Bind error"sv, data);
        close(listen_socket);
        return EXIT_FAILURE;
    }

    if (listen(listen_socket, 1000) == -1)
    {
        boost::json::value data = {};
        logger::Logger::GetInstance().Log("Listen error"sv, data);
        shutdown(listen_socket, SHUT_RDWR);
        return EXIT_FAILURE;
    }
    if(pipe(list_chats_pipe) < 0){
        boost::json::value data = {};
        logger::Logger::GetInstance().Log("List of chats pipe opening error"sv, data);
        shutdown(listen_socket, SHUT_RDWR);
        return EXIT_FAILURE;
    }
    if(pipe(disco_pipe) < 0){
        boost::json::value data = {};
        logger::Logger::GetInstance().Log("Disconnection pipe opening error"sv, data);
        shutdown(listen_socket, SHUT_RDWR);
        close(list_chats_pipe[0]);
        close(list_chats_pipe[1]);
        return EXIT_FAILURE;
    }
    if(pipe(username_enter_pipe) < 0){
        boost::json::value data = {};
        logger::Logger::GetInstance().Log("Username enter pipe opening error"sv, data);
        shutdown(listen_socket, SHUT_RDWR);
        close(list_chats_pipe[0]);
        close(list_chats_pipe[1]);
        close(disco_pipe[0]);
        close(disco_pipe[1]);
        return EXIT_FAILURE;
    }
    fcntl(list_chats_pipe[0], F_SETFL, fcntl(list_chats_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(disco_pipe[0], F_SETFL, fcntl(disco_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(username_enter_pipe[0], F_SETFL, fcntl(username_enter_pipe[0], F_GETFL) | O_NONBLOCK);

    std::thread(user_name_enter, username_enter_pipe[0], list_chats_pipe[1]).detach();
    std::thread(list_of_chats, list_chats_pipe[0]).detach();
    std::thread(disconnect).detach();

    struct sockaddr_in client_address;
    socklen_t address_length = sizeof(sockaddr_in);
    int connection_socket;
    while ((connection_socket = accept(listen_socket, (struct sockaddr *)&client_address, &address_length)) != -1)
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = TIMEOUT_MICRO;
        setsockopt(connection_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        write(username_enter_pipe[1], &connection_socket, sizeof(int));
    }
    if (!(errno & (EAGAIN | EWOULDBLOCK)))
    {
        boost::json::value data = {};
        logger::Logger::GetInstance().Log("Accept failure"sv, data);
        return EXIT_FAILURE;
    }
    // perror("Server died with status");
    server_ending(); // because why not
    exit(EXIT_SUCCESS);
}
