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
 * @file    tracker_view.h
 *
 * @brief   Renders the tracker screen.
 *
 * The view is a pure function of view state: it never mutates the song or
 * the cursor.  It owns the pixel-level presentation of the pattern grid,
 * the header and the status line.
 */

#ifndef TRACKER_VIEW_H
#define TRACKER_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "cursor.h"
#include "song.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   Everything the view needs to draw one frame.
 */
typedef struct {
    const t_song *song;
    const t_cursor *cursor;
    const char *message; // Transient status message (may be NULL).

    bool playing;        // Transport running.
    uint8_t play_row;    // Current playing row.
    uint8_t play_pattern; // Current playing pattern.

    bool insert;         // Insert mode indicator.
    bool recording;      // Record mode indicator.
    uint8_t slot;        // Active song slot (for the header).
} t_view_state;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

/**
 * @brief   Draw a complete frame.
 */
void tracker_view_render(const t_view_state *view);

/**
 * @brief   Blank the screen once at start-up.
 */
void tracker_view_clear(void);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_VIEW_H */

/*----- End of file --------------------------------------------------*/
