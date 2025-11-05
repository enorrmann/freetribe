#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <stdint.h>
#include <stdbool.h>
#include "freetribe.h"

#define MAX_STEPS 64

// --- Sequencer modes ---
typedef enum {
    SEQ_MODE_IMMEDIATE = 0,   // Apply parameter changes instantly
    SEQ_MODE_SYNCED = 1       // Apply changes only at next cycle
} SEQ_UpdateMode;

// --- Sequencer states ---
typedef enum {
    SEQ_STATE_STOPPED = 0,
    SEQ_STATE_RUNNING,
    SEQ_STATE_PAUSED
} SEQ_State;

// --- Step definition ---
typedef struct {
    bool active;      // Whether this step should trigger
    uint8_t note;     // MIDI note number or arbitrary identifier
    void (*on_trigger)(void); // Callback when step is triggered
} Step;

// --- Main sequencer structure ---
typedef struct {
    Step steps[MAX_STEPS];
    uint8_t num_steps;
    uint8_t beats_per_bar;  
    uint8_t beat_unit;      
    uint8_t step_unit;      // Step resolution (4=quarter, 8=eighth, 16=sixteenth)
    float bpm;
    uint8_t current_step;
    float counter_ms;

    // Pending values for synced update mode
    float pending_bpm;
    uint8_t pending_beats_per_bar;
    uint8_t pending_beat_unit;
    uint8_t pending_step_unit;
    uint8_t pending_num_steps;

    SEQ_UpdateMode update_mode;
    SEQ_State state;
} Sequencer;

// --- Global instance ---
extern Sequencer seq;

// --- Core control API ---
void SEQ_init(float bpm, uint8_t beats_per_bar, uint8_t beat_unit, uint8_t step_unit, uint8_t steps);
void SEQ_set_params(float bpm, uint8_t beats_per_bar, uint8_t beat_unit, uint8_t step_unit, uint8_t steps);
void SEQ_start(void);
void SEQ_pause(void);
void SEQ_continue(void);
void SEQ_stop(void);
void SEQ_tick(void);
void SEQ_fill_every_n(uint8_t n) ;
// --- Internal helpers ---
void SEQ_trigger(uint8_t note);
void SEQ_set_step_callback(int step_index, void (*callback)(void));


#endif // SEQUENCER_H
