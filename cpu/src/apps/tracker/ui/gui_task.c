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
      but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
          See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2024

----------------------------------------------------------------------*/

/**
 * @file    gui_task.c
 *
 * @brief   Generic GUI task and drawing primitives.
 */

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freetribe.h"

#include "gui_task.h"

#include "ugui.h"

/*----- Macros -------------------------------------------------------*/

#define GUI_MESSAGE_LEN 21
#define GUI_MESSAGE_LINES 1

/*----- Typedefs -----------------------------------------------------*/

typedef enum { GUI_STATE_INIT, GUI_STATE_RUN, GUI_STATE_ERROR } t_gui_state;

/*----- Static variable definitions ----------------------------------*/

static UG_GUI g_gui;
static UG_DEVICE g_device;

static t_gui_state g_state = GUI_STATE_INIT;

static char g_message[GUI_MESSAGE_LEN];

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _put_pixel(UG_S16 x, UG_S16 y, UG_COLOR c);
static void _init(void);
static void _apply_colours(bool inverted);

/*----- Extern function implementations ------------------------------*/

void gui_task(void) {

    if (g_state == GUI_STATE_INIT) {
        _init();
        g_state = GUI_STATE_RUN;
    }
}

void gui_print(uint8_t x, uint8_t y, const char *text) {

    if (text == NULL) {
        return;
    }

    UG_PutString(x, y, (char *)text);
}

void gui_print_int(uint8_t x, uint8_t y, int32_t value) {

    char text[12];

    snprintf(text, sizeof(text), "%ld", (long)value);
    UG_PutString(x, y, text);
}

void gui_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                   bool colour) {

    UG_DrawLine(x0, y0, x1, y1, colour ? 0xff : 0x00);
}

void gui_fill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool colour) {

    UG_FillFrame(x0, y0, x1, y1, colour ? 0xff : 0x00);
}

void gui_pixel(uint8_t x, uint8_t y, bool colour) {

    UG_DrawPixel(x, y, colour ? 0xff : 0x00);
}

void gui_clear(bool colour) { UG_FillScreen(colour ? 0xff : 0x00); }

void gui_set_inverted(bool inverted) { _apply_colours(inverted); }

uint8_t gui_char_width(void) { return GUI_CHAR_W; }

void gui_message(const char *text) {

    size_t len;

    if (text == NULL) {
        return;
    }

    len = strlen(text);

    if (len >= GUI_MESSAGE_LEN) {
        len = GUI_MESSAGE_LEN - 1;
    }

    memset(g_message, 0, sizeof(g_message));
    memcpy(g_message, text, len);
}

/*----- Static function implementations ------------------------------*/

static void _init(void) {

    g_device.x_dim = GUI_WIDTH;
    g_device.y_dim = GUI_HEIGHT;
    g_device.pset = _put_pixel;

    UG_Init(&g_gui, &g_device);

    // Use FONT_6X8 for a compact, legible tracker grid.
    UG_FontSelect(FONT_6X8);

    UG_SetForecolor(0xff);
    UG_SetBackcolor(0x00);

    memset(g_message, 0, sizeof(g_message));

    // Blank the display on entry.
    UG_FillScreen(0x00);
}

static void _apply_colours(bool inverted) {

    if (inverted) {
        UG_SetForecolor(0x00);
        UG_SetBackcolor(0xff);
    } else {
        UG_SetForecolor(0xff);
        UG_SetBackcolor(0x00);
    }
}

/**
 * @brief   Pixel sink for uGUI.
 *
 * The panel framebuffer stores a set bit for a lit pixel, so invert the
 * uGUI colour value as we write it.
 */
static void _put_pixel(UG_S16 x, UG_S16 y, UG_COLOR c) {

    ft_put_pixel(x, y, !c);
}

/*----- End of file --------------------------------------------------*/
