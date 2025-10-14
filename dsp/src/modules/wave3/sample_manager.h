#ifndef SAMPLE_MANAGER_H
#define SAMPLE_MANAGER_H

#include "sample.h"
#include "types.h"
#include "aleph.h"


fract16 *data_sdram;
//uint32_t wavtab_index = 0;
extern Sample  samples[16];



extern fract16 *data_sdram;
extern uint32_t wavtab_index;


void SMPMAN_init ();
void SMPMAN_record(fract32 data);
void SMPMAN_record_reset();
void SMPMAN_set_parameter(int sample_number, int parameter, fract32 value) ;


#endif    