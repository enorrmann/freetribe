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
 * @file    geon_oscillator.h
 *
 * @brief   Public API for Geonkick oscillators.
 */

#ifndef GEON_OSCILLATOR_H
#define GEON_OSCILLATOR_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include "geon_types.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void geon_oscillator_init(t_geon_oscillator *osc, int sample_rate);

/**
 * @brief   Reset phase, FM input, noise seed and filter state.
 */
void geon_oscillator_reset(t_geon_oscillator *osc);

/**
 * @brief   Advance the phase by one sample.
 *
 * The frequency for this sample is supplied by the CPU, which
 * evaluates the frequency envelope.
 */
void geon_oscillator_increment_phase(t_geon_oscillator *osc);

/**
 * @brief   One sample of oscillator output.
 *
 * Uses the amplitude, frequency and noise density most recently set by
 * the CPU.
 */
float geon_oscillator_value(t_geon_oscillator *osc);

float geon_osc_func_sine(float phase);

float geon_osc_func_square(float phase);

float geon_osc_func_triangle(float phase);

float geon_osc_func_sawtooth(float phase);

float geon_osc_func_noise_white(uint32_t *seed, uint32_t density);

float geon_osc_func_noise_brownian(float *previous, uint32_t *seed,
                                   uint32_t density);

#ifdef __cplusplus
}
#endif
#endif /* GEON_OSCILLATOR_H */

/*----- End of file --------------------------------------------------*/
