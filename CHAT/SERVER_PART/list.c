#include "list.h"

#include <string.h>
#include <malloc.h>

void delChat(list_of_chats* lc, char chat_name[]){
    list_of_chats *prev = lc;
    while(lc){
        lc = lc->next;
        if (strcmp(chat_name, lc->name_of_chat) == 0) {
            prev->next = lc->next;
            free(lc);
        }
    }
}

void delSub(list_of_subscribers* lc, int sock){
    list_of_subscribers *prev = lc;
    while(lc){
        lc = lc->next;
        if (sock == lc->socket) {
            prev->next = lc->next;
            free(lc);
        }
    }
}

void delChatList(list_of_chats* lc){
    list_of_chats *prev;
    while(lc){
        prev = lc;
        lc = lc->next;
        free(prev);
    }
}

void delSubList(list_of_subscribers* ls){
    list_of_subscribers *prev;
    while(ls){
        prev = ls;
        ls = ls->next;
        free(prev);
    }
}

list_of_chats* findChat(list_of_chats* lc, char* c){
    list_of_chats* runner = lc;
    while(runner && strnmp(runner->name_of_chat, c) != 0) runner = runner->next;
    return runner;
}

list_of_chats* addChat(list_of_chats* lc, char* c){
    if(!lc){
        lc = (list_of_chats*) malloc(sizeof(list_of_chats));
        if(!lc) return lc;
        lc->next = NULL;
        strncpy(lc->name_of_chat, c, 256);
        lc->subs = NULL;
        return lc;
    }
    list_of_chats* inserter = lc;
    while(inserter->next) inserter = inserter->next;
    inserter->next = (list_of_chats*) malloc(sizeof(list_of_chats));
    if(!inserter->next) return NULL;
    inserter = inserter->next;
    strncpy(inserter->name_of_chat, c, 256);
    inserter->subs = NULL;
    return inserter;
}

list_of_subscribers* addSub(list_of_subscribers* ls, int sub){
    if(!ls){
        ls = (list_of_subscribers*) malloc(sizeof(list_of_subscribers));
        if(!ls) return ls;
        ls->next = NULL;
        ls->socket = sub;
        return ls;
    }
    list_of_subscribers* inserter = ls;
    while(inserter->next) inserter = inserter->next;
    inserter->next = (list_of_subscribers*) malloc(sizeof(list_of_subscribers));
    if(!inserter->next) return NULL;
    inserter = inserter->next;
    inserter->socket = sub;
    return inserter;
}
