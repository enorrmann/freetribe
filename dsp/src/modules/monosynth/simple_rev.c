#include <stdint.h>
#include "aleph.h"

#define SDRAM_ADDRESS 0x00000000
#define NUM_LINES 4

int32_t *reverb_buf = (int32_t *)SDRAM_ADDRESS;

static int line_n_reverb_pos[] = {0, 0, 0, 0, 0, 0, 0, 0}; // Separate positions for each delay line
static int line_n_sample_length[] = {
    12011,  // 250 ms
    16807,  // 350 ms
    20593,  // 430 ms
    28933   // 600 ms
};

int32_t apply_simple_reverb(int32_t input, int line_number) {
    int reverb_pos = line_n_reverb_pos[line_number];
    int sample_length = line_n_sample_length[line_number];

    // offset por línea (CLAVE)
    int offset = 0;
    int i;
    for (i = 0; i < line_number; i++) {
        offset += line_n_sample_length[i];
    }

    int32_t delayed = reverb_buf[offset + reverb_pos];

    int32_t feedback = delayed *2/3; // Feedback con ganancia de 0.75 (ajustable)

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
    int64_t acc = input; // acumulador para evitar overflow
int line_number;
    for (line_number = 0; line_number < NUM_LINES; line_number++) {
        acc = add_fr1x32(acc,apply_simple_reverb(input, line_number));
    }

    // normalizar (1 dry + 4 wet)
//    acc /= (NUM_LINES);

    return (int32_t)acc;
}