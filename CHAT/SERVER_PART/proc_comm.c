#include "server_interface.h"

static void process_command(const message_t comm){
    message_t resp;
    int first_time = 1;
    resp = comm; // копирует команду обратно, затем изменяет resp, как требовалось

    if(!start_resp_from_server()){
        fprintf(stderr, "Server Warning:-\
                start_resp_to_client %d failed\n", resp.client_pid);
        return;
    }

    resp.responce = s_success;
    memset(resp.error_text, '\0', sizeof(resp.error_text));
    int save_errno = 0; //extra definition

    switch(resp.request){
        case c_create_new_database:
            if(!database_initialize(1)) resp.responce = s_failure;
            break;
        case c_get_cdc_entry:
            resp.cdc_entry_data = get_cdc_entry(comm.cdc_entry_data.catalog);
            break;
        case c_get_cdt_entry:
            resp.cdt_entry_data = get_cdt_entry(comm.cdt_entry_data.catalog,
                                                comm.cdt_entry_data.track_no);
            break;
        case c_add_cdc_entry:
            if(!add_cdc_entry(comm.cdc_entry_data)) resp.responce = s_failure;
            break;
        case c_add_cdt_entry:
            if(!add_cdt_entry(comm.cdt_entry_data)) resp.responce = s_failure;
            break;
        case c_del_cdc_entry:
            if(!del_cdc_entry(comm.cdc_entry_data.catalog)) resp.responce = s_failure;
            break;
        case c_del_cdt_entry:
            if(!del_cdt_entry(comm.cdc_entry_data.catalog, comm.cdt_entry_data.track_no)) resp.responce = s_failure;
            break;
        default:
            resp.responce = s_failure;
            break;
    }
    sprintf(resp.error_text, "Command failed: \n\t%s\n", stderror(save_errno));
    if(!send_resp_to_client(resp)){
        fprintf(stderr, "Server Warning:- failed to respond to %d\n", resp.client_pid);
    }
    end_resp_to_client();
    return;
}