#ifndef AUTOSAMPLER_H
#define AUTOSAMPLER_H
void AUTOSAMPLER_init(char p_start_note, char p_end_note,int p_step_in_semitones, int p_record_time_in_ms);
void AUTOSAMPLER_call_every_ms();
#endif