#include "sample_manager.h"
 #include "common/parameters.h"
#include "types.h"

#define SDRAM_ADDRESS 0x00000000

fract16 *data_sdram;
uint32_t wavtab_index = 0;

t_sample samples[MAX_SAMPLES];

void SMPMAN_init() {
    wavtab_index = 0;
    data_sdram = (fract16 *)SDRAM_ADDRESS;

    /*int i = 0;
    for (i = 0; i < 16; i++) {
        samples[i] = (Sample)malloc(sizeof(t_sample));
        samples[i]->quality = 0;
        samples[i]->start_position = 0;
        samples[i]->loop_point = 0; // NO LOOP
        samples[i]->playback_rate = 1;
        samples[i]->global_offset = 0;
    }*/
}

void SMPMAN_record(fract32 data) {
    Sample s = &samples[0];
    wavtab_index++;
    data_sdram[wavtab_index >> s->quality] =
        (fract16)shr_fr1x32(data, 16); // ADJUST RECORDING FOR SELECTED QUALITY
}

void SMPMAN_record_reset() { wavtab_index = 0; }

void SMPMAN_set_parameter(int sample_number, int parameter, fract32 value) {
    // int sample_number = 0;
    Sample s = &samples[sample_number];
    switch (parameter) {
    case SAMPLE_PARAM_QUALITY:
        s->quality = value;
        break;
    case SAMPLE_START_POINT:
        s->start_position = value;
        break;
    case SAMPLE_LOOP_POINT:
        s->loop_point = value;
        break;
    case SAMPLE_PLAYBACK_RATE:
        s->playback_rate = value;
        break;
    case SAMPLE_GLOBAL_OFFSET:{
        int i;
        for ( i = 0; i < 16; i++) {
            s = &samples[i];
            s->global_offset = value;
        }
        }
        break;
    case SAMPLE_LENGTH:
            s->end_position = s->start_position + value;
            break;
    default:
        break;
    }
}
