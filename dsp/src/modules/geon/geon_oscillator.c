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
 * @file    geon_oscillator.c
 *
 * @brief   Oscillators, as used by Geonkick.
 *
 * Ported from Geonkick (src/dsp/src/oscillator.c), which is
 * Copyright (C) 2017 Iurie Nistor, GPL-3.0-or-later.
 *
 * Geonkick evaluates the amplitude, frequency and noise density
 * envelopes inside the oscillator.  Here the CPU does that and sends
 * the resulting values, so these functions only generate a sample from
 * the values they were given.
 *
 * Geonkick uses rand() for the noise generators, this port uses a
 * small deterministic PRNG so that a given seed gives the same noise
 * everywhere.
 */

/*----- Includes -----------------------------------------------------*/

#include <math.h>

#include "geon_filter.h"
#include "geon_oscillator.h"

/*----- Macros -------------------------------------------------------*/

#define GEON_RAND_MAX 0x7fffffff

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static uint32_t _geon_rand(uint32_t *seed);

/*----- Extern function implementations ------------------------------*/

void geon_oscillator_init(t_geon_oscillator *osc, int sample_rate) {

    osc->enabled = false;
    osc->func = GEON_OSC_FUNC_SINE;
    osc->sample_rate = sample_rate;
    osc->brownian = 0.0f;
    osc->seed = 100;
    osc->seedp = osc->seed;
    osc->phase = 0.0f;
    osc->frequency = 150.0f;
    osc->amplitude = 1.0f;
    osc->noise_density = 1.0f;
    osc->fm_input = 0.0f;
    osc->fm_k = 1.0f;
    osc->is_fm = false;

    osc->filter_enabled = false;
    geon_filter_init(&osc->filter, sample_rate);
}

void geon_oscillator_reset(t_geon_oscillator *osc) {

    osc->phase = 0.0f;
    osc->fm_input = 0.0f;
    osc->seedp = osc->seed;
    osc->brownian = 0.0f;

    geon_filter_reset(&osc->filter);
}

void geon_oscillator_increment_phase(t_geon_oscillator *osc) {

    float f = osc->frequency;

    /* The first oscillator of an instrument can frequency modulate the
     * second one, as in upstream. */
    f += f * osc->fm_k * osc->fm_input;

    osc->phase += (GEON_2PI * f) / (float)osc->sample_rate;

    if (osc->phase > GEON_2PI) {
        osc->phase -= GEON_2PI;
    }
}

float geon_oscillator_value(t_geon_oscillator *osc) {

    float v;

    switch (osc->func) {

    case GEON_OSC_FUNC_SINE:
        v = geon_osc_func_sine(osc->phase);
        break;

    case GEON_OSC_FUNC_SQUARE:
        v = geon_osc_func_square(osc->phase);
        break;

    case GEON_OSC_FUNC_TRIANGLE:
        v = geon_osc_func_triangle(osc->phase);
        break;

    case GEON_OSC_FUNC_SAWTOOTH:
        v = geon_osc_func_sawtooth(osc->phase);
        break;

    case GEON_OSC_FUNC_NOISE_WHITE:
        v = geon_osc_func_noise_white(
            &osc->seedp, (uint32_t)(GEON_MAX_NOISE_DENSITY *
                                    osc->noise_density));
        break;

    case GEON_OSC_FUNC_NOISE_BROWNIAN:
        v = geon_osc_func_noise_brownian(
            &osc->brownian, &osc->seedp,
            (uint32_t)(GEON_MAX_NOISE_DENSITY * osc->noise_density));
        break;

    default:
        v = geon_osc_func_sine(osc->phase);
        break;
    }

    v *= osc->amplitude;

    if (osc->filter_enabled) {
        v = geon_filter_val(&osc->filter, v);
    }

    return v;
}

float geon_osc_func_sine(float phase) { return sinf(phase); }

float geon_osc_func_square(float phase) {

    if (phase < GEON_PI) {
        return -1.0f;

    } else {
        return 1.0f;
    }
}

float geon_osc_func_triangle(float phase) {

    if (phase < GEON_PI) {
        return -1.0f + (2.0f / GEON_PI) * phase;

    } else {
        return 3.0f - (2.0f / GEON_PI) * phase;
    }
}

float geon_osc_func_sawtooth(float phase) {

    if (phase < GEON_PI) {
        return phase / GEON_PI;

    } else {
        return (phase / GEON_PI) - 2.0f;
    }
}

float geon_osc_func_noise_white(uint32_t *seed, uint32_t density) {

    float result = 0.0f;

    if ((density >= 1) &&
        !(_geon_rand(seed) % (GEON_MAX_NOISE_DENSITY + 1 - density))) {

        result = (2.0f * (float)(_geon_rand(seed) % GEON_RAND_MAX) /
                  (float)GEON_RAND_MAX) -
                 1.0f;
    }

    return result;
}

float geon_osc_func_noise_brownian(float *previous, uint32_t *seed,
                                   uint32_t density) {

    float sign = 1.0f;
    float walk;

    if (_geon_rand(seed) % 2) {
        sign = -1.0f;
    }

    if ((density >= 1) &&
        !(_geon_rand(seed) % (GEON_MAX_NOISE_DENSITY + 1 - density))) {

        walk = sign * 0.1f * ((float)(_geon_rand(seed) % GEON_RAND_MAX) /
                              (float)GEON_RAND_MAX);

    } else {
        walk = 0.0f;
    }

    if (((*previous + walk) > 1.0f) || ((*previous + walk) < -1.0f)) {
        *previous -= walk;

    } else {
        *previous += walk;
    }

    return *previous;
}

/*----- Static function implementations ------------------------------*/

/**
 * @brief   xorshift32 PRNG, seeded with the oscillator seed.
 */
static uint32_t _geon_rand(uint32_t *seed) {

    uint32_t x = *seed;

    if (x == 0) {
        x = 0x1234567;
    }

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    *seed = x;

    return x >> 1;
}

/*----- End of file --------------------------------------------------*/
