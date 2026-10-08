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
 * @file    geon_filter.h
 *
 * @brief   Public API for Geonkick state variable filter.
 */

#ifndef GEON_FILTER_H
#define GEON_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include "geon_types.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void geon_filter_init(t_geon_filter *filter, int sample_rate);

void geon_filter_reset(t_geon_filter *filter);

void geon_filter_set_type(t_geon_filter *filter, e_geon_filter_type type);

/**
 * @brief   Set cutoff and resonance for the current sample.
 *
 * Both values are evaluated on the CPU, this runs at audio rate so it
 * only stores them.
 */
void geon_filter_set_cutoff(t_geon_filter *filter, float cutoff);

void geon_filter_set_factor(t_geon_filter *filter, float factor);

/**
 * @brief   One sample of low/high/band pass filtering.
 *
 * @param[in]   filter  Filter to use.
 * @param[in]   in_val  Input sample.
 *
 * @return      Filtered sample.
 */
float geon_filter_val(t_geon_filter *filter, float in_val);

#ifdef __cplusplus
}
#endif
#endif /* GEON_FILTER_H */

/*----- End of file --------------------------------------------------*/
