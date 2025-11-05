/* sequencer.c
 * Robust corrected implementation with diagnostics.
 *
 * NOTE: bpm is interpreted as "number of beat_unit notes per minute".
 *       Example: beat_unit = 4, bpm = 120 -> 120 quarter notes per minute.
 */

#include "seq.h"
#include <stdio.h>

/* Global instance (declared in header as extern) */
Sequencer seq;
static float ms_per_step;

/*
 * Compute milliseconds per step.
 *
 * Semantics: bpm = number of notes of type `beat_unit` per minute.
 * Duration of one beat_unit note in ms = 60000 / bpm.
 * A step represents (1 / step_unit) of a whole note.
 * A beat_unit represents (1 / beat_unit) of a whole note.
 * Therefore: step duration = (60000 / bpm) * (beat_unit / step_unit)
 */
float SEQ_get_ms_per_step(void) {
    float ms_per_beat_unit = 60000.0f / seq.bpm; /* ms for one beat_unit note */
    float step_ratio = (float)seq.beat_unit / (float)seq.step_unit;
    return ms_per_beat_unit * step_ratio;
}

/* Apply pending parameters (for synced mode). Ensure they are sane. */
static void SEQ_apply_pending_params(void) {

    seq.bpm = seq.pending_bpm;
    seq.beats_per_bar = seq.pending_beats_per_bar;
    seq.beat_unit = seq.pending_beat_unit;
    seq.step_unit = seq.pending_step_unit;
    seq.num_steps = seq.pending_num_steps;
    ms_per_step = SEQ_get_ms_per_step();
}

/*
 * Initialize the sequencer.
 */
void SEQ_init(float bpm, uint8_t beats_per_bar, uint8_t beat_unit,
              uint8_t step_unit, uint8_t steps) {
    seq.bpm = bpm;
    seq.beats_per_bar = beats_per_bar;
    seq.beat_unit = beat_unit;
    seq.step_unit = step_unit;
    seq.num_steps = steps;

    seq.current_step = 0;
    seq.counter_ms = 0.0f;
    seq.update_mode = SEQ_MODE_IMMEDIATE;
    seq.state = SEQ_STATE_STOPPED;

    seq.pending_bpm = bpm;
    seq.pending_beats_per_bar = beats_per_bar;
    seq.pending_beat_unit = beat_unit;
    seq.pending_step_unit = step_unit;
    seq.pending_num_steps = steps;

    /* default pattern: all off, assign example notes */
    for (uint8_t i = 0; i < seq.num_steps; ++i) {
        seq.steps[i].active = false;
        seq.steps[i].note = 60 + i;
    }
    ms_per_step = SEQ_get_ms_per_step();
}

/*
 * Set update mode (immediate or synced).
 */
void SEQ_set_update_mode(SEQ_UpdateMode mode) { seq.update_mode = mode; }

/*
 * Schedule or immediately apply new parameters.
 */
void SEQ_set_params(float bpm, uint8_t beats_per_bar, uint8_t beat_unit,
                    uint8_t step_unit, uint8_t steps) {
    if (steps > MAX_STEPS)
        steps = MAX_STEPS;
    /* store pending */
    seq.pending_bpm = bpm;
    seq.pending_beats_per_bar = beats_per_bar;
    seq.pending_beat_unit = beat_unit;
    seq.pending_step_unit = step_unit;
    seq.pending_num_steps = steps;

    if (seq.update_mode == SEQ_MODE_IMMEDIATE) {

        SEQ_apply_pending_params();
    }
}

/*
 * Start sequencer from step 0.
 */
void SEQ_start(void) {
    seq.current_step = 0;
    seq.counter_ms = 0.0f;
    seq.state = SEQ_STATE_RUNNING;
}

/*
 * Pause sequencer (hold current position).
 */
void SEQ_pause(void) {
    if (seq.state == SEQ_STATE_RUNNING) {
        seq.state = SEQ_STATE_PAUSED;
    }
}

/*
 * Continue sequencer after pause.
 */
void SEQ_continue(void) {
    if (seq.state == SEQ_STATE_PAUSED) {
        seq.state = SEQ_STATE_RUNNING;
    }
}

/*
 * Stop sequencer and reset to step 0.
 */
void SEQ_stop(void) {
    seq.state = SEQ_STATE_STOPPED;
    seq.current_step = 0;
    seq.counter_ms = 0.0f;
}

/*
 * Called every 1 ms to advance time and trigger steps.
 */
void SEQ_tick(void) {
    if (seq.state != SEQ_STATE_RUNNING)
        return;

    seq.counter_ms += 1.0f;

    if (seq.counter_ms >= ms_per_step) {
        seq.counter_ms -= ms_per_step;
        seq.current_step++;
        if (seq.current_step >= seq.num_steps)
            seq.current_step = 0;

        /* apply pending only after we advanced the step (so change occurs at
         * boundary) */
        if (seq.update_mode == SEQ_MODE_SYNCED) {
            SEQ_apply_pending_params();
        }

        if (seq.steps[seq.current_step].active) {
            // When the step is executed:
            if (seq.steps[seq.current_step].on_trigger) {
                seq.steps[seq.current_step].on_trigger();
            }
            SEQ_trigger(seq.steps[seq.current_step].note);
        }
    }
}

/* Utilities (same as before) */
void SEQ_clear_pattern(void) {
    for (uint8_t i = 0; i < seq.num_steps; ++i)
        seq.steps[i].active = false;
}

void SEQ_fill_every_n(uint8_t n) {
    if (n == 0)
        return;
    SEQ_clear_pattern();
    for (uint8_t i = 0; i < seq.num_steps; ++i)
        if ((i % n) == 0)
            seq.steps[i].active = true;
}

void SEQ_fill_on_beats(void) {
    SEQ_clear_pattern();
    if (seq.num_steps == 0)
        return;
    for (uint8_t i = 0; i < seq.num_steps; ++i)
        if (((uint32_t)i * (uint32_t)seq.beats_per_bar) %
                (uint32_t)seq.num_steps ==
            0)
            seq.steps[i].active = true;
}

char *int_to_char(int32_t value) {
    // buffer estático para almacenar el resultado (-2147483648 = 11 chars +
    // '\0')
    static char str_buf[12];

    // conversión usando itoa, base 10 (decimal)
    itoa(value, str_buf, 10);

    // devolver puntero al buffer resultante
    return str_buf;
}

// --- Event trigger callback ---
void SEQ_trigger(uint8_t note) {
    ft_print(" ");
    if (seq.current_step == 0) {
        ft_print("\n");
    }
    ft_print(int_to_char(seq.current_step));
}
// Function to set callback for a given step index
void SEQ_set_step_callback(int step_index, void (*callback)(void)) {
    if (step_index < 0) return;
    seq.steps[step_index].on_trigger = callback;
}