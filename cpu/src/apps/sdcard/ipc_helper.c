#include "ipc_helper.h"
#include "parameters.h"

uint32_t MAX_IPC_TRANSFER_SIZE =
    16 * 1024; // Max transfer size in 32-bit words (must be <= 65535 for 16-bit
               // word count in metadata)
#define IPC_BUFFER_SIZE (1024 * 512)
int32_t ipc_buffer[IPC_BUFFER_SIZE];
uint32_t ipc_buffer_index = 0; // Index for current position in ipc_buffer

void ipc_init_buffer() { 
    ipc_buffer_index = 0;
    // zero out the buffer
    uint32_t i;
    for (i = 0; i < IPC_BUFFER_SIZE; i++) {
        ipc_buffer[i] = 0;  
    } 
}

void ipc_callback(void *ctx, t_ipc_status status) {
    ft_printf("IPC callback called with user context");
    ft_printf("IPC transfer status from test: %i", (int)status);
}

void ipc_add_to_buffer(uint32_t sample) {
    ipc_buffer[ipc_buffer_index++] = sample;
}

void ipc_send_buffer_via_param(uint32_t total_samples) {
    uint32_t i;
    ft_set_module_param(0, PARAM_TRANSMISSION_START, 0);
    for (i = 0; i < total_samples; i++) {
        ft_set_module_param(0, PARAM_SAMPLE_LOAD, ipc_buffer[i]);
    }
    ft_set_module_param(0, PARAM_TRANSMISSION_END, total_samples);

}

void ipc_send_buffer_chunked(uint32_t total_samples) {
    uint32_t base_address = 0x00000080;

    uint32_t num_chunks = (total_samples + MAX_IPC_TRANSFER_SIZE - 1) / MAX_IPC_TRANSFER_SIZE;
    ft_printf("Total samples: %i, num_chunks: %i", (int)total_samples, (int)num_chunks);

    for (uint32_t i = 0; i < num_chunks; i++) {

        uint32_t offset = (uint32_t)i * (uint32_t)MAX_IPC_TRANSFER_SIZE;

        uint16_t count = MAX_IPC_TRANSFER_SIZE;
        if (offset + MAX_IPC_TRANSFER_SIZE > total_samples) {
            count = total_samples - offset;
        }

        int status = dev_dsp_ipc_transfer(
            base_address, &ipc_buffer[offset], count, ipc_callback,
            (void *)0x23AC1D23 // arbitrary user context value for testing
        );

         ft_printf("status: %d, chunk %u, count: %u, offset: %u, address: 0x%08x", status, i, count, offset, base_address);

        base_address += (uint32_t)count * sizeof(int32_t);
    }
}
