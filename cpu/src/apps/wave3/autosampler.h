#ifndef AUTOSAMPLER_H
#define AUTOSAMPLER_H

#define SAMPLING_START_NOTE 48
#define SAMPLING_END_NOTE 60
#define SAMPLING_STEP_IN_SEMITONES 3
#define SAMPLE_RATE 48000

void AUTOSAMPLER_init(char p_start_note, char p_end_note,int p_step_in_semitones, int p_record_time_in_ms);
void AUTOSAMPLER_call_every_ms();
#endif