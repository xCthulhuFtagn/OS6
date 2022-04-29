#include "cliserv.h"

static void process_command(const message_db_t comm){
    message_db_t resp;
    int first_time = 1;
    resp = comm; // копирует команду обратно, затем изменяет resp, как требовалось

    if(!start_resp_from_server(resp)){
        fprintf(stderr, "Server Warning:-\
                start_resp_to_client %d failed\n", resp.client_pid);
        return;
    }

    resp.responce = r_success;
    memset(resp.error_text, '\0', sizeof(resp.error_text));
    save_errno = 0;

    switch(resp.request){
        case s_create_new_database:
            if(!database_initialize(1)) resp.responce = r_failure;
            break;
        case s_get_cdc_entry:
            resp.cdt_entry_data = get_cdc_entry(comm.cdc_entry_data.catalog);
            break;
        case s_get_cdt_entry:
            resp.cdt_entry_data = get_cdt_entry(comm.cdt_entry_data.catalog,
                                                comm.cdt_entry_data.track_no);
            break;
        case s_add_cdc_entry:
            if(!add_cdc_entry(comm.cdc_entry_data)) resp.responce = r_failure;
            break;
        case s_add_cdt_entry:
            if(!add_cdt_entry(comm.cdt_entry_data)) resp.responce = r_failure;
            break;
        case s_del_cdc_entry:
            if(!add_cdc_entry(comm.cdc_entry_data.catalog)) resp.responce = r_failure;
            break;
        case s_del_cdt_entry:
            if(!del_cdt_entry(comm.cdc_entry_data.catalog, comm.cdt_entry_data.track_no)) resp.responce = r_failure;
            break;
        case s_find_cdc_entry:
            do{
                resp.cdc_entry_data = search_cdc_entry(comm.cdc_entry_data.catalog, &first_time);
                if(resp.cdc_entry_data.catalog[0] != 0){
                    resp.responce = r_success;
                    if(!send_resp_to_client(resp)){
                        fprintf(stderr, "Server Warning:- failed to respond to %d\n", resp.client_pid);
                        break;
                    }
                }else{
                    resp.responce = r_find_no_more;
                }
            }while(resp.responce == r_success);
            break;
        default:
            resp.responce = r_failure;
            break;
    }
    sprintf(resp.error_text, "Command failed: \n\t%s\n", stderror(save_errno));
    if(!send_resp_to_client(resp)){
        fprintf(stderr, "Server Warning:- failed to respond to %d\n", resp.client_pid);
    }
    end_resp_to_client();
    return;
}