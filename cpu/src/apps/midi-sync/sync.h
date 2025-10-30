
#ifndef SYNC_H
#define SYNC_H  
#include <stdint.h>
#include <stdbool.h>
#include "freetribe.h"

#define SYNC_PPQN 1
#define SYNC_INTERNAL_BPM 60


void send_sync_out(uint16_t bpm, uint8_t ppqn) ;
void send_sync_out2(uint16_t pulse_period_ms) ;
void poll_sync_gpio() ;
void print_bpm();
#endif