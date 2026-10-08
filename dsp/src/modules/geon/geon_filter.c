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
 * @file    geon_filter.c
 *
 * @brief   Low/high/band pass digital state variable filter.
 *
 * Ported from Geonkick (src/dsp/src/filter.c), which is
 * Copyright (C) 2017 Iurie Nistor, GPL-3.0-or-later.
 *
 * Upstream evaluates the cutoff and Q envelopes inside the filter, the
 * CPU does that here, so this only stores the values it is given.
 */

/*----- Includes -----------------------------------------------------*/

#include <math.h>

#include "geon_filter.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void geon_filter_init(t_geon_filter *filter, int sample_rate) {

    uint8_t i;

    filter->type = GEON_FILTER_LOW_PASS;
    filter->sample_rate = sample_rate;
    filter->cutoff = GEON_DEFAULT_FILTER_CUTOFF;
    filter->factor = GEON_DEFAULT_FILTER_FACTOR;
    filter->queue_empty = true;

    for (i = 0; i < 2; i++) {
        filter->queue_l[i] = 0.0f;
        filter->queue_b[i] = 0.0f;
        filter->queue_h[i] = 0.0f;
    }

    filter->coefficients[0] = 0.0f;
    filter->coefficients[1] = filter->factor;
}

void geon_filter_reset(t_geon_filter *filter) {

    uint8_t i;

    filter->queue_empty = true;

    for (i = 0; i < 2; i++) {
        filter->queue_l[i] = 0.0f;
        filter->queue_b[i] = 0.0f;
        filter->queue_h[i] = 0.0f;
    }

    filter->coefficients[1] = filter->factor;
}

void geon_filter_set_type(t_geon_filter *filter, e_geon_filter_type type) {

    filter->type = type;
}

void geon_filter_set_cutoff(t_geon_filter *filter, float cutoff) {

    filter->cutoff = cutoff;
}

void geon_filter_set_factor(t_geon_filter *filter, float factor) {

    /* Upstream uses the reciprocal of the user facing Q factor. */
    filter->factor = 10.0f / factor;
    filter->coefficients[1] = filter->factor;
}

float geon_filter_val(t_geon_filter *filter, float in_val) {

    float F;
    float Q;
    float val;

    if (isnan(in_val)) {
        in_val = 0.0f;

    } else {
        in_val = geon_clamp(in_val, -1.0f, 1.0f);
    }

    F = 2.0f * sinf(GEON_PI * filter->cutoff / (float)filter->sample_rate);

    Q = filter->coefficients[1];

    if (Q > GEON_MAX_FILTER_Q) {
        Q = GEON_MAX_FILTER_Q;
    }

    if (filter->queue_empty) {

        filter->queue_l[0] = filter->queue_l[1] = 0.0f;
        filter->queue_b[0] = filter->queue_b[1] = 0.0f;
        filter->queue_h[0] = filter->queue_h[1] = 0.0f;
        filter->queue_empty = false;

    } else {

        filter->queue_h[0] = filter->queue_h[1];
        filter->queue_b[0] = filter->queue_b[1];
        filter->queue_l[0] = filter->queue_l[1];
    }

    filter->queue_h[1] = in_val - filter->queue_l[0] - (Q * filter->queue_b[0]);
    filter->queue_b[1] = (F * filter->queue_h[1]) + filter->queue_b[0];
    filter->queue_l[1] = (F * filter->queue_b[1]) + filter->queue_l[0];

    switch (filter->type) {

    case GEON_FILTER_HIGH_PASS:
        val = filter->queue_h[1];
        break;

    case GEON_FILTER_BAND_PASS:
        val = filter->queue_b[1];
        break;

    default:
        val = filter->queue_l[1];
        break;
    }

    return val;
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
