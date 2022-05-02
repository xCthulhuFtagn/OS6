#ifndef LIST
#define LIST

#ifndef MAX_CHAT_NAME_LEN
#define MAX_CHAT_NAME_LEN 32
#endif

typedef struct {
    int socket;
    struct listofsubscribers* next;
} list_of_subscribers;

typedef struct {
    char name_of_chat[MAX_CHAT_NAME_LEN + 1];
    struct listofsubscribers* subs;
    struct listofchats* next;
} list_of_chats;

// #define struct listofchats list_of_chats
// #define struct listofsubscribers list_of_subscribers

void delChat(list_of_chats* lc, char chat_name[]);
void delSub(list_of_subscribers* lc, int sock);
void delChatList(list_of_chats* lc);
void delSubList(list_of_subscribers* ls);
list_of_chats* findChat(list_of_chats* lc, char* c);//returns NULL if not found
list_of_chats* addChat(list_of_chats* lc, char* c);//returns NULL on error
list_of_subscribers* addSub(list_of_subscribers* ls, int sub);//returns NULL on error

#endif //LIST