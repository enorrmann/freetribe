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
 * @file    geon_instrument.c
 *
 * @brief   A Geonkick instrument, oscillators mixed through the
 *          amplitude envelope, filter and distortion.
 *
 * Ported from Geonkick (src/dsp/src/synthesizer.c), which is
 * Copyright (C) 2018 Iurie Nistor, GPL-3.0-or-later with the
 * architecture of the Freetribe monosynth example:
 *
 *   - The CPU evaluates the amplitude, frequency, cutoff and drive
 *     envelopes and sends the values only when they change.
 *   - The DSP renders the instrument sample by sample from those
 *     values, so it needs no envelope points and no kick buffer.
 */

/*----- Includes -----------------------------------------------------*/

#include <math.h>

#include "geon_filter.h"
#include "geon_instrument.h"
#include "geon_oscillator.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static float _distortion(float in, float drive, float volume);

/*----- Extern function implementations ------------------------------*/

void geon_instrument_init(t_geon_instrument *instrument, int sample_rate) {

    uint8_t o;

    instrument->enabled = false;
    instrument->sample_rate = sample_rate;
    instrument->current_time = 0.0f;
    instrument->length = GEON_DEFAULT_LENGTH;
    instrument->amplitude = 1.0f;
    instrument->filter_cutoff = GEON_DEFAULT_FILTER_CUTOFF;
    instrument->filter_q = GEON_DEFAULT_FILTER_FACTOR;
    instrument->filter_enabled = false;
    instrument->distortion_enabled = false;
    instrument->distortion_drive = 1.0f;
    instrument->distortion_volume = 1.0f;

    for (o = 0; o < GEON_OSCILLATORS; o++) {
        geon_oscillator_init(&instrument->osc[o], sample_rate);
    }

    /* First oscillator sounds by default. */
    instrument->osc[0].enabled = true;

    geon_filter_init(&instrument->filter, sample_rate);
}

void geon_instrument_trigger(t_geon_instrument *instrument) {

    uint8_t o;

    instrument->current_time = 0.0f;

    for (o = 0; o < GEON_OSCILLATORS; o++) {
        geon_oscillator_reset(&instrument->osc[o]);
    }

    geon_filter_reset(&instrument->filter);
}

bool geon_instrument_process(t_geon_instrument *instrument, float *out) {

    float val = 0.0f;
    float fm_val;
    uint8_t o;

    if (instrument->current_time > instrument->length) {
        *out = 0.0f;
        return false;
    }

    for (o = 0; o < GEON_OSCILLATORS; o++) {

        if (!instrument->osc[o].enabled) {
            continue;
        }

        /* First oscillator can frequency modulate the second one. */
        if (instrument->osc[o].is_fm && (o == 0)) {

            fm_val = geon_oscillator_value(&instrument->osc[o]);
            instrument->osc[1].fm_input = fm_val;

        } else {

            val += geon_oscillator_value(&instrument->osc[o]);
        }

        geon_oscillator_increment_phase(&instrument->osc[o]);
    }

    /* Amplitude envelope value, evaluated on the CPU. */
    val *= instrument->amplitude;

    if (instrument->filter_enabled) {
        val = geon_filter_val(&instrument->filter, val);
    }

    if (instrument->distortion_enabled) {
        val = _distortion(val, instrument->distortion_drive,
                          instrument->distortion_volume);
    }

    if (isnan(val)) {
        val = 0.0f;

    } else {
        val = geon_clamp(val, -1.0f, 1.0f);
    }

    *out = val;

    instrument->current_time += 1.0f / (float)instrument->sample_rate;

    return (instrument->current_time <= instrument->length);
}

/*----- Static function implementations ------------------------------*/

/**
 * @brief   Soft clipping distortion.
 *
 * Geonkick offers several curves, the tanh one is used here as it is
 * the upstream default.
 */
static float _distortion(float in, float drive, float volume) {

    return tanhf(drive * in) * volume;
}

/*----- End of file --------------------------------------------------*/
