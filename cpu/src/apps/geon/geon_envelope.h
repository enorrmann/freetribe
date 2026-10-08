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
 * @file    geon_envelope.h
 *
 * @brief   Public API for Geonkick envelopes, evaluated on the CPU.
 *
 * A port of the Geonkick envelope (src/dsp/src/envelope.c,
 * Copyright (C) 2017 Iurie Nistor, GPL-3.0-or-later), following the
 * monosynth example: the envelopes live on the CPU and the value for a
 * given position in the instrument is sent to the DSP.
 */

#ifndef GEON_ENVELOPE_H
#define GEON_ENVELOPE_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

/*----- Macros -------------------------------------------------------*/

/**
 * @brief   Maximum number of points in an envelope.
 */
#define GEON_MAX_ENVELOPE_POINTS 8

/**
 * @brief   Envelope interpolation.
 */
typedef enum {
    GEON_ENVELOPE_APPLY_LINEAR = 0,
    GEON_ENVELOPE_APPLY_LOGARITHMIC = 1,
} e_geon_envelope_apply;

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    float x;
    float y;
} t_geon_envelope_point;

/**
 * @brief   Envelope, a sorted list of points.
 */
typedef struct {
    e_geon_envelope_apply apply_type;
    uint8_t npoints;
    t_geon_envelope_point points[GEON_MAX_ENVELOPE_POINTS];
} t_geon_envelope;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void geon_envelope_init(t_geon_envelope *envelope);

/**
 * @brief       Replace the envelope with a two point ramp.
 */
void geon_envelope_set_ramp(t_geon_envelope *envelope, float x0, float y0,
                            float x1, float y1);

/**
 * @brief       Add a point, keeping the list sorted by x.
 *
 * Points with the same x as an existing point are ignored.
 */
bool geon_envelope_add_point(t_geon_envelope *envelope, float x, float y);

void geon_envelope_clear(t_geon_envelope *envelope);

void geon_envelope_set_apply_type(t_geon_envelope *envelope,
                                  e_geon_envelope_apply apply_type);

e_geon_envelope_apply
geon_envelope_get_apply_type(const t_geon_envelope *envelope);

/**
 * @brief   Value of envelope at normalised position xm (0...1).
 */
float geon_envelope_get_value(const t_geon_envelope *envelope, float xm);

#ifdef __cplusplus
}
#endif
#endif /* GEON_ENVELOPE_H */

/*----- End of file --------------------------------------------------*/
