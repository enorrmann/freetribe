#ifndef IPC_HELPER_H
#define IPC_HELPER_H

#include "dev_dsp_ipc.h"
#include "freetribe.h"


void ipc_init_buffer();
void ipc_send_buffer_chunked(uint32_t total_samples) ;
void ipc_add_to_buffer (uint32_t sample) ;

#endif // IPC_HELPER_H