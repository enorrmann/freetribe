#include "ipc_helper.h"
#include "parameters.h"
#include "macros.h"

uint32_t IPC_CHUNK_TRANSFER_SIZE =
    16 * 1024; // Max transfer size in 32-bit words (must be <= 65535 for 16-bit
               // word count in metadata)
#define IPC_BUFFER_SIZE (1024 * 512)
uint32_t ipc_buffer[IPC_BUFFER_SIZE];
uint32_t total_samples = 0; // Index for current position in ipc_buffer
uint32_t total_chunks_sent = 0; // Number of chunks sent
const uint32_t initial_base_address = 0x00000060;

static void ipc_send_chunk(uint32_t chunk_number) ;

void ipc_init_buffer() { 
    total_samples = 0;
    total_chunks_sent = 0;
    // zero out the buffer
    uint32_t i;
    for (i = 0; i < IPC_BUFFER_SIZE; i++) {
        ipc_buffer[i] = 0;  
    } 
}

void ipc_callback(void *ctx, t_ipc_status status) {
    DEBUG_LOG("IPC callback called with user context");
    DEBUG_LOG("IPC transfer status from test: %i", (int)status);
}

void ipc_simple_send(uint32_t value) {
            uint32_t to_send [1] = { value };
        int status = dev_dsp_ipc_transfer(
            initial_base_address, to_send, sizeof(to_send), ipc_callback,
            (void *)0x23AC1D23 // arbitrary user context value for testing
        );

         DEBUG_LOG("simple sent: %u, status %i", value, status);

}

void ipc_send_last_chunk() {
        if (total_chunks_sent == 0) {
            ft_set_module_param(0, PARAM_TRANSMISSION_END, total_samples);

        } else {
            ft_set_module_param(0, PARAM_SAMPLE_COUNT_UPDATE, total_samples);
        }
    ipc_send_chunk(total_chunks_sent);
}

void ipc_add_to_buffer(uint32_t sample) {
    ipc_buffer[total_samples++] = sample;
    static uint32_t partial_samples_sent = 0;
    partial_samples_sent++;
    if (partial_samples_sent == IPC_CHUNK_TRANSFER_SIZE) {
        
        if (total_chunks_sent == 0) {
            ft_set_module_param(0, PARAM_TRANSMISSION_END, total_samples);

        } else {
            ft_set_module_param(0, PARAM_SAMPLE_COUNT_UPDATE, total_samples);
        }

        partial_samples_sent = 0;
        
        ipc_send_chunk(total_chunks_sent); // send chunk when buffer is full
        total_chunks_sent++;
    }
}

void ipc_send_buffer_via_param() {
    
    uint32_t i;
    ft_set_module_param(0, PARAM_TRANSMISSION_START, 0);
    for (i = 0; i < total_samples; i++) {
        ft_set_module_param(0, PARAM_SAMPLE_LOAD, ipc_buffer[i]);
    }
    ft_set_module_param(0, PARAM_TRANSMISSION_END, total_samples);

}

static void ipc_send_chunk(uint32_t chunk_number) {
    uint32_t offset = chunk_number * IPC_CHUNK_TRANSFER_SIZE;
    uint32_t address = initial_base_address + chunk_number * IPC_CHUNK_TRANSFER_SIZE * sizeof(int32_t);

    int status = dev_dsp_ipc_transfer(
        address, &ipc_buffer[offset], IPC_CHUNK_TRANSFER_SIZE, ipc_callback,
        (void *)0x23AC1D23
    );

    DEBUG_LOG("status: %d, chunk %u, count: %u, offset: %u, address: 0x%08x",
              status, chunk_number, IPC_CHUNK_TRANSFER_SIZE, offset, address);
}

void ipc_send_buffer_chunked() {
    // send total samples to dsp before chunks to avoid playback delay
    ft_set_module_param(0, PARAM_TRANSMISSION_END, total_samples);

    uint32_t total_chunks = (total_samples + IPC_CHUNK_TRANSFER_SIZE - 1) / IPC_CHUNK_TRANSFER_SIZE;
    DEBUG_LOG("Total samples: %i, num_chunks: %i", (int)total_samples, (int)total_chunks);

    for (uint32_t chunk_number = 0; chunk_number < total_chunks; chunk_number++) {
        ipc_send_chunk(chunk_number);
    }
}
