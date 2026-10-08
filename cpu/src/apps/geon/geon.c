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
 * @file    geon.c
 *
 * @brief   Geonkick percussion synthesizer for the Electribe 2.
 *
 * A port of Geonkick to Freetribe, following the monosynth example:
 * the CPU owns every envelope and parameter and sends the DSP the
 * values it needs only when they change.
 *
 * Panel mapping, on the Electribe 2S:
 *
 *   Trigger pads      Play an instrument, velocity sets its amplitude.
 *   [Osc] encoder     Select the waveform of the current oscillator.
 *   [Filter] encoder  Select the filter type.
 *   [Mod] encoder     Select the modulation, noise density or FM.
 *   [Level] knob      Instrument level.
 *   [Pitch] knob      Oscillator frequency.
 *   [Res] knob        Filter resonance.
 *   [EG] knob         Filter cutoff, or envelope amount with Shift.
 *   [Attack] knob     Instrument length, or amp attack with Shift.
 *   [Decay] knob      Amp decay, or amp release with Shift.
 *   [Mod Depth] knob  Envelope amount of the current parameter.
 *   [Mod Speed] knob  Oscillator pitch envelope depth.
 *   [LPF]/[HPF]/[BPF] Enable and select the filter.
 *   [Amp EG]          Toggle the oscillator / the instrument.
 *   [Menu]            Print the current patch.
 *   [Shift]           Second function of the knobs.
 *   [Play]            All notes off.
 */

/*----- Includes -----------------------------------------------------*/

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "freetribe.h"

#include "gui_task.h"

#include "geon_control.h"
#include "geon_envelope.h"
#include "geon_interface.h"

/*----- Macros -------------------------------------------------------*/

#define CONTROL_RATE (1000)

#define TRIGGER_PADS 16

/* Panel control indices, as reported by svc_panel. */
#define KNOB_LEVEL 0x00
#define KNOB_PITCH 0x02
#define KNOB_RES 0x03
#define KNOB_EG 0x04
#define KNOB_MOD_DEPTH 0x05
#define KNOB_ATTACK 0x06
#define KNOB_DECAY 0x08
#define KNOB_MOD_SPEED 0x0a

#define ENCODER_OSC 0x01
#define ENCODER_CUTOFF 0x02
#define ENCODER_MOD 0x03

#define BUTTON_MENU 0x09
#define BUTTON_PLAY 0x13
#define BUTTON_SHIFT 0x0a

#define BUTTON_LPF 0x12
#define BUTTON_HPF 0x14
#define BUTTON_BPF 0x16
#define BUTTON_AMP_EG 0x20

#define OSC_COUNT GEON_OSCILLATORS

#define FREQ_MIN 20.0f
#define FREQ_MAX 8000.0f

#define CUTOFF_MIN 20.0f
#define CUTOFF_MAX 16000.0f

#define LENGTH_MIN_MS 20
#define LENGTH_MAX_MS GEON_MAX_LENGTH_MS

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   What the [Mod] encoder and the [Mod Depth] knob edit.
 */
typedef enum {
    MOD_NOISE_DENSITY,
    MOD_FM,
    MOD_OSC_LEVEL,
    MOD_TYPE_COUNT
} e_mod_type;

/*----- Static variable definitions ----------------------------------*/

static uint8_t g_instrument;
static uint8_t g_oscillator;

static e_mod_type g_mod_type;

static bool g_shift_held;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _tick_callback(void);
static void _knob_callback(uint8_t index, uint8_t value);
static void _encoder_callback(uint8_t index, uint8_t value);
static void _button_callback(uint8_t index, bool state);
static void _trigger_callback(uint8_t pad, uint8_t velocity, bool state);

static void _set_filter_type(uint8_t type);
static void _send_length(uint8_t instrument, uint16_t milliseconds);
static void _send_frequency(uint8_t instrument, uint8_t osc, float hz);
static void _send_cutoff(uint8_t instrument, float hz);

static float _knob_to_float(uint8_t value);
static float _knob_to_frequency(uint8_t value, float low, float high);

static void _patch_init(uint8_t instrument);
static void _print_patch(void);

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

    t_status status = ERROR;
    uint8_t i;

    g_instrument = 0;
    g_oscillator = 0;
    g_mod_type = MOD_NOISE_DENSITY;
    g_shift_held = false;

    geon_control_init();

    /* Build the default kit. */
    for (i = 0; i < TRIGGER_PADS; i++) {

        _patch_init(i);

        /* Every pad gets a different pitch envelope, so the default kit
         * covers kicks, toms, snares and hats out of the box. */
        geon_control_send_all(i);
    }

    ft_register_panel_callback(KNOB_EVENT, _knob_callback);
    ft_register_panel_callback(ENCODER_EVENT, _encoder_callback);
    ft_register_panel_callback(BUTTON_EVENT, _button_callback);
    ft_register_panel_callback(TRIGGER_EVENT, _trigger_callback);

    ft_register_tick_callback(0, _tick_callback);

    ft_set_led(LED_OSC_ENC, LED_ON);

    /* Initialise GUI. */
    gui_task();

    ft_print("Freetribe Geonkick");
    gui_print(4, 7, "Geonkick");
    gui_print(4, 18, "Pad 1: Kick");

    status = SUCCESS;
    return status;
}

/**
 * @brief   Run application.
 */
void app_run(void) { gui_task(); }

/*----- Static function implementations ------------------------------*/

static void _tick_callback(void) { geon_control_process(); }

/**
 * @brief   Callback triggered by panel knob events.
 *
 * @param[in]   index   Index of knob.
 * @param[in]   value   Value of knob.
 */
static void _knob_callback(uint8_t index, uint8_t value) {

    t_geon_control *control = geon_control_get(g_instrument);
    t_geon_osc_control *osc;

    if (control == NULL) {
        return;
    }

    osc = &control->osc[g_oscillator];

    switch (index) {

    case KNOB_LEVEL:
        control->amplitude = _knob_to_float(value);
        geon_set_amplitude(g_instrument, control->amplitude);
        gui_post_param("Level: ", value);
        break;

    case KNOB_PITCH:

        osc->frequency = _knob_to_frequency(value, FREQ_MIN, FREQ_MAX);
        _send_frequency(g_instrument, g_oscillator, osc->frequency);
        gui_post_param("Freq: ", value);
        break;

    case KNOB_RES:
        control->filter_q = 0.5f + (_knob_to_float(value) * 9.5f);
        geon_set_filter_q(g_instrument, control->filter_q);
        geon_set_osc_filter_q(g_instrument, g_oscillator, control->filter_q);
        gui_post_param("Res: ", value);
        break;

    case KNOB_EG:

        if (g_shift_held) {

            /* Envelope amount of the amplitude envelope, as a decay. */
            geon_envelope_set_ramp(&control->env_amplitude, 0.0f,
                                   _knob_to_float(value), 1.0f, 0.0f);

            gui_post_param("Amp EG: ", value);

        } else {

            control->filter_cutoff =
                _knob_to_frequency(value, CUTOFF_MIN, CUTOFF_MAX);

            _send_cutoff(g_instrument, control->filter_cutoff);

            geon_set_osc_filter_cutoff(g_instrument, g_oscillator,
                                       control->filter_cutoff);

            gui_post_param("Cutoff: ", value);
        }
        break;

    case KNOB_ATTACK:

        if (g_shift_held) {

            geon_envelope_set_ramp(&control->env_amplitude, 0.0f, 0.0f,
                                   _knob_to_float(value), 1.0f);

            gui_post_param("Amp Atk: ", value);

        } else {

            _send_length(g_instrument,
                         LENGTH_MIN_MS +
                             (uint16_t)(_knob_to_float(value) *
                                        (LENGTH_MAX_MS - LENGTH_MIN_MS)));

            gui_post_param("Length: ", value);
        }
        break;

    case KNOB_DECAY:

        /* Pitch sweep: ramp the oscillator frequency from the [Pitch]
         * value down to the pitch amount set by [Mod Speed]. */
        osc->pitch_amount = _knob_to_float(value);

        geon_envelope_set_ramp(&osc->env_frequency, 0.0f, 1.0f, 1.0f,
                               osc->pitch_amount);

        gui_post_param("Pitch Dec: ", value);
        break;

    case KNOB_MOD_DEPTH:

        switch (g_mod_type) {

        case MOD_NOISE_DENSITY:
            osc->noise_density = _knob_to_float(value);
            geon_set_osc_noise_density(g_instrument, g_oscillator,
                                       osc->noise_density);
            gui_post_param("Noise: ", value);
            break;

        case MOD_FM:
            osc->is_fm = (value > 0x40);
            geon_set_osc_fm(g_instrument, g_oscillator, osc->is_fm);
            gui_post_param("FM: ", osc->is_fm);
            break;

        case MOD_OSC_LEVEL:
            /* Oscillator level. */
            osc->amplitude = _knob_to_float(value);
            geon_set_osc_amplitude(g_instrument, g_oscillator,
                                   osc->amplitude);
            gui_post_param("Osc Amp: ", value);
            break;

        default:
            break;
        }
        break;

    case KNOB_MOD_SPEED:

        /* Pitch envelope depth of the current oscillator. */
        osc->pitch_amount = _knob_to_float(value) * 4.0f;

        geon_envelope_set_ramp(&osc->env_frequency, 0.0f, 1.0f, 1.0f,
                               osc->pitch_amount);

        gui_post_param("Pitch Amt: ", value);
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

    t_geon_control *control = geon_control_get(g_instrument);
    int8_t delta = (value == 0x01) ? 1 : -1;

    if (control == NULL) {
        return;
    }

    switch (index) {

    case ENCODER_OSC:

        if (delta > 0) {
            control->osc[g_oscillator].func++;
            if (control->osc[g_oscillator].func >= GEON_OSC_FUNC_COUNT) {
                control->osc[g_oscillator].func = 0;
            }

        } else {
            if (control->osc[g_oscillator].func == 0) {
                control->osc[g_oscillator].func = GEON_OSC_FUNC_COUNT - 1;

            } else {
                control->osc[g_oscillator].func--;
            }
        }

        geon_set_osc_func(g_instrument, g_oscillator,
                          control->osc[g_oscillator].func);

        gui_post_param("Wave: ", control->osc[g_oscillator].func);
        break;

    case ENCODER_CUTOFF:

        if (delta > 0) {
            control->filter_type++;
            if (control->filter_type >= GEON_FILTER_TYPE_COUNT) {
                control->filter_type = 0;
            }

        } else {
            if (control->filter_type == 0) {
                control->filter_type = GEON_FILTER_TYPE_COUNT - 1;

            } else {
                control->filter_type--;
            }
        }

        _set_filter_type(control->filter_type);
        break;

    case ENCODER_MOD:

        if (delta > 0) {
            g_mod_type++;
            if (g_mod_type >= MOD_TYPE_COUNT) {
                g_mod_type = 0;
            }

        } else {
            if (g_mod_type == 0) {
                g_mod_type = MOD_TYPE_COUNT - 1;

            } else {
                g_mod_type--;
            }
        }

        gui_post_param("Mod: ", g_mod_type);
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
static void _button_callback(uint8_t index, bool state) {

    t_geon_control *control = geon_control_get(g_instrument);
    t_geon_osc_control *osc;

    if (control == NULL) {
        return;
    }

    osc = &control->osc[g_oscillator];

    switch (index) {

    case BUTTON_SHIFT:
        g_shift_held = state;
        break;

    case BUTTON_MENU:
        if (state) {
            _print_patch();
        }
        break;

    case BUTTON_PLAY:
        if (state) {
            geon_control_all_notes_off();
            gui_post("All notes off");
        }
        break;

    case BUTTON_LPF:
        if (state) {
            _set_filter_type(GEON_FILTER_LOW_PASS);
        }
        break;

    case BUTTON_HPF:
        if (state) {
            _set_filter_type(GEON_FILTER_HIGH_PASS);
        }
        break;

    case BUTTON_BPF:
        if (state) {
            _set_filter_type(GEON_FILTER_BAND_PASS);
        }
        break;

    case BUTTON_AMP_EG:
        if (state) {

            if (g_shift_held) {

                /* Select which oscillator the knobs edit. */
                g_oscillator++;
                if (g_oscillator >= OSC_COUNT) {
                    g_oscillator = 0;
                }

                ft_set_led(LED_OSC_ENC, g_oscillator == 0 ? LED_ON : LED_OFF);
                ft_set_led(LED_MAIN_ENC, g_oscillator == 1 ? LED_ON : LED_OFF);

                gui_post_param("Osc: ", g_oscillator);

            } else {

                osc->enabled = !osc->enabled;
                geon_set_osc_enabled(g_instrument, g_oscillator, osc->enabled);
                ft_set_led(LED_AMP_EG, osc->enabled ? LED_ON : LED_OFF);
                gui_post_param("Osc On: ", osc->enabled);
            }
        }
        break;

    default:
        break;
    }
}

/**
 * @brief   Callback triggered by the trigger pads.
 *
 * @param[in]   pad         Index of pad.
 * @param[in]   velocity    Velocity of pad.
 * @param[in]   state       State of pad.
 */
static void _trigger_callback(uint8_t pad, uint8_t velocity, bool state) {

    if (pad >= TRIGGER_PADS) {
        return;
    }

    if (state) {

        g_instrument = pad;

        /* Velocity scales the instrument level. */
        t_geon_control *control = geon_control_get(pad);

        if (control != NULL) {

            control->amplitude =
                fmaxf(0.1f, (float)velocity / 127.0f) * 1.0f;

            geon_set_amplitude(pad, control->amplitude);
        }

        geon_control_note_on(pad);

        /* Show which instrument is being edited. */
        ft_set_led(LED_PAD_0_RED + (pad * 2), LED_ON);
        gui_post_param("Pad: ", pad);

    } else {
        geon_control_note_off(pad);
    }
}

static void _set_filter_type(uint8_t type) {

    t_geon_control *control = geon_control_get(g_instrument);

    if (control == NULL) {
        return;
    }

    control->filter_type = (e_geon_filter_type)type;

    control->filter_enabled = true;

    ft_set_led(LED_LPF, type == GEON_FILTER_LOW_PASS ? LED_ON : LED_OFF);
    ft_set_led(LED_HPF, type == GEON_FILTER_HIGH_PASS ? LED_ON : LED_OFF);
    ft_set_led(LED_BPF, type == GEON_FILTER_BAND_PASS ? LED_ON : LED_OFF);

    geon_set_filter_enabled(g_instrument, true);
    geon_set_filter_type(g_instrument, control->filter_type);
    geon_set_osc_filter_enabled(g_instrument, g_oscillator, true);
    geon_set_osc_filter_type(g_instrument, g_oscillator, control->filter_type);

    gui_post_param("Filter: ", type);
}

static void _send_length(uint8_t instrument, uint16_t milliseconds) {

    t_geon_control *control = geon_control_get(instrument);

    if (control == NULL) {
        return;
    }

    control->length = (float)milliseconds / 1000.0f;

    geon_set_length(instrument, milliseconds);
}

static void _send_frequency(uint8_t instrument, uint8_t osc, float hz) {

    t_geon_control *control = geon_control_get(instrument);

    if (control == NULL) {
        return;
    }

    /* Store the base frequency, the tick sends the envelope scaled
     * value.  Also send it now so the change is heard without waiting,
     * the envelope x is 0 at the start of an instrument so the tick
     * would otherwise send the same value straight away. */
    control->osc[osc].frequency = hz;
    control->osc[osc].last_frequency = -1.0f;

    geon_set_osc_frequency(instrument, osc, hz);
}

static void _send_cutoff(uint8_t instrument, float hz) {

    t_geon_control *control = geon_control_get(instrument);

    if (control == NULL) {
        return;
    }

    geon_set_filter_cutoff(instrument, hz);
}

static float _knob_to_float(uint8_t value) { return (float)value / 127.0f; }

/**
 * @brief   Map a knob to a frequency, logarithmically.
 */
static float _knob_to_frequency(uint8_t value, float low, float high) {

    float norm = _knob_to_float(value);

    return low * powf(high / low, norm);
}

/**
 * @brief   Set up a default patch.
 *
 * Pad 0 is a kick, the rest are shaped by the patch number so the
 * default kit is playable straight away.
 */
static void _patch_init(uint8_t instrument) {

    t_geon_control *control = geon_control_get(instrument);
    t_geon_osc_control *osc;
    float base;

    if (control == NULL) {
        return;
    }

    osc = &control->osc[0];

    /* Spread the instruments over a few octaves, lowest pad lowest. */
    base = 60.0f * powf(2.0f, (float)instrument / 4.0f);

    if (base > 4000.0f) {
        base = 4000.0f;
    }

    osc->frequency = base;
    osc->pitch_amount = 1.0f;

    /* A pitch sweep over the instrument is what makes a kick a kick. */
    geon_envelope_set_ramp(&control->env_amplitude, 0.0f, 1.0f, 1.0f, 0.0f);
    geon_envelope_set_ramp(&osc->env_frequency, 0.0f, 1.0f, 1.0f,
                           osc->pitch_amount);
    geon_envelope_set_ramp(&osc->env_amplitude, 0.0f, 1.0f, 1.0f, 0.0f);

    control->length = 0.3f;
    control->amplitude = 0.9f;
}

/**
 * @brief   Print the current patch to the console.
 */
static void _print_patch(void) {

    t_geon_control *control = geon_control_get(g_instrument);

    if (control == NULL) {
        return;
    }

    ft_printf("Geonkick\n");
    ft_printf("Instrument %u\n", g_instrument);
    ft_printf("Oscillator %u, wave %u\n", g_oscillator,
              control->osc[g_oscillator].func);
    ft_printf("Frequency %u Hz\n",
              (unsigned)control->osc[g_oscillator].frequency);
    ft_printf("Length %u ms\n", (unsigned)(control->length * 1000.0f));
    ft_printf("Level %u\n", (unsigned)(control->amplitude * 100.0f));
    ft_printf("Cutoff %u Hz\n", (unsigned)control->filter_cutoff);
    ft_printf("Resonance %u\n", (unsigned)(control->filter_q * 10.0f));

    gui_post("Patch printed");
}

/*----- End of file --------------------------------------------------*/
