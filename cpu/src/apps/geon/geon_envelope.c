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
 * @file    geon_envelope.c
 *
 * @brief   Envelopes, evaluated on the CPU.
 *
 * Ported from Geonkick (src/dsp/src/envelope.c), which is
 * Copyright (C) 2017 Iurie Nistor, GPL-3.0-or-later.
 *
 * Geonkick supports linear segments and quadratic Bezier segments, this
 * port keeps the linear interpolation and the logarithmic apply type,
 * which is what the percussion envelopes need.
 */

/*----- Includes -----------------------------------------------------*/

#include <math.h>
#include <string.h>

#include "geon_envelope.h"

/*----- Macros -------------------------------------------------------*/

#define GEON_EPSILON 1e-7f

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static float _linear_interpolate(float x0, float y0, float x1, float y1,
                                 float x);

/*----- Extern function implementations ------------------------------*/

void geon_envelope_init(t_geon_envelope *envelope) {

    memset(envelope, 0, sizeof(t_geon_envelope));

    envelope->apply_type = GEON_ENVELOPE_APPLY_LINEAR;
}

void geon_envelope_set_ramp(t_geon_envelope *envelope, float x0, float y0,
                            float x1, float y1) {

    geon_envelope_clear(envelope);
    geon_envelope_add_point(envelope, x0, y0);
    geon_envelope_add_point(envelope, x1, y1);
}

bool geon_envelope_add_point(t_geon_envelope *envelope, float x, float y) {

    uint8_t index;
    uint8_t i;

    if ((envelope == NULL) || (envelope->npoints >= GEON_MAX_ENVELOPE_POINTS)) {
        return false;
    }

    /* Find the insert position, keep sorted by x. */
    index = envelope->npoints;

    for (i = 0; i < envelope->npoints; i++) {

        if (x < envelope->points[i].x) {
            index = i;
            break;
        }

        /* Don't allow two points with the same x. */
        if (fabsf(x - envelope->points[i].x) < GEON_EPSILON) {
            return false;
        }
    }

    if (index < envelope->npoints) {

        memmove(&envelope->points[index + 1], &envelope->points[index],
                (envelope->npoints - index) * sizeof(t_geon_envelope_point));
    }

    envelope->points[index].x = x;
    envelope->points[index].y = y;
    envelope->npoints++;

    return true;
}

void geon_envelope_clear(t_geon_envelope *envelope) {

    if (envelope != NULL) {
        envelope->npoints = 0;
    }
}

void geon_envelope_set_apply_type(t_geon_envelope *envelope,
                                  e_geon_envelope_apply apply_type) {

    envelope->apply_type = apply_type;
}

e_geon_envelope_apply
geon_envelope_get_apply_type(const t_geon_envelope *envelope) {

    return envelope->apply_type;
}

float geon_envelope_get_value(const t_geon_envelope *envelope, float xm) {

    uint8_t i;

    if ((envelope == NULL) || (envelope->npoints < 2)) {
        return 0.0f;
    }

    /* Outside the range, and at the ends, as upstream. */
    if ((xm < envelope->points[0].x) ||
        (xm > envelope->points[envelope->npoints - 1].x)) {
        return 0.0f;
    }

    if (fabsf(xm - envelope->points[0].x) < GEON_EPSILON) {
        return envelope->points[0].y;
    }

    if (fabsf(envelope->points[envelope->npoints - 1].x - xm) < GEON_EPSILON) {
        return envelope->points[envelope->npoints - 1].y;
    }

    for (i = 0; i < (uint8_t)(envelope->npoints - 1); i++) {

        if ((envelope->points[i].x <= xm) &&
            (xm <= envelope->points[i + 1].x)) {

            return _linear_interpolate(
                envelope->points[i].x, envelope->points[i].y,
                envelope->points[i + 1].x, envelope->points[i + 1].y, xm);
        }
    }

    return 0.0f;
}

/*----- Static function implementations ------------------------------*/

static float _linear_interpolate(float x0, float y0, float x1, float y1,
                                 float x) {

    if (fabsf(x1 - x0) < GEON_EPSILON) {
        return y0;
    }

    return y0 + ((y1 - y0) * ((x - x0) / (x1 - x0)));
}

/*----- End of file --------------------------------------------------*/
