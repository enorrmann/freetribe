#include "sample_manager.h"
// #include "common/sample.h"
#include "types.h"

#define SDRAM_ADDRESS 0x00000000

fract16 *data_sdram;
uint32_t wavtab_index = 0;

Sample samples[150];

void SMPMAN_init() {
    wavtab_index = 0;
    data_sdram = (fract16 *)SDRAM_ADDRESS;
    samples[0] = (Sample)malloc(sizeof(t_sample));

    samples[0]->quality = 0;        // HI
    samples[0]->start_position = 0; // HI
    samples[0]->loop_point = 0;  // NO LOOP
}

void SMPMAN_record(fract32 data) {
    wavtab_index++;
    data_sdram[wavtab_index >> samples[0]->quality] =
        (fract16)shr_fr1x32(data, 16); // ADJUST RECORDING FOR SELECTED QUALITY
}

void SMPMAN_record_reset() { wavtab_index = 0; }

void SMPMAN_set_parameter(int sample_number, int parameter, fract32 value) {
    //int sample_number = 0;
    switch (parameter) {
    case SAMPLE_PARAM_QUALITY:
        samples[sample_number]->quality = value;
        break;
    case SAMPLE_START_POINT:
        samples[sample_number]->start_position = value;
        break;
    case SAMPLE_LOOP_POINT:
        samples[sample_number]->loop_point = value;
        break;

    default:
        break;
    }
}
