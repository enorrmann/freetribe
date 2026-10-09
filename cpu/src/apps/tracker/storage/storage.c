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
 * @file    storage.c
 *
 * @brief   Volatile song storage implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include <string.h>

#include "storage.h"

/*----- Macros -------------------------------------------------------*/

// Magic number guards against loading an unwritten slot.
#define STORAGE_MAGIC 0x54524b30 // "TRK0"

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    uint32_t magic;
    t_song song;
} t_storage_slot;

/*----- Static variable definitions ----------------------------------*/

static t_storage_slot g_slots[STORAGE_SLOTS];

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void storage_init(void) { memset(g_slots, 0, sizeof(g_slots)); }

e_storage_status storage_save(uint8_t slot, const t_song *song) {

    if (slot >= STORAGE_SLOTS || song == NULL) {
        return STORAGE_ERROR;
    }

    g_slots[slot].song = *song;
    g_slots[slot].magic = STORAGE_MAGIC;

    return STORAGE_OK;
}

e_storage_status storage_load(uint8_t slot, t_song *song) {

    if (slot >= STORAGE_SLOTS || song == NULL) {
        return STORAGE_ERROR;
    }

    if (g_slots[slot].magic != STORAGE_MAGIC) {
        return STORAGE_EMPTY;
    }

    *song = g_slots[slot].song;

    return STORAGE_OK;
}

bool storage_slot_valid(uint8_t slot) {

    if (slot >= STORAGE_SLOTS) {
        return false;
    }

    return g_slots[slot].magic == STORAGE_MAGIC;
}

const char *storage_status_text(e_storage_status status) {

    switch (status) {

    case STORAGE_OK:
        return "OK";

    case STORAGE_EMPTY:
        return "EMPTY";

    case STORAGE_ERROR:
    default:
        return "ERR";
    }
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
