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
 * @file    undo.h
 *
 * @brief   Multi-level undo for destructive edits.
 *
 * Undo stores per-pattern snapshots on a fixed-size ring.  A snapshot is
 * cheap to take (a pattern is a flat array) and restores the full pattern
 * state, which keeps the logic trivially correct at the cost of a little
 * memory.  Only the affected pattern is snapshotted.
 */

#ifndef TRACKER_UNDO_H
#define TRACKER_UNDO_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "song.h"

/*----- Macros -------------------------------------------------------*/

#define UNDO_LEVELS 5

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    t_pattern pattern;
    uint8_t pattern_index;
} t_undo_entry;

typedef struct {
    t_undo_entry entry[UNDO_LEVELS];
    uint8_t head;  // Next slot to write.
    uint8_t count; // Number of valid entries.
} t_undo_stack;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void undo_init(t_undo_stack *undo);

/**
 * @brief   Snapshot the given pattern before a destructive edit.
 */
void undo_push(t_undo_stack *undo, const t_song *song, uint8_t pattern);

/**
 * @brief   Restore the most recent snapshot.
 *
 * @return  True if an edit was undone.
 */
bool undo_pop(t_undo_stack *undo, t_song *song);

/**
 * @brief   Discard all history (e.g. after load).
 */
void undo_clear(t_undo_stack *undo);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_UNDO_H */

/*----- End of file --------------------------------------------------*/
