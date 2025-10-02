/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

                                License

                   GNU AFFERO GENERAL PUBLIC LICENSE
                      Version 3, 19 November 2007

                           AGPL-3.0-or-later

 Freetribe is free software: you can redistribute it and/or modify it
under the terms of the GNU Affero General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
                  (at your option) any later version.

     Freetribe is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty
        of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
          See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2024

----------------------------------------------------------------------*/

/**
 * @file    monosynth.c
 *
 * @brief   Monophonic synth example.
 */

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>

#include "freetribe.h"

#include "keyboard.h"

#include "leaf-config.h"
#include "param_scale.h"
#include "svc_panel.h"

#include "gui_task.h"

#include "leaf.h"

#include "module_interface.h"
#include "panel_buttons.h"
#include "sysex_manager.h"

#include "common/sample.h"

#include "globals.h"
#include "panel_callbacks.h"

/*----- Macros -------------------------------------------------------*/

#define CONTROL_RATE (1000)
#define MEMPOOL_SIZE (0x1000)

#define DEFAULT_SCALE_NOTES NOTES_IONIAN // MAJOR SCALE
#define DEFAULT_SCALE_TONES 12
#define DEFAULT_SCALE_MODE 0

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/



#define MULTIPLYER 4096
static int32_t tet_decimal_values[12] = {
    1.000000 * MULTIPLYER, 1.059463 * MULTIPLYER, 1.122462 * MULTIPLYER,
    1.189207 * MULTIPLYER, 1.259921 * MULTIPLYER, 1.334840 * MULTIPLYER,
    1.414214 * MULTIPLYER, 1.498307 * MULTIPLYER, 1.587401 * MULTIPLYER,
    1.681793 * MULTIPLYER, 1.781797 * MULTIPLYER, 1.887749 * MULTIPLYER};

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _tick_callback(void);
static void _trigger_callback_2(uint8_t pad, uint8_t vel, bool state);
static void _note_on_callback(char chan, char note, char vel);
static void _note_off_callback(char chan, char note, char vel);
static void process_note_event(uint8_t note, uint8_t vel, bool state);

static void _lut_init(void);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Initialise application.
 *
 * @return status   Status code indicating success:
 *                  - SUCCESS
 *                  - WARNING
 *                  - ERROR
 */
t_status app_init(void) {
    voice_manager_init();
    t_status status = ERROR;

    LEAF_init(&g_leaf, CONTROL_RATE, g_mempool, MEMPOOL_SIZE, NULL);

    module_init(&g_leaf);
    //_set_filter_type(FILTER_TYPE_LPF);

    _lut_init();

    scale_init(&g_scale, DEFAULT_SCALE_NOTES, DEFAULT_SCALE_TONES);
    keyboard_init(&g_kbd, &g_scale);
    g_kbd.octave = 4;

    ft_register_panel_callback(KNOB_EVENT, knob_callback);
    ft_register_panel_callback(ENCODER_EVENT, encoder_callback);
    ft_register_panel_callback(BUTTON_EVENT, button_callback);
    ft_register_panel_callback(TRIGGER_EVENT, _trigger_callback_2);
    ft_register_midi_callback(EVT_CHAN_NOTE_ON, _note_on_callback);
    ft_register_midi_callback(EVT_CHAN_NOTE_OFF, _note_off_callback);
    midi_register_sysex_handler(_sysex_callback);

    ft_register_tick_callback(0, _tick_callback);

    // Initialise GUI.
    gui_task();

    ft_print("waves Example");
    gui_print(4, 7, "waves Example");

    g_current_editing_sample_parameter = 0;
    g_current_editing_sample_parameter_value = 0;

    status = SUCCESS;
    return status;
}

/**
 * @brief   Run application.
 */
void app_run(void) { gui_task(); }

/*----- Static function implementations ------------------------------*/

static void _tick_callback(void) {
    //
    module_process();
}


static void process_note_event(uint8_t note, uint8_t vel, bool state) {
    uint8_t voice_idx;
    if (state) {
        /* Note ON event */

        /* Check if note is already active (prevent retriggering) */
        voice_idx = voice_manager_get_voice_by_note(note);
        if (voice_idx != INVALID_VOICE) {
            // return;
            // Note already active, just retrigger envelope
            module_set_param_voice(voice_idx, PARAM_VEL, 1.0);
            module_set_param_voice(voice_idx, PARAM_GATE, true);
            module_set_param_voice(voice_idx, PARAM_PHASE_RESET, true);
            voice_manager_set_voice_in_release_stage(voice_idx, false);
            return;
        }

        /* Find available voice */
        voice_idx = voice_manager_get_free_voice();

        /* If no free voices, use voice stealing */
        if (voice_idx == INVALID_VOICE) {
            ft_print("no free vices");
            return;
            voice_idx = voice_manager_get_oldest_voice();

            /* Turn off gate for stolen voice */
            // module_set_param_voice(voice_idx, PARAM_GATE, false);
            voice_manager_set_voice_in_release_stage(voice_idx, false);
        }

        /* Assign new note to selected voice */
        voice_manager_assign_note(voice_idx, note);

        /* Configure voice parameters */
        module_set_param_voice(voice_idx, PARAM_VEL, 1.0);
        module_set_param_voice(voice_idx, PARAM_GATE, true);
        module_set_param_voice(voice_idx, PARAM_OSC_BASE_FREQ,
                               g_midi_pitch_cv_lut[note]);
        module_set_param_voice(voice_idx, PARAM_FREQ,
                               g_midi_pitch_cv_lut[note]);

        int note_index = note % 12;
        int sample_root_note = 62; // the note in which the sample was sampled
        int scale_offset = sample_root_note % 12;
        module_set_param_voice(voice_idx, PARAM_PHASE_RESET,
                               true); // sets phase to zero

        if (scale_offset <= note_index) {
            note_index -= scale_offset;
            // this method allows only addressing 23 seconds of samples becasue
            // we are wasting 12 bits of 32 so max table index would be 1048576
            // (2**20) / (48000 sample rate)
            module_set_param_voice(voice_idx, PARAM_PLAYBACK_RATE,
                                   tet_decimal_values[note_index]);
        } else {
            note_index = note_index + 12 - scale_offset;
            module_set_param_voice(voice_idx, PARAM_PLAYBACK_RATE,
                                   tet_decimal_values[note_index] >>
                                       1); // lower octave
        }

        gui_post_param("note ", note);

    } else {
        /* Note OFF event */

        /* Find and release the voice assigned to this note */
        voice_idx = voice_manager_get_voice_by_note(note);
        if (voice_idx != INVALID_VOICE) {

            bool in_release_stage =
                voice_manager_is_voice_in_release_stage(voice_idx);
            if (in_release_stage) {
                /* Voice is already in release stage, do nothing */
                ft_print("Voice already in release stage");
                return;
            }
            /* Turn off gate (start release phase) */

            module_set_param_voice(voice_idx, PARAM_GATE, false);
            voice_manager_set_voice_in_release_stage(voice_idx, true);

            /* Release note allocation */
            // voice_manager_release_voice(voice_idx);
            // voice_manager_release_note(note);
        }
    }
}

/**
 * @brief Polyphonic trigger callback function
 *
 * Handles note on/off events with polyphonic voice allocation.
 * Supports up to MAX_VOICES simultaneous notes with voice stealing
 * when all voices are in use.
 *
 * @param pad Input pad/key identifier
 * @param vel Velocity value (0-127)
 * @param state Note state (true = note on, false = note off)
 */
static void _trigger_callback_2(uint8_t pad, uint8_t vel, bool state) {
    uint8_t note = keyboard_map_note(&g_kbd, pad);

    process_note_event(note, vel, state);
}

/**
 * @brief Initialize the polyphonic system
 *
 * Call this function during system initialization to set up
 * the voice management system.
 */
void polyphonic_system_init(void) {
    voice_manager_init();
    ft_print("Polyphonic voice manager initialized (");

    char count_str[4];
    itoa(MAX_VOICES, count_str, 10);
    ft_print(count_str);
    ft_print(" voices)\n");
}







void print_param(char value) {
    char val_string[4];
    itoa(value, val_string, 10);
    ft_print(val_string);
}

/**
 * @brief   Callback triggered by MIDI note on events.
 *
 * Echo received note on messages to MIDI output.
 *
 * @param[in]   chan    MIDI channel.
 * @param[in]   note    MIDI note number.
 * @param[in]   vel     MIDI note velocity.
 */
static void _note_on_callback(char chan, char note, char vel) {
    uint8_t note_int = (uint8_t)note;
    uint8_t vel_int = (uint8_t)vel;
    process_note_event(note_int, vel_int, true);
}

/**
 * @brief   Callback triggered by MIDI note off events.
 *
 * Echo received note on messages to MIDI output.
 *
 * @param[in]   chan    MIDI channel.
 * @param[in]   note    MIDI note number.
 * @param[in]   vel     MIDI note velocity.
 */
static void _note_off_callback(char chan, char note, char vel) {
    uint8_t note_int = (uint8_t)note;
    uint8_t vel_int = (uint8_t)vel;
    process_note_event(note_int, vel_int, false);
}

static void _lut_init(void) {

    int i;
    float scaled;

    for (i = 0; i <= 127; i++) {

        g_midi_pitch_cv_lut[i] = note_to_cv(i);
    }

    for (i = 0; i <= 255; i++) {

        scaled = i / 255.0;
        g_amp_cv_lut[i] = powf(scaled, 2);
    }

    for (i = 0; i <= 255; i++) {
        g_knob_cv_lut[i] = i / 255.0;
    }

    /// TODO: Should be log.
    //
    // Initialise pitch mod lookup table.
    float tune;
    for (i = 0; i <= 255; i++) {

        if (i <= 128) {
            // 0.5...1.
            tune = (i / 256.0) + 0.5;

        } else {
            // >1...2.0
            tune = ((i - 128) / 127.0) + 1;
        }

        // // Convert to fix16,
        // tune *= (1 << 16);
        // g_octave_tune_lut[i] = (int32_t)tune;

        g_octave_tune_lut[i] = tune;
    }

    int32_t res;
    for (i = 0; i <= 255; i++) {

        res = 0x7fffffff - (i * (1 << 23));

        g_filter_res_lut[i] = res;
    }
}

/*----- End of file --------------------------------------------------*/
