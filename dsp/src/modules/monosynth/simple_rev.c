#include <stdint.h>
#include "aleph.h"

#define SDRAM_ADDRESS 0x00000000
#define NUM_LINES 4
#define MS_IN_SAMPLES(ms) ((ms * 48) )


int32_t *reverb_buf = (int32_t *)SDRAM_ADDRESS;

static int line_n_reverb_pos[] = {0, 0, 0, 0, 0, 0, 0, 0}; // Separate positions for each delay line
static int line_n_memory_offset[NUM_LINES];

static int line_n_sample_length[] = {
    MS_IN_SAMPLES(250),  
    MS_IN_SAMPLES(350),  
    MS_IN_SAMPLES(430),  
    MS_IN_SAMPLES(600)   
};
/*static int line_n_sample_length[] = {
    MS_IN_SAMPLES(FX_REVERB_T0_MS),  
    MS_IN_SAMPLES(FX_REVERB_T1_MS),  
    MS_IN_SAMPLES(FX_REVERB_T2_MS),  
    MS_IN_SAMPLES(FX_REVERB_T3_MS),   
    MS_IN_SAMPLES(FX_REVERB_T4_MS),   
    MS_IN_SAMPLES(FX_REVERB_T5_MS),   
    MS_IN_SAMPLES(FX_REVERB_T6_MS),   
    MS_IN_SAMPLES(FX_REVERB_T7_MS)    
};*/


void init_offset() {
    static int initialized = 0;
    if (initialized) return; // Evitar reinicialización
    int i;
    int offset = 0;
    for (i = 0; i < NUM_LINES; i++) {
        line_n_memory_offset[i] = offset;
        offset += line_n_sample_length[i];
    }
    initialized = 1;
}

int32_t apply_simple_reverb(int32_t input, int line_number) {
    int reverb_pos = line_n_reverb_pos[line_number];
    int sample_length = line_n_sample_length[line_number];

    // offset por línea (CLAVE)
    int offset = line_n_memory_offset[line_number];
    int32_t delayed = reverb_buf[offset + reverb_pos];
    int32_t feedback = delayed >> 1;
    reverb_buf[offset + reverb_pos] = input + feedback;

    // avanzar
    reverb_pos++;
    if (reverb_pos >= sample_length)
        reverb_pos = 0;

    // guardar SIEMPRE
    line_n_reverb_pos[line_number] = reverb_pos;

    //return (input >> 1) + (delayed >> 1);
    return delayed>>2;
}
int32_t apply_complex_reverb(int32_t input)
{
    init_offset(); // Asegurar que los offsets estén inicializados
    int64_t acc = input; // acumulador para evitar overflow
int line_number;
    for (line_number = 0; line_number < NUM_LINES; line_number++) {
        acc = add_fr1x32(acc,apply_simple_reverb(input, line_number));
    }

    // normalizar (1 dry + 4 wet)
//    acc /= (NUM_LINES);

    return (int32_t)acc;
}