#define _POSIX_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cd_data.h"
#include "client_interface.h"

static pid_t mypid;
static int read_one_responce(message_db_t* rec_ptr);

int database_initalize(const int new_database){
    if(!client_starting()) return 0;
    mypid = getpid();
    return 1;
} //инициализация базы данных

void database_close(void){
    client_ending();
}

cdc_entry get_cdc_entry(const char* cd_catalog_ptr){
    cdc_entry ret_val;
    message_db_t mess_send;
    message_db_t mess_ret;

    ret_val.catalog[0] = '\0';
    mess_send.client_pid = mypid;
    mess_send.request = s_get_cdc_entry;
    strcpy(mess_send.cdc_entry_data.catalog, cd_catalog_ptr);

    if(send_mess_to_server(&mess_send)){
        if(read_one_responce(&mess_ret)){
            if(mess_ret.responce == r_success){
                ret_val = mess_ret.cdc_entry_data;
            }else{
                fprintf(stderr, "%s", mess_ret.error_text);
            }
        }else{
            fprintf(stderr, "Server failed to respond\n");
        }
    }else{
        fprintf(stderr, "Server not accepting requests\n");
    }
    return ret_val;
}

static int read_one_responce(message_db_t* rec_ptr){
    int return_code = 0;
    if(!rec_ptr) return 0;
    if(start_resp_from_server()){
        if(read_resp_from_server(rec_ptr)){
            return_code = 1;
        }
        end_resp_from_server();
    }
    return return_code;
}

cdt_entry get_cdt_entry(const char* cd_catalog_ptr, const int track_no){
    cdt_entry ret_val;
    message_db_t mess_send;
    message_db_t mess_ret;

    ret_val.catalog[0] = '\0';
    mess_send.client_pid = mypid;
    mess_send.request = s_get_cdt_entry;
    strcpy(mess_send.cdt_entry_data.catalog, cd_catalog_ptr);
    mess_send.cdt_entry_data.track_no = track_no;

    if(send_mess_to_server(&mess_send)){
        if(read_one_responce(&mess_ret)){
            if(mess_ret.responce == r_success){
                ret_val = mess_ret.cdt_entry_data;
            }else{
                fprintf(stderr, "%s", mess_ret.error_text);
            }
        }else{
            fprintf(stderr, "Server failed to respond\n");
        }
    }else{
        fprintf(stderr, "Server not accepting requests\n");
    }
    return ret_val;
}

int add_cdc_entry(const cdc_entry entry_to_add){
    message_db_t mess_send;
    message_db_t mess_ret;

    mess_send.client_pid = mypid;
    mess_send.request = s_add_cdc_entry;
    mess_send.cdc_entry_data = entry_to_add;

    if(send_mess_to_server(&mess_send)){
        if(read_one_responce(&mess_ret)){
            if(mess_ret.responce == r_success){
                return 1;
            }else{
                fprintf(stderr, "%s", mess_ret.error_text);
            }
        }else{
            fprintf(stderr, "Server failed to respond\n");
        }
    }else{
        fprintf(stderr, "Server not accepting requests\n");
    }
    return 0;
}

int add_cdt_entry(const cdt_entry entry_to_add){
    message_db_t mess_send;
    message_db_t mess_ret;

    mess_send.client_pid = mypid;
    mess_send.request = s_add_cdc_entry;
    mess_send.cdt_entry_data = entry_to_add;

    if(send_mess_to_server(&mess_send)){
        if(read_one_responce(&mess_ret)){
            if(mess_ret.responce == r_success){
                return 1;
            }else{
                fprintf(stderr, "%s", mess_ret.error_text);
            }
        }else{
            fprintf(stderr, "Server failed to respond\n");
        }
    }else{
        fprintf(stderr, "Server not accepting requests\n");
    }
    return 0;
}

int del_cdc_entry(const char* cd_catalog_ptr){
    message_db_t mess_send;
    message_db_t mess_ret;

    mess_send.client_pid = mypid;
    mess_send.request = s_del_cdc_entry;
    strcpy(mess_send.cdc_entry_data.catalog, cd_catalog_ptr);

    if(send_mess_to_server(&mess_send)){
        if(read_one_responce(&mess_ret)){
            if(mess_ret.responce == r_success){
                return 1;
            }else{
                fprintf(stderr, "%s", mess_ret.error_text);
            }
        }else{
            fprintf(stderr,"Server failed to respond\n");
        }
    }else{
        fprintf(stderr, "Server not accepting requests\n");
    }
    return 0;
}

int del_cdt_entry(const char* cd_catalog_ptr, const int track_no){
    message_db_t mess_send;
    message_db_t mess_ret;

    mess_send.client_pid = mypid;
    mess_send.request = s_del_cdt_entry;
    strcpy(mess_send.cdt_entry_data.catalog, cd_catalog_ptr);
    mess_send.cdt_entry_data.track_no = track_no;
    if(send_mess_to_server(&mess_send)){
        if(read_one_responce(&mess_ret)){
            if(mess_ret.responce == r_success){
                return 1;
            }else{
                fprintf(stderr, "%s", mess_ret.error_text);
            }
        }else{
            fprintf(stderr,"Server failed to respond\n");
        }
    }else{
        fprintf(stderr, "Server not accepting requests\n");
    }
    return 0;
}

cdc_entry search_cdc_entry(const char*  cd_catalog_ptr, int* first_call_ptr){
    message_db_t mess_send;
    message_db_t mess_ret;

    static FILE* work_file = (FILE*) 0;
    static int entries_matching = 0;
    cdc_entry ret_val;

    ret_val.catalog[0] = '\0';
    if(!work_file && (*first_call_ptr == 0)) return ret_val;
    if(*first_call_ptr){
        *first_call_ptr = 0;
        if(work_file) fclose(work_file);
        work_file = tmpfile();
        if(!work_file) return ret_val;

        mess_send.client_pid = mypid;
        mess_send.request = s_find_cdc_entry;
        strcpy(mess_send.cdc_entry_data.catalog, cd_catalog_ptr);
        if(send_mess_to_server(&mess_send)){
            if(read_one_responce(&mess_ret)){
                while(read_resp_from_server(&mess_ret)){
                    if(mess_ret.responce == r_success){
                        fwrite(&mess_ret.cdc_entry_data, sizeof(cdc_entry), 1, work_file);
                        entries_matching++;
                    }else{
                        break;
                    }
                }
            }else{
                fprintf(stderr,"Server failed to respond\n");
            }
        }else{
            fprintf(stderr, "Server not accepting requests\n");
        }
        if(entries_matching == 0){
            fclose(work_file);
            work_file = (FILE*) 0;
            return ret_val;
        }
        (void) fseek(work_file, 0L, SEEK_SET);
    } else {
        // не first_call_ptr
        if(entries_matching == 0){
            fclose(work_file);
            work_file = (FILE*)0;
            return ret_val;
        }
    }

    fread(&ret_val, sizeof(cdc_entry), 1, work_file);
    entries_matching--;

    return ret_val;
}