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
 * @file    undo.c
 *
 * @brief   Undo stack implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include <string.h>

#include "undo.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void undo_init(t_undo_stack *undo) {

    memset(undo, 0, sizeof(t_undo_stack));
}

void undo_push(t_undo_stack *undo, const t_song *song, uint8_t pattern) {

    t_undo_entry *slot;

    pattern %= TRACKER_PATTERNS;
    slot = &undo->entry[undo->head];

    slot->pattern = song->pattern[pattern];
    slot->pattern_index = pattern;

    undo->head = (undo->head + 1) % UNDO_LEVELS;

    if (undo->count < UNDO_LEVELS) {
        undo->count++;
    }
}

bool undo_pop(t_undo_stack *undo, t_song *song) {

    t_undo_entry *slot;

    if (undo->count == 0) {
        return false;
    }

    // Step back through the ring.
    undo->head = (undo->head + UNDO_LEVELS - 1) % UNDO_LEVELS;
    undo->count--;

    slot = &undo->entry[undo->head];
    song->pattern[slot->pattern_index] = slot->pattern;

    return true;
}

void undo_clear(t_undo_stack *undo) { memset(undo, 0, sizeof(t_undo_stack)); }

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
