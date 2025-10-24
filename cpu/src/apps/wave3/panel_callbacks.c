#include "panel_callbacks.h"

static void _set_mod_speed(uint32_t mod_speed);
static void _print_gui_param(int param, int value);

static int g_current_editing_sample_parameter = 0;
static int g_current_editing_sample_parameter_value[SAMPLE_PARAM_COUNT];

static bool g_shift_held;
static bool g_menu_held;

static bool g_button_bar_1_held;
static bool g_button_bar_2_held;
static bool g_button_bar_3_held;
static bool g_button_bar_4_held;

static e_mod_type g_mod_type;

static bool g_shift_held;
static bool g_menu_held;
static bool g_amp_eg;
static bool g_retrigger;

static int g_knob_multiplier = 1;
static int g_recording_time_in_ms = 1000;

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
    case SAMPLE_PARAM_ROOT_NOTE:
        gui_post_param("Root  ", value);
        break;
    case SAMPLE_PARAM_LOW_NOTE:
        gui_post_param("Low   ", value);
        break;
    case SAMPLE_PARAM_HI_NOTE:
        gui_post_param("High  ", value);
        break;
    case SAMPLE_SELECTED_SAMPLE:
        gui_post_param("SelSam", value);
        break;
    case SAMPLE_GLOBAL_OFFSET:
        gui_post_param("GlOfst", value);
        break;
    case SAMPLE_RECORDING_THRESHOLD:
        gui_post_param("Threshl", value);
        break;
    case SAMPLE_RECORDING_TIME_IN_MS:
        gui_post_param("RecTimeMs ", value);
        break;
    case SAMPLE_LENGTH:
        gui_post_param("Length ", value);
        break;

    default:
        break;
    }
}

static void _set_filter_type(uint8_t filter_type) {

    static uint8_t filter_variation = 0;
    filter_variation++;
    if (filter_variation > 1) {
        filter_variation = 0;
    }
    uint8_t ft = filter_type + (FILTER_TYPE_COUNT * filter_variation);
    // uint8_t ft = filter_type ;
    module_set_param_all_voices(PARAM_FILTER_TYPE,
                                (1.0 / FILTER_TYPE_COUNT) * ft);

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

    case MOD_MORPH_LFO:
        module_set_param_all_voices(PARAM_LFO_1_DEPTH, g_knob_cv_lut[mod_depth]);
        gui_post_param("LFO_1 Dpt: ", mod_depth);
        break;

        case MOD_AMP_LFO:
        module_set_param_all_voices(PARAM_AMP_LFO_DEPTH, g_knob_cv_lut[mod_depth]);
        gui_post_param("A.LFO Dpt: ", mod_depth);
        break;

    case MOD_FILTER_LFO:
        module_set_param_all_voices(PARAM_FILTER_LFO_DEPTH, g_knob_cv_lut[mod_depth]);
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

    case MOD_MORPH_LFO:
        //module_set_param_all_voices(PARAM_LFO_1_SPEED,mod_speed); // test modulation lfo
        module_set_param_all_voices(PARAM_LFO_1_SPEED,g_knob_cv_lut[mod_speed]);
        gui_post_param("LFO_1 Spd: ", mod_speed);
        break;
    case MOD_AMP_LFO:
        module_set_param_all_voices(PARAM_AMP_LFO_SPEED,g_knob_cv_lut[mod_speed]);
        gui_post_param("A.LFO Spd: ", mod_speed);
        break;

    case MOD_FILTER_LFO:
        module_set_param_all_voices(PARAM_FILTER_LFO_SPEED,g_knob_cv_lut[mod_speed]);
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
 * @brief   Callback triggered by panel button events.
 *
 * @param[in]   index   Index of button.
 * @param[in]   state   State of button.
 */
void PANEL_button_callback(uint8_t index, bool state) {
    static bool ifx_on = false;

    switch (index) {
    case BUTTON_RECORD:

        if (state) {
            // donesnt matter wich voice, send voice 0
            ft_set_module_param(0, SAMPLE_RECORD_START,
                                1); // moved to autosampler
            // loaded_samples = 0;
            gui_post_param("recording ", 1);

            int record_time_in_ms = g_recording_time_in_ms; // 1 second each
            int recorded_samples = ((SAMPLING_END_NOTE - SAMPLING_START_NOTE) /
                                    SAMPLING_STEP_IN_SEMITONES) +
                                   1;

            AUTOSAMPLER_init(SAMPLING_START_NOTE, SAMPLING_END_NOTE,
                             SAMPLING_STEP_IN_SEMITONES, record_time_in_ms);

            int sample_lenght_in_samples =
                (SAMPLE_RATE * record_time_in_ms) / 1000;

            // config_samples here
            int i;
            for (i = 0; i < recorded_samples; i++) {
                /*g_samples[i]->root_note = SAMPLING_START_NOTE + (i *
                SAMPLING_STEP_IN_SEMITONES); g_samples[i]->low_note =
                g_samples[i]->root_note - 1; g_samples[i]->hi_note =
                g_samples[i]->root_note + 1;*/

                int start_pos_in_samples = (sample_lenght_in_samples * i);
                module_set_param_sample(i, SAMPLE_START_POINT,
                                        start_pos_in_samples);
                module_set_param_sample(i, SAMPLE_LENGTH,
                                        sample_lenght_in_samples);
            }
            gui_post_param("Sampling  ", recorded_samples);
        }

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
            if (g_shift_held) {
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
            _print_gui_param(g_current_editing_sample_parameter,
                             g_current_editing_sample_parameter_value
                                 [g_current_editing_sample_parameter]);
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
    case BUTTON_BAR_1:
        g_knob_multiplier = 10;
        g_button_bar_1_held = state;
        break;
    case BUTTON_BAR_2:
        g_knob_multiplier = 100;
        g_button_bar_2_held = state;
        break;
    case BUTTON_BAR_3:
        g_knob_multiplier = 1000;
        g_button_bar_3_held = state;
        break;
    case BUTTON_BAR_4:
        g_knob_multiplier = 10000;
        g_button_bar_4_held = state;
        break;
    case BUTTON_WRITE:
        ft_get_module_param(0, 1);
        break;
    default:
        break;
    }
}

/**
 * @brief   Callback triggered by panel knob events.
 *
 * @param[in]   index   Index of knob.
 * @param[in]   value   Value of knob.
 */
void PANEL_knob_callback(uint8_t index, uint8_t value) {

    switch (index) {

    case KNOB_PITCH:
        //  if osc type is unison attenuate unison detune
        if (module_get_param(PARAM_UNISON) == 1) {
            module_set_param_all_voices(PARAM_TUNE,
                                        1 - (value * 0.02f / 255.0f));
            gui_post_param("U. Detune: ", value);
        } else {
            module_set_param_all_voices(PARAM_TUNE, g_octave_tune_lut[value]);
            // module_set_param_all_voices(PARAM_TUNE, 1 - (value * 1.5f /
            // 255.0f));  // sync testing
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
void PANEL_encoder_callback(uint8_t index, uint8_t value) {

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
        module_set_param_all_voices(PARAM_FILTER_BASE_CUTOFF, g_midi_pitch_cv_lut[cutoff]);
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
            module_set_param_all_voices(PARAM_OSC_2_TYPE,
                                        (1.0 / OSC_TYPE_COUNT) * osc_type);
            gui_show_osc_type(2, osc_type);
        } else {
            module_set_param_all_voices(PARAM_OSC_TYPE,
                                        (1.0 / OSC_TYPE_COUNT) * osc_type);
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
        int amt = g_knob_multiplier;
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
            g_current_editing_sample_parameter_value
                [g_current_editing_sample_parameter] += amt;
            /*if (g_current_editing_sample_parameter_value > 4096) { // magic
            number g_current_editing_sample_parameter_value = 4096;
            }*/
        } else {
            g_current_editing_sample_parameter_value
                [g_current_editing_sample_parameter] -= amt;
            if (g_current_editing_sample_parameter_value
                    [g_current_editing_sample_parameter] < 0) {
                g_current_editing_sample_parameter_value
                    [g_current_editing_sample_parameter] = 0;
            }
        }

        int sample_number = 0; // only one sample for now
        sample_number =
            g_current_editing_sample_parameter_value[SAMPLE_SELECTED_SAMPLE];

        /*if (g_current_editing_sample_parameter == SAMPLE_PARAM_ROOT_NOTE) {
            g_samples[sample_number]->root_note =
                g_current_editing_sample_parameter_value
                    [g_current_editing_sample_parameter];
        }
        if (g_current_editing_sample_parameter == SAMPLE_PARAM_LOW_NOTE) {
            g_samples[sample_number]->low_note =
                g_current_editing_sample_parameter_value
                    [g_current_editing_sample_parameter];
        }
        if (g_current_editing_sample_parameter == SAMPLE_PARAM_HI_NOTE) {
            g_samples[sample_number]->hi_note =
                g_current_editing_sample_parameter_value
                    [g_current_editing_sample_parameter];
        }*/
        if (g_current_editing_sample_parameter == SAMPLE_RECORDING_TIME_IN_MS) {
            g_recording_time_in_ms = g_current_editing_sample_parameter_value
                [g_current_editing_sample_parameter];
        }

        _print_gui_param(g_current_editing_sample_parameter,
                         g_current_editing_sample_parameter_value
                             [g_current_editing_sample_parameter]);
        module_set_param_sample(sample_number,
                                g_current_editing_sample_parameter,
                                g_current_editing_sample_parameter_value
                                    [g_current_editing_sample_parameter]);

        break;

    default:
        break;
    }
}

void PANEL_set_current_sample(int sample_number) {
    g_current_editing_sample_parameter_value[SAMPLE_SELECTED_SAMPLE] =
        sample_number;
}
void PANEL_init() {
    _set_filter_type(FILTER_TYPE_LPF);
}