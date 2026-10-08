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
 * @file    geon_control.c
 *
 * @brief   Geonkick control, evaluated on the CPU.
 *
 * Every instrument holds its envelopes and parameters here.  The tick
 * evaluates the envelopes and sends values to the DSP only when they
 * change, following the monosynth example:
 *
 *   - the envelope x coordinate is time / length, as in Geonkick,
 *   - values are only transmitted when they differ from the last one
 *     sent, so the CPU/DSP link carries control rate, not audio rate.
 */

/*----- Includes -----------------------------------------------------*/

#include <math.h>
#include <string.h>

#include "freetribe.h"

#include "geon_control.h"
#include "geon_envelope.h"
#include "geon_interface.h"

/*----- Macros -------------------------------------------------------*/

#define CONTROL_RATE (1000)

#define DEFAULT_LENGTH_MS 300
#define DEFAULT_FREQUENCY 150.0f
#define DEFAULT_CUTOFF 350.0f
#define DEFAULT_Q 1.0f
#define DEFAULT_DRIVE 1.0f
#define DEFAULT_VOLUME 1.0f

/**
 * @brief   Smallest useful difference between two values.
 */
#define CV_EPSILON 1e-4f

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

static t_geon_control g_control[GEON_INSTRUMENTS];

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _init_instrument(t_geon_control *control);
static void _send_osc_config(uint8_t instrument, t_geon_control *control,
                             uint8_t osc);
static bool _changed(float now, float last);

/*----- Extern function implementations ------------------------------*/

void geon_control_init(void) {

    uint8_t i;

    memset(g_control, 0, sizeof(g_control));

    for (i = 0; i < GEON_INSTRUMENTS; i++) {
        _init_instrument(&g_control[i]);
    }
}

/**
 * @brief   Evaluate every sounding instrument.
 */
void geon_control_process(void) {

    float dt = 1.0f / (float)CONTROL_RATE;
    uint8_t i;
    uint8_t o;
    float env_x;

    t_geon_control *control;
    t_geon_osc_control *osc;
    float value;

    for (i = 0; i < GEON_INSTRUMENTS; i++) {

        control = &g_control[i];

        if (!control->active) {
            continue;
        }

        control->time += dt;

        if (control->time > control->length) {
            control->active = false;
            continue;
        }

        env_x = control->time / control->length;

        /* Global amplitude envelope. */
        value = geon_envelope_get_value(&control->env_amplitude, env_x);

        if (_changed(value, control->last_amplitude)) {
            control->last_amplitude = value;
            geon_set_amplitude(i, value);
        }

        /* Global filter cutoff envelope. */
        if (control->filter_enabled) {

            value = control->filter_cutoff;

            if (_changed(value, control->last_filter_cutoff)) {
                control->last_filter_cutoff = value;
                geon_set_filter_cutoff(i, value);
            }
        }

        for (o = 0; o < GEON_OSCILLATORS_MAX; o++) {

            osc = &control->osc[o];

            if (!osc->enabled) {
                continue;
            }

            /* Frequency envelope. */
            value = osc->frequency *
                    geon_envelope_get_value(&osc->env_frequency, env_x);

            if (_changed(value, osc->last_frequency)) {
                osc->last_frequency = value;
                geon_set_osc_frequency(i, o, value);
            }

            /* Amplitude envelope. */
            value = osc->amplitude *
                    geon_envelope_get_value(&osc->env_amplitude, env_x);

            if (_changed(value, osc->last_amplitude)) {
                osc->last_amplitude = value;
                geon_set_osc_amplitude(i, o, value);
            }

            /* Noise density envelope, for the noise waveforms. */
            if ((osc->func == GEON_OSC_FUNC_NOISE_WHITE) ||
                (osc->func == GEON_OSC_FUNC_NOISE_BROWNIAN)) {

                value = osc->noise_density * geon_envelope_get_value(
                                                &osc->env_noise_density,
                                                env_x);

                if (_changed(value, osc->last_noise_density)) {
                    osc->last_noise_density = value;
                    geon_set_osc_noise_density(i, o, value);
                }
            }
        }
    }
}

t_geon_control *geon_control_get(uint8_t instrument) {

    if (instrument >= GEON_INSTRUMENTS) {
        return NULL;
    }

    return &g_control[instrument];
}

void geon_control_note_on(uint8_t instrument) {

    t_geon_control *control = geon_control_get(instrument);

    if (control == NULL) {
        return;
    }

    control->time = 0.0f;
    control->active = true;
    control->note_on = true;

    geon_note_on(instrument);
}

void geon_control_note_off(uint8_t instrument) {

    t_geon_control *control = geon_control_get(instrument);

    if (control == NULL) {
        return;
    }

    control->note_on = false;
}

void geon_control_all_notes_off(void) {

    uint8_t i;

    for (i = 0; i < GEON_INSTRUMENTS; i++) {
        g_control[i].active = false;
        g_control[i].note_on = false;
    }

    geon_all_notes_off();
}

/**
 * @brief   Send the whole configuration of an instrument to the DSP.
 *
 * Used after a patch change or at start up, when the DSP has no idea of
 * the current values.  The incremental updates during playback assume
 * this ran at least once.
 */
void geon_control_send_all(uint8_t instrument) {

    t_geon_control *control = geon_control_get(instrument);

    uint8_t o;

    if (control == NULL) {
        return;
    }

    geon_set_length(instrument, (uint16_t)(control->length * 1000.0f));
    geon_set_amplitude(instrument, control->amplitude);

    geon_set_filter_enabled(instrument, control->filter_enabled);
    geon_set_filter_type(instrument, control->filter_type);
    geon_set_filter_cutoff(instrument, control->filter_cutoff);
    geon_set_filter_q(instrument, control->filter_q);

    geon_set_distortion_enabled(instrument, control->distortion_enabled);
    geon_set_distortion_drive(instrument, control->distortion_drive);
    geon_set_distortion_volume(instrument, control->distortion_volume);

    for (o = 0; o < GEON_OSCILLATORS_MAX; o++) {
        _send_osc_config(instrument, control, o);
    }
}

/*----- Static function implementations ------------------------------*/

static void _init_instrument(t_geon_control *control) {

    uint8_t o;
    t_geon_osc_control *osc;

    control->active = false;
    control->note_on = false;
    control->length = DEFAULT_LENGTH_MS / 1000.0f;
    control->time = 0.0f;

    control->amplitude = 1.0f;
    control->filter_enabled = false;
    control->filter_type = GEON_FILTER_LOW_PASS;
    control->filter_cutoff = DEFAULT_CUTOFF;
    control->filter_q = DEFAULT_Q;
    control->distortion_enabled = false;
    control->distortion_drive = DEFAULT_DRIVE;
    control->distortion_volume = DEFAULT_VOLUME;

    /* Amplitude envelope, a short percussion curve by default. */
    geon_envelope_init(&control->env_amplitude);
    geon_envelope_set_ramp(&control->env_amplitude, 0.0f, 1.0f, 1.0f, 0.0f);

    for (o = 0; o < GEON_OSCILLATORS_MAX; o++) {

        osc = &control->osc[o];

        osc->enabled = (o == 0);
        osc->func = GEON_OSC_FUNC_SINE;
        osc->frequency = DEFAULT_FREQUENCY;
        osc->amplitude = 1.0f;
        osc->noise_density = 1.0f;
        osc->is_fm = false;
        osc->pitch_amount = 1.0f;

        osc->filter_enabled = false;
        osc->filter_type = GEON_FILTER_LOW_PASS;
        osc->filter_cutoff = DEFAULT_CUTOFF;
        osc->filter_q = DEFAULT_Q;

        geon_envelope_init(&osc->env_frequency);
        geon_envelope_set_ramp(&osc->env_frequency, 0.0f, 1.0f, 1.0f, 1.0f);

        geon_envelope_init(&osc->env_amplitude);
        geon_envelope_set_ramp(&osc->env_amplitude, 0.0f, 1.0f, 1.0f, 0.0f);

        geon_envelope_init(&osc->env_noise_density);
        geon_envelope_set_ramp(&osc->env_noise_density, 0.0f, 1.0f, 1.0f, 1.0f);

        osc->last_frequency = -1.0f;
        osc->last_amplitude = -1.0f;
        osc->last_noise_density = -1.0f;
        osc->last_filter_cutoff = -1.0f;
    }

    control->last_amplitude = -1.0f;
    control->last_filter_cutoff = -1.0f;
}

static void _send_osc_config(uint8_t instrument, t_geon_control *control,
                             uint8_t osc) {

    t_geon_osc_control *o = &control->osc[osc];

    geon_set_osc_enabled(instrument, osc, o->enabled);
    geon_set_osc_func(instrument, osc, o->func);
    geon_set_osc_frequency(instrument, osc, o->frequency);
    geon_set_osc_amplitude(instrument, osc, o->amplitude);
    geon_set_osc_noise_density(instrument, osc, o->noise_density);
    geon_set_osc_fm(instrument, osc, o->is_fm);

    geon_set_osc_filter_enabled(instrument, osc, o->filter_enabled);
    geon_set_osc_filter_type(instrument, osc, o->filter_type);
    geon_set_osc_filter_cutoff(instrument, osc, o->filter_cutoff);
    geon_set_osc_filter_q(instrument, osc, o->filter_q);

    /* The DSP takes the last values sent as the current ones. */
    o->last_frequency = o->frequency;
    o->last_amplitude = o->amplitude;
    o->last_noise_density = o->noise_density;
    o->last_filter_cutoff = o->filter_cutoff;
}

/**
 * @brief   Has a control value changed enough to be worth sending?
 *
 * Relative, because the values range from a fraction of a Hz to
 * thousands.  The first call for a value always reports a change, as
 * the last value is initialised to -1.
 */
static bool _changed(float now, float last) {

    float scale = fmaxf(fabsf(now), 1.0f);

    return fabsf(now - last) > (CV_EPSILON * scale);
}

/*----- End of file --------------------------------------------------*/
