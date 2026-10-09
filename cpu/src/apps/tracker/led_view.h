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
 * @file    led_view.h
 *
 * @brief   Panel LED feedback for the tracker.
 *
 * LEDs are a cheap, glanceable mirror of the sequencer state: play/record
 * indicators, bar LEDs that follow the playhead, and pad LEDs that show
 * which channels are active in the current pattern.
 */

#ifndef TRACKER_LED_VIEW_H
#define TRACKER_LED_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "cursor.h"
#include "sequencer.h"
#include "song.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

/**
 * @brief   Update the play / stop / record LEDs.
 */
void led_view_update_transport(bool playing, bool recording);

/**
 * @brief   Light the bar LED that corresponds to the playing row.
 */
void led_view_update_playhead(uint8_t order_index);

/**
 * @brief   Light a pad LED per channel that has data in the pattern.
 */
void led_view_update_channels(const t_song *song, uint8_t pattern);

/**
 * @brief   Show the edit-step using the bar LEDs.
 */
void led_view_show_step(uint8_t step);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_LED_VIEW_H */

/*----- End of file --------------------------------------------------*/
