#include "autosampler.h"
#include "sample.h"
#include "module_interface.h"
#include "freetribe.h"


static int record_timer;
static int record_time_in_ms;
static char chan, start_note, current_note, end_note, vel;
static int step_in_semitones;
static int recording = 0;
static int current_sample = 0;

void AUTOSAMPLER_init(char p_start_note, char p_end_note, int p_step_in_semitones,
          int p_record_time_in_ms) {
    chan = 0;
    record_timer = record_time_in_ms - 200; 
    record_time_in_ms = p_record_time_in_ms;
    start_note = p_start_note;
    end_note = p_end_note;
    step_in_semitones = p_step_in_semitones;
    current_note = p_start_note-p_step_in_semitones; // lazy hack
    vel = 100;
    recording = 1;
    current_sample = 0;
}

void AUTOSAMPLER_call_every_ms() {
    if (!recording){
                return;}

    record_timer++;
    if (record_timer >= record_time_in_ms) {
        record_timer = 0;
        ft_send_note_off(chan, current_note, vel);
        current_note += step_in_semitones;
        if (current_note > end_note) {
            recording = 0;        
        } else {
            /*if (current_note == start_note){ //start recording
                ft_set_module_param(0, SAMPLE_RECORD_START, 1);
            }*/
            ft_send_note_on(chan, current_note, vel);
         //   module_set_param_sample(current_sample, SAMPLE_MARK_START_POINT, 0);
        }
    }
}