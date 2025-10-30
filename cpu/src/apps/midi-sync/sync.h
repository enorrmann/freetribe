
#ifndef SYNC_H
#define SYNC_H  
#include <stdint.h>
#include <stdbool.h>
#include "freetribe.h"

#define PULSE_LENGHT 2
#define SYNC_PPQN 24
#define SYNC_INTERNAL_BPM 240
#define MIDI_SYNC_PPQN 24

#define rising_edge_bank  7
#define rising_edge_pin   9
#define rising_edge_out_bank  6
#define rising_edge_out_pin  11



void send_sync_out(uint16_t bpm, uint8_t ppqn) ;
void send_sync_out2(uint16_t pulse_period_ms) ;
void poll_sync_gpio() ;
void print_bpm();
void send_sync_out_pulse_start();
void check_sync_out_pulse_end();
#endif