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
 * @file    storage.h
 *
 * @brief   Song save / load.
 *
 * Storage is currently volatile: slots live in RAM in a reserved array.
 * The interface is deliberately a small set of byte-oriented functions
 * so that it can be backed by flash (or an SD card) later without any
 * change to callers.  See docs/architecture.md for the migration plan.
 */

#ifndef TRACKER_STORAGE_H
#define TRACKER_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "song.h"

/*----- Macros -------------------------------------------------------*/

#define STORAGE_SLOTS 4 // Number of song slots.

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
    STORAGE_OK = 0,
    STORAGE_EMPTY, // Slot has never been written.
    STORAGE_ERROR,
} e_storage_status;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void storage_init(void);

/**
 * @brief   Store a song into a slot.
 */
e_storage_status storage_save(uint8_t slot, const t_song *song);

/**
 * @brief   Load a song from a slot.
 */
e_storage_status storage_load(uint8_t slot, t_song *song);

/**
 * @brief   True if the slot holds data.
 */
bool storage_slot_valid(uint8_t slot);

/**
 * @brief   Human readable status for the UI.
 */
const char *storage_status_text(e_storage_status status);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_STORAGE_H */

/*----- End of file --------------------------------------------------*/
