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
 * @file    geon_types.h
 *
 * @brief   Types and constants for the Geonkick DSP port.
 *
 * A port of the Geonkick percussion synthesizer DSP[1] to the
 * Blackfin used by the Electribe 2.
 *
 * Architecture follows the monosynth example: the CPU owns every
 * envelope, LFO and parameter, and sends the resulting values to the
 * DSP only when they change.  The DSP is a pure sample generator, it
 * holds no envelope points, so its state stays small enough for L1.
 *
 * [1] https://github.com/Geonkick-Synthesizer/geonkick
 *     Copyright (C) 2017 Iurie Nistor, GPL-3.0-or-later.
 */

#ifndef GEON_TYPES_H
#define GEON_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

/*----- Macros -------------------------------------------------------*/

#define GEON_2PI 6.2831853f
#define GEON_PI 3.1415927f
#define GEON_LOG20 1.3010299f

/**
 * @brief   Instruments rendered simultaneously, one per trigger pad.
 */
#define GEON_INSTRUMENTS 16

/**
 * @brief   Oscillators per instrument.
 */
#define GEON_OSCILLATORS 2

/**
 * @brief   Maximum length of an instrument in seconds.
 */
#define GEON_MAX_LENGTH 2.0f

#define GEON_DEFAULT_LENGTH 0.3f

#define GEON_MAX_NOISE_DENSITY 400

#define GEON_DEFAULT_FILTER_CUTOFF 350.0f
#define GEON_DEFAULT_FILTER_FACTOR 1.0f
#define GEON_MAX_FILTER_Q 10.0f

/**
 * @brief   Per oscillator envelope input.
 *
 * The CPU evaluates the envelopes, these are the values it sends.
 */
#define GEON_ENV_AMPLITUDE 0
#define GEON_ENV_FREQUENCY 1
#define GEON_ENV_NOISE_DENSITY 2

#define geon_clamp(value, low, high)                                       \
    (((value) < (low)) ? (low) : (((value) > (high)) ? (high) : (value)))

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   Waveform of an oscillator.
 */
typedef enum {
    GEON_OSC_FUNC_SINE = 0,
    GEON_OSC_FUNC_SQUARE,
    GEON_OSC_FUNC_TRIANGLE,
    GEON_OSC_FUNC_SAWTOOTH,
    GEON_OSC_FUNC_NOISE_WHITE,
    GEON_OSC_FUNC_NOISE_BROWNIAN,

    GEON_OSC_FUNC_COUNT
} e_geon_osc_func;

/**
 * @brief   Filter type.
 */
typedef enum {
    GEON_FILTER_LOW_PASS = 0,
    GEON_FILTER_HIGH_PASS,
    GEON_FILTER_BAND_PASS,

    GEON_FILTER_TYPE_COUNT
} e_geon_filter_type;

/**
 * @brief   Digital state variable filter, as used by Geonkick.
 *
 * The CPU sends the cutoff and Q for the current sample, so there is
 * no envelope here, only the filter state.
 */
typedef struct {
    e_geon_filter_type type;
    int sample_rate;
    float cutoff;
    float factor;
    float queue_l[2];
    float queue_b[2];
    float queue_h[2];
    bool queue_empty;
    float coefficients[2];
} t_geon_filter;

/**
 * @brief   A single oscillator of an instrument.
 *
 * amplitude, frequency and noise_density are the envelope values for
 * the current sample, updated by the CPU.
 */
typedef struct {
    bool enabled;
    e_geon_osc_func func;
    int sample_rate;

    /* Used for Brownian noise. */
    float brownian;

    uint32_t seed;
    uint32_t seedp;

    float phase;
    float frequency;
    float amplitude;
    float noise_density;

    /* FM input value and coefficient. */
    float fm_input;
    float fm_k;

    /* Specifies if this oscillator is a FM source for the next one. */
    bool is_fm;

    bool filter_enabled;
    t_geon_filter filter;
} t_geon_oscillator;

/**
 * @brief   A Geonkick instrument (a "kick").
 *
 * amplitude and filter_cutoff are the envelope values for the current
 * sample, updated by the CPU.
 */
typedef struct {
    bool enabled;

    int sample_rate;
    float current_time;
    float length;
    float amplitude;

    /* Cutoff and Q of the global filter, evaluated on the CPU. */
    float filter_cutoff;
    float filter_q;

    bool filter_enabled;
    t_geon_filter filter;

    bool distortion_enabled;
    float distortion_drive;
    float distortion_volume;

    t_geon_oscillator osc[GEON_OSCILLATORS];
} t_geon_instrument;

#ifdef __cplusplus
}
#endif
#endif /* GEON_TYPES_H */

/*----- End of file --------------------------------------------------*/
