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
 * @file    geon_instrument.h
 *
 * @brief   Public API for a Geonkick instrument.
 */

#ifndef GEON_INSTRUMENT_H
#define GEON_INSTRUMENT_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include "geon_types.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void geon_instrument_init(t_geon_instrument *instrument, int sample_rate);

/**
 * @brief   Trigger the instrument, resetting oscillators and filters.
 */
void geon_instrument_trigger(t_geon_instrument *instrument);

/**
 * @brief   Render one sample of the instrument.
 *
 * Advances internal time, call once per sample while the instrument is
 * sounding.  The amplitude, cutoff and distortion values most recently
 * set by the CPU are applied.
 *
 * @param[in]   instrument  Instrument to render.
 * @param[out]  out         Rendered sample.
 *
 * @return      true if the instrument is still rendering.
 */
bool geon_instrument_process(t_geon_instrument *instrument, float *out);

#ifdef __cplusplus
}
#endif
#endif /* GEON_INSTRUMENT_H */

/*----- End of file --------------------------------------------------*/
