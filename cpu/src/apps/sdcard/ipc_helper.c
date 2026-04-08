#include "ipc_helper.h"
#include "parameters.h"

uint32_t IPC_CHUNK_TRANSFER_SIZE =
    16 * 1024; // Max transfer size in 32-bit words (must be <= 65535 for 16-bit
               // word count in metadata)
#define IPC_BUFFER_SIZE (1024 * 512)
uint32_t ipc_buffer[IPC_BUFFER_SIZE];
uint32_t ipc_buffer_index = 0; // Index for current position in ipc_buffer
const uint32_t initial_base_address = 0x00000060;

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

void ipc_simple_send(uint32_t value) {
            uint32_t to_send [1] = { value };
        int status = dev_dsp_ipc_transfer(
            base_address, to_send, sizeof(to_send), ipc_callback,
            (void *)0x23AC1D23 // arbitrary user context value for testing
        );
         ft_printf("simple sent: %u, status %i", value, status);
         status = dev_dsp_ipc_transfer(
            base_address+1, to_send, sizeof(to_send), ipc_callback,
            (void *)0x23AC1D23 // arbitrary user context value for testing
        );

         ft_printf("simple sent: %u, status %i", value, status);

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

void ipc_send_buffer_chunked() {
uint32_t total_samples = ipc_buffer_index; // total number of samples currently in the buffer    total_samples_read
    

    uint32_t num_chunks = (total_samples + IPC_CHUNK_TRANSFER_SIZE - 1) / IPC_CHUNK_TRANSFER_SIZE;
    ft_printf("Total samples: %i, num_chunks: %i", (int)total_samples, (int)num_chunks);
    uint32_t base_address = initial_base_address;
    for (uint32_t i = 0; i < num_chunks; i++) {

        uint32_t offset = (uint32_t)i * (uint32_t)IPC_CHUNK_TRANSFER_SIZE;

        int status = dev_dsp_ipc_transfer(
            base_address, &ipc_buffer[offset], IPC_CHUNK_TRANSFER_SIZE, ipc_callback,
            (void *)0x23AC1D23 // arbitrary user context value for testing
        );

         ft_printf("status: %d, chunk %u, count: %u, offset: %u, address: 0x%08x", status, i, IPC_CHUNK_TRANSFER_SIZE, offset, base_address);

        base_address += (uint32_t)IPC_CHUNK_TRANSFER_SIZE * sizeof(int32_t);
    }

    ft_set_module_param( 0, PARAM_TRANSMISSION_END, total_samples); // send total samples to dsp for playback
}
