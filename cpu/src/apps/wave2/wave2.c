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
#include "common/parameters.h"
#include "panel_buttons.h"

#include "common/sample.h"



/*----- Macros -------------------------------------------------------*/

#define CONTROL_RATE (1000)
#define MEMPOOL_SIZE (0x1000)



#define DEFAULT_SCALE_NOTES NOTES_PHRYGIAN_DOMINANT
#define DEFAULT_SCALE_TONES 12
#define DEFAULT_SCALE_MODE 0

#define PROFILE_INTERVAL 100

#define MULTIPLYER 4096
static int32_t tet_decimal_values[12] = {
    1.000000 * MULTIPLYER, 1.059463 * MULTIPLYER, 1.122462 * MULTIPLYER,
    1.189207 * MULTIPLYER, 1.259921 * MULTIPLYER, 1.334840 * MULTIPLYER,
    1.414214 * MULTIPLYER, 1.498307 * MULTIPLYER, 1.587401 * MULTIPLYER,
    1.681793 * MULTIPLYER, 1.781797 * MULTIPLYER, 1.887749 * MULTIPLYER};

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/
static int g_current_editing_sample_parameter;
static int g_current_editing_sample_parameter_value[SAMPLE_PARAM_COUNT];

static bool g_shift_held;
static bool g_menu_held;

static bool g_button_bar_1_held;
static bool g_button_bar_2_held;
static bool g_button_bar_3_held;
static bool g_button_bar_4_held;


static LEAF g_leaf;
static char g_mempool[MEMPOOL_SIZE];

static t_keyboard g_kbd;
static t_scale g_scale;

static e_mod_type g_mod_type;

static bool g_shift_held;
static bool g_menu_held;
static bool g_amp_eg;
static bool g_retrigger;

static float g_midi_pitch_cv_lut[128];
static float g_amp_cv_lut[256];
static float g_knob_cv_lut[256];

static float g_midi_hz_lut[128];
static float g_octave_tune_lut[256];
static float g_filter_res_lut[256];

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _tick_callback(void);
static void _knob_callback(uint8_t index, uint8_t value);
static void _encoder_callback(uint8_t index, uint8_t value);
static void _button_callback(uint8_t index, bool state);
static void _trigger_callback_2(uint8_t pad, uint8_t vel, bool state);
static void _note_on_callback(char chan, char note, char vel);
static void _note_off_callback(char chan, char note, char vel);
static void process_note_event(uint8_t note, uint8_t vel, bool state);

static void _set_filter_type(uint8_t filter_type);
static void _set_mod_depth(uint32_t mod_depth);
static void _set_mod_speed(uint32_t mod_speed);

static void _lut_init(void);

static void _profile_callback(uint32_t period, uint32_t cycles);

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
    _set_filter_type(FILTER_TYPE_LPF);

    _lut_init();

    scale_init(&g_scale, DEFAULT_SCALE_NOTES, DEFAULT_SCALE_TONES);
    keyboard_init(&g_kbd, &g_scale);
    g_kbd.octave = 4;

    ft_register_panel_callback(KNOB_EVENT, _knob_callback);
    ft_register_panel_callback(ENCODER_EVENT, _encoder_callback);
    ft_register_panel_callback(BUTTON_EVENT, _button_callback);
    ft_register_panel_callback(TRIGGER_EVENT, _trigger_callback_2);
    ft_register_midi_callback(EVT_CHAN_NOTE_ON, _note_on_callback);
    ft_register_midi_callback(EVT_CHAN_NOTE_OFF, _note_off_callback);

    ft_register_tick_callback(0, _tick_callback);

    ft_register_dsp_callback(MSG_TYPE_SYSTEM, SYSTEM_PROFILE,
                             _profile_callback);

    // Initialise GUI.
    gui_task();

    gui_print(4, 7, "M a k e W a v e s");

    status = SUCCESS;
    return status;
}

/**
 * @brief   Run application.
 */
void app_run(void) { gui_task(); }

/*----- Static function implementations ------------------------------*/
static void _print_gui_param(int param, int value) {
    switch (param) {
    case SAMPLE_START_POINT:
        gui_post_param("Start ", value);
        break;
    case SAMPLE_PLAYBACK_RATE:
        gui_post_param("Rate  ", value);
        break;
    case SAMPLE_PARAM_QUALITY:
        gui_post_param("Qual  ", value);
        break;
    case SAMPLE_LOOP_POINT:
        gui_post_param("Loop  ", value);
        break;
    default:
        break;
    }
}

static void _tick_callback(void
) {

    static uint32_t tick_count;

    module_process();

    if (tick_count++ >= PROFILE_INTERVAL) {

        svc_dsp_get_profile();

        tick_count = 0;
    }
}

static void _profile_callback(uint32_t period, uint32_t cycles) {
gui_print(1, 55, "buf");
    uint32_t percent;

    char buf[4] = {0};

    // Clear display.
    memset(buf, 0x20, sizeof(buf) - 1);
    gui_print(1, 55, buf);

    percent = (uint32_t)(((float)cycles / (float)period) * 100.0);

    if (percent < 1000) {

        itoa(percent, buf, 10);

    } else {
        strncpy(buf, "ERR", sizeof(buf));
    }

    gui_print(1, 55, buf);
}

/**
 * @brief   Callback triggered by panel knob events.
 *
 * @param[in]   index   Index of knob.
 * @param[in]   value   Value of knob.
 */
static void _knob_callback(uint8_t index, uint8_t value) {

    switch (index) {

    case KNOB_PITCH:
        //  if osc type is unison attenuate unison detune 
        if (module_get_param(PARAM_UNISON) == 1) {
            module_set_param_all_voices(PARAM_TUNE, 1 - (value * 0.02f / 255.0f));
            gui_post_param("U. Detune: ", value);
        } else {
            module_set_param_all_voices(PARAM_TUNE, g_octave_tune_lut[value]); 
            //module_set_param_all_voices(PARAM_TUNE, 1 - (value * 1.5f / 255.0f));  // sync testing
            gui_post_param("Pitch: ", value);

        }

        break;

    case KNOB_ATTACK:
        if (g_shift_held) {

            if (g_amp_eg) {
                module_set_param_all_voices(PARAM_AMP_ENV_SUSTAIN,
                                            g_knob_cv_lut[value]);
                gui_post_param("Amp Sus: ", value);

            } else {
                module_set_param_all_voices(PARAM_FILTER_ENV_SUSTAIN,
                                            g_knob_cv_lut[value]);

                gui_post_param("Fil Sus: ", value);
            }

        } else {
            if (g_amp_eg) {
                module_set_param_all_voices(PARAM_AMP_ENV_ATTACK,
                                            g_knob_cv_lut[value]);
                gui_post_param("Amp Atk: ", value);

            } else {
                module_set_param_all_voices(PARAM_FILTER_ENV_ATTACK,
                                            g_knob_cv_lut[value]);
                gui_post_param("Fil Atk: ", value);
            }
        }
        break;

    case KNOB_DECAY:
        if (g_shift_held) {

            if (g_amp_eg) {
                module_set_param_all_voices(PARAM_AMP_ENV_RELEASE,
                                            g_knob_cv_lut[value]);
                gui_post_param("Amp Rel: ", value);

            } else {
                module_set_param_all_voices(PARAM_FILTER_ENV_RELEASE,
                                            g_knob_cv_lut[value]);

                gui_post_param("Fil Rel: ", value);
            }

        } else {
            if (g_amp_eg) {
                module_set_param_all_voices(PARAM_AMP_ENV_DECAY,
                                            g_knob_cv_lut[value]);
                gui_post_param("Amp Dec: ", value);

            } else {
                module_set_param_all_voices(PARAM_FILTER_ENV_DECAY,
                                            g_knob_cv_lut[value]);
                gui_post_param("Fil Dec: ", value);
            }
        }
        break;

    case KNOB_LEVEL:
        if (g_shift_held) {
            module_set_param_all_voices(PARAM_AMP_2_LEVEL, g_amp_cv_lut[value]);
            gui_post_param("Amp2 Level: ", value);
            
        } else {
            module_set_param_all_voices(PARAM_AMP_LEVEL, g_amp_cv_lut[value]);
            gui_post_param("Amp Level: ", value);
            
        }
        break;

    case KNOB_RESONANCE:
        module_set_param_all_voices(PARAM_RES, g_knob_cv_lut[value]);
        gui_post_param("Resonance: ", value);
        break;

    case KNOB_EG_INT:
        module_set_param_all_voices(PARAM_FILTER_ENV_DEPTH,
                                    g_knob_cv_lut[value]);
        gui_post_param("EG Depth: ", value);
        break;

    case KNOB_MOD_DEPTH:
        _set_mod_depth(value);
        break;

    case KNOB_MOD_SPEED:
        _set_mod_speed(value);
        break;

    default:
        break;
    }
}

/**
 * @brief   Callback triggered by panel encoder events.
 *
 * @param[in]   index   Index of encoder.
 * @param[in]   value   Value of encoder.
 */
static void _encoder_callback(uint8_t index, uint8_t value) {

    static uint8_t cutoff = DEFAULT_CUTOFF;
    static int8_t osc_type = DEFAULT_OSC_TYPE;
    static int8_t mod_type;

    switch (index) {

    case ENCODER_CUTOFF:

        if (value == 0x01) {

            if (cutoff < 0x7f) {
                cutoff++;
            }

        } else {
            if (cutoff > 0) {
                cutoff--;
            }
        }
        module_set_param_all_voices(PARAM_FILTER_BASE_CUTOFF,
                                    g_midi_pitch_cv_lut[cutoff]);
        gui_post_param("Cutoff: ", cutoff);

        break;

    case ENCODER_OSC:

        if (value == 0x01) {
            osc_type++;
            if (osc_type > OSC_TYPE_MAX) {
                osc_type = 0;
            }
        } else {
            osc_type--;
            if (osc_type < 0) {
                osc_type = OSC_TYPE_MAX;
            }
        }
        if (g_shift_held) {
            module_set_param_all_voices(PARAM_OSC_2_TYPE, (1.0 / OSC_TYPE_COUNT) * osc_type);
            gui_show_osc_type(2, osc_type);
        } else {
            module_set_param_all_voices(PARAM_OSC_TYPE, (1.0 / OSC_TYPE_COUNT) * osc_type);
            gui_show_osc_type(1, osc_type);
        }

        break;

    case ENCODER_MOD:

        if (value == 0x01) {
            mod_type++;
            if (mod_type > MOD_TYPE_MAX) {
                mod_type = 0;
            }
        } else {
            mod_type--;
            if (mod_type < 0) {
                mod_type = MOD_TYPE_MAX;
            }
        }

        g_mod_type = mod_type;
        gui_show_mod_type(mod_type);

        break;

        case ENCODER_MAIN:
        int amt = 1;
        if (g_button_bar_1_held) {
            amt = 10;
        }
        if (g_button_bar_2_held) {
            amt = 100;
        }
        if (g_button_bar_3_held) {
            amt = 1000;
        }
        if (g_button_bar_4_held) {
            amt = 10000;
        }

        if (value == 0x01) {
            g_current_editing_sample_parameter_value[g_current_editing_sample_parameter] += amt;
            /*if (g_current_editing_sample_parameter_value > 4096) { // magic
            number g_current_editing_sample_parameter_value = 4096;
            }*/
        } else {
            g_current_editing_sample_parameter_value[g_current_editing_sample_parameter] -= amt;
            if (g_current_editing_sample_parameter_value[g_current_editing_sample_parameter] < 0) {
                g_current_editing_sample_parameter_value[g_current_editing_sample_parameter] = 0;
            }
        }
        int sample_number = 0; // only one sample for now

        _print_gui_param(g_current_editing_sample_parameter,g_current_editing_sample_parameter_value[g_current_editing_sample_parameter]);
        module_set_param_sample(sample_number,
                                g_current_editing_sample_parameter,
                                g_current_editing_sample_parameter_value[g_current_editing_sample_parameter]);
        // only voice 0 for now
        break;

    default:
        break;
    }
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
            // ft_print("no free vices");
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
        module_set_param_voice(voice_idx, PARAM_PHASE_RESET, true);

        int note_index = note % 12;
        int sample_root_note = 62; // the note in which the sample was sampled
        int scale_offset = sample_root_note % 12;

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
                //ft_print("Voice already in release stage");
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
 * @brief   Callback triggered by panel button events.
 *
 * @param[in]   index   Index of button.
 * @param[in]   state   State of button.
 */
static void _button_callback(uint8_t index, bool state) {
    static bool ifx_on = false;

    switch (index) {
    case BUTTON_RECORD:
        // donesnt matter wich voice, send voice 0
        ft_set_module_param(0, SAMPLE_RECORD_START, 1);
        //loaded_samples = 0;
        gui_post_param("recording ", 1);
        g_menu_held = state;
        break;

    case BUTTON_STOP:
        // donesnt matter wich voice, send voice 0
        ft_set_module_param(0, SAMPLE_RECORD_STOP, 1);
        gui_post_param("recording ", 0);
        g_menu_held = state;
        break;

    case BUTTON_IFX_ON:
        if (state == 1) {
            ifx_on = !ifx_on;
            ft_set_led(LED_IFX, ifx_on);
            ft_set_module_param(0, PARAM_IFX, ifx_on);
        }
        break;
        case BUTTON_EXIT:
        if (state == 1) {
            ft_shutdown();
        }
        break;
    case BUTTON_MENU:
        g_menu_held = state;
        break;

    case BUTTON_SHIFT:
        g_shift_held = state;
        break;

    case BUTTON_AMP_EG:
        if (state) {

            if (g_shift_held) {

                g_retrigger = !g_retrigger;
                module_set_param_all_voices(PARAM_RETRIGGER, g_retrigger);
                gui_post_param("Env Retrig: ", g_retrigger);

            } else {
                g_amp_eg = !g_amp_eg;
                ft_set_led(LED_AMP_EG, g_amp_eg);
            }
        }
        break;

    case BUTTON_LPF:
        if (state) {
            _set_filter_type(FILTER_TYPE_LPF);
        }
        break;

    case BUTTON_BPF:
        if (state) {
            if (g_shift_held){
                _set_filter_type(FILTER_TYPE_NOTCH);
            } else {
                _set_filter_type(FILTER_TYPE_BPF);
            }
        }
        break;

    case BUTTON_HPF:
        if (state) {
            _set_filter_type(FILTER_TYPE_HPF);
        }
        break;
    case BUTTON_FORWARD:
        if (state) {
            if (g_current_editing_sample_parameter < SAMPLE_PARAM_COUNT - 1) {
                g_current_editing_sample_parameter++;
            }
            // gui_post_param("SampPrm  :
            // ",g_current_editing_sample_parameter_value[g_current_editing_sample_parameter]);
            _print_gui_param(g_current_editing_sample_parameter,g_current_editing_sample_parameter_value[g_current_editing_sample_parameter]);
        }
        break;
    case BUTTON_BACK:
        if (state) {
            if (g_current_editing_sample_parameter > 0) {
                g_current_editing_sample_parameter--;
            }

            // gui_post_param("SampPrm  : ",
            // g_current_editing_sample_parameter_value
            // [g_current_editing_sample_parameter]);
            _print_gui_param(g_current_editing_sample_parameter,
                             g_current_editing_sample_parameter_value
                                 [g_current_editing_sample_parameter]);
        }
        break;

    default:
        break;
    }
}

static void _set_filter_type(uint8_t filter_type) {

    static uint8_t filter_variation = 0;
    filter_variation++;
    if (filter_variation >1){
        filter_variation = 0;
    }
    uint8_t ft = filter_type + ( FILTER_TYPE_COUNT * filter_variation);
    //uint8_t ft = filter_type ;
    module_set_param_all_voices(PARAM_FILTER_TYPE, (1.0 / FILTER_TYPE_COUNT) * ft);

    switch (filter_type) {

    case FILTER_TYPE_LPF:
        ft_set_led(LED_LPF, 1);
        ft_set_led(LED_BPF, 0);
        ft_set_led(LED_HPF, 0);
        break;

    case FILTER_TYPE_BPF:
    case FILTER_TYPE_NOTCH:
        ft_set_led(LED_LPF, 0);
        ft_set_led(LED_BPF, 1);
        ft_set_led(LED_HPF, 0);
        break;

    case FILTER_TYPE_HPF:
        ft_set_led(LED_LPF, 0);
        ft_set_led(LED_BPF, 0);
        ft_set_led(LED_HPF, 1);

        break;

    default:
        break;
    }
    gui_post_param("Fil Type: ", ft);
}

static void _set_mod_depth(uint32_t mod_depth) {

    switch (g_mod_type) {

    case MOD_AMP_LFO:
        module_set_param_all_voices(PARAM_AMP_LFO_DEPTH,
                                    g_knob_cv_lut[mod_depth]);
        gui_post_param("A.LFO Dpt: ", mod_depth);
        break;

    case MOD_FILTER_LFO:
        module_set_param_all_voices(PARAM_FILTER_LFO_DEPTH,
                                    g_knob_cv_lut[mod_depth]);
        gui_post_param("F.LFO Dpt: ", mod_depth);
        break;

    case MOD_PITCH_LFO:
        module_set_param_all_voices(
            PARAM_PITCH_LFO_DEPTH,
            g_knob_cv_lut[mod_depth] /
                8); // Divide by 8 to reduce pitch modulation range
        gui_post_param("P.LFO Dpt: ", mod_depth);
        break;

    default:
        break;
    }
}

static void _set_mod_speed(uint32_t mod_speed) {

    switch (g_mod_type) {

    case MOD_AMP_LFO:
        module_set_param_all_voices(PARAM_AMP_LFO_SPEED,
                                    g_knob_cv_lut[mod_speed]);
        gui_post_param("A.LFO Spd: ", mod_speed);
        break;

    case MOD_FILTER_LFO:
        module_set_param_all_voices(PARAM_FILTER_LFO_SPEED,
                                    g_knob_cv_lut[mod_speed]);
        gui_post_param("F.LFO Spd: ", mod_speed);
        break;

    case MOD_PITCH_LFO:
        module_set_param_all_voices(PARAM_PITCH_LFO_SPEED,
                                    g_knob_cv_lut[mod_speed]);
        gui_post_param("P.LFO Spd: ", mod_speed);
        break;

    default:
        break;
    }
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
