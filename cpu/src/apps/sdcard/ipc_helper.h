#ifndef IPC_HELPER_H
#define IPC_HELPER_H

#include "dev_dsp_ipc.h"
#include "freetribe.h"

void ipc_callback(void *ctx, t_ipc_status status);
void ipc_init_buffer();
void ipc_send_buffer_chunked() ;
void ipc_add_to_buffer (uint32_t sample) ;
void ipc_send_buffer_via_param() ; // for testing
void ipc_simple_send(uint32_t value) ; // for testing

#endif // IPC_HELPER_H