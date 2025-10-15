#include "autosampler.h"
#include "freetribe.h"
#include "module_interface.h"
#include "sample.h"

static int record_timer;
static int record_time_in_ms;
static char chan, start_note, current_note, end_note, vel;
static int step_in_semitones;
static int recording = 0;
static int current_sample = 0;
static int recording_tail = 0;

static int tail_time_in_ms = 500;
static int tail_timer = 0;
static int tail_active = 0;

void AUTOSAMPLER_init(char p_start_note, char p_end_note,
                      int p_step_in_semitones, int p_record_time_in_ms) {
    chan = 0;
    record_timer = record_time_in_ms - 200;
    record_time_in_ms = p_record_time_in_ms;
    start_note = p_start_note;
    end_note = p_end_note;
    step_in_semitones = p_step_in_semitones;
    current_note = p_start_note - p_step_in_semitones; // lazy hack
    vel = 100;
    recording = 1;
    current_sample = 0;
    recording_tail = 0;
}

void old_AUTOSAMPLER_call_every_ms() {
    if (!recording) {
        return;
    }

    record_timer++;
    if (record_timer >= record_time_in_ms) {
        record_timer = 0;
        ft_send_note_off(chan, current_note, vel);
        current_note += step_in_semitones;
        if (current_note > end_note) {
            recording = 0;
        } else {

            ft_send_note_on(chan, current_note, vel);
        }
    }
}

void AUTOSAMPLER_call_every_ms() {
    if (!recording) return;

    record_timer++;

    // enviar NOTE OFF antes de terminar el total (dejando espacio para la cola)
    if (record_timer == (record_time_in_ms - tail_time_in_ms)) {
        ft_send_note_off(chan, current_note, vel);
    }

    // al cumplirse el tiempo total, pasamos a la siguiente nota
    if (record_timer >= record_time_in_ms) {
        record_timer = 0;

        current_note += step_in_semitones;
        if (current_note > end_note) {
            recording = 0;  // fin del proceso
        } else {
            ft_send_note_on(chan, current_note, vel);
        }
    }
}