
#ifndef SYNC_H
#define SYNC_H  
#include <stdint.h>
#include <stdbool.h>
#include "freetribe.h"


#define PULSE_LENGHT 3
#define SYNC_PPQN 4
#define SYNC_INTERNAL_BPM 128
#define MIDI_SYNC_PPQN 24

#define rising_edge_bank  7
#define rising_edge_pin   9
#define rising_edge_out_bank  6
#define rising_edge_out_pin  11



void send_sync_out() ;
void poll_sync_gpio() ;
void print_bpm();
void send_sync_out_pulse_start();
void send_sync_out_pulse_end();
void check_sync_out_pulse_end();
void svc_midi_send_clock();
void svc_midi_send_start();
void svc_midi_send_continue();
void svc_midi_send_stop() ;
#endif