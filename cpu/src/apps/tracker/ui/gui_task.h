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
 * @file    gui_task.h
 *
 * @brief   Generic GUI task and drawing primitives.
 *
 * The GUI task owns a uGUI instance and a small ring buffer of drawing
 * events.  Producers enqueue events from any context; the main loop
 * drains them at a safe point.  Higher layers (tracker_view) build on the
 * primitives exposed here and never touch uGUI directly.
 */

#ifndef TRACKER_GUI_TASK_H
#define TRACKER_GUI_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

/*----- Macros -------------------------------------------------------*/

#define GUI_WIDTH 128
#define GUI_HEIGHT 64

// Text width in characters with FONT_6X8 (no horizontal spacing).
#define GUI_CHAR_W 6
#define GUI_CHAR_H 8

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

/**
 * @brief   Advance the GUI task: drain events and flush the display.
 *
 * Call every iteration of app_run().
 */
void gui_task(void);

/**
 * @brief   Draw a string at pixel position.
 */
void gui_print(uint8_t x, uint8_t y, const char *text);

/**
 * @brief   Draw a right-aligned integer, padded to `width` characters.
 */
void gui_print_int(uint8_t x, uint8_t y, int32_t value);

/**
 * @brief   Draw a horizontal line.
 */
void gui_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                   bool colour);

/**
 * @brief   Fill a rectangle with the given colour.
 */
void gui_fill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool colour);

/**
 * @brief   Draw a single pixel.
 */
void gui_pixel(uint8_t x, uint8_t y, bool colour);

/**
 * @brief   Fill the whole screen with the given colour.
 */
void gui_clear(bool colour);

/**
 * @brief   Invert the display (negative text mode).
 */
void gui_set_inverted(bool inverted);

/**
 * @brief   Number of pixels wide a character occupies.
 */
uint8_t gui_char_width(void);

/**
 * @brief   Console-style message (transient status line).
 */
void gui_message(const char *text);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_GUI_TASK_H */

/*----- End of file --------------------------------------------------*/
