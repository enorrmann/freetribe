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

                       Copyright bangcorrupt 2023

----------------------------------------------------------------------*/

/**
 * @file    gui_task.c
 *
 * @brief   Event handling for Freetribe GUI.
 */

/*----- Includes -----------------------------------------------------*/

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freetribe.h"

#include "gui_task.h"
// #include "gui_window.h"

#include "ring_buffer.h"
#include "ugui.h"

/*----- Macros -------------------------------------------------------*/

#define GUI_EVENT_BUF_LEN 0x20
#define GUI_MAX_STRING_LEN 19 // FONT_6X8

// #define GUI_MAX_STRING_LEN 24 // FONT_4X6

/*----- Typedefs -----------------------------------------------------*/

typedef enum { STATE_INIT, STATE_RUN, STATE_ERROR } t_gui_task_state;

typedef enum {
    GUI_PRINT_STRING,
    GUI_POST_STRING,
    GUI_DRAW_LINE,
    GUI_DRAW_FRAME,
    GUI_DRAW_CIRCLE,

    GUI_EVENT_COUNT
} e_gui_event;

typedef struct {
    e_gui_event type;
    uint8_t x_start;
    uint8_t y_start;
    uint8_t x_end;
    uint8_t y_end;
    bool colour;
    char text[GUI_MAX_STRING_LEN + 1];
} t_gui_event;

/*----- Static variable definitions ----------------------------------*/

static UG_GUI g_gui;
static UG_DEVICE g_device;

// GUI event ring buffer.
static rbd_t g_gui_rbd;
static char g_gui_rbmem[GUI_EVENT_BUF_LEN][sizeof(t_gui_event)];

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static t_status _init(void);

static void _run(void);

t_status _parse_event(t_gui_event *event);
static void _put_pixel(UG_S16 x, UG_S16 y, UG_COLOR c);

/*----- Extern function implementations ------------------------------*/

void gui_task(void) {

    static t_gui_task_state state = STATE_INIT;

    switch (state) {

    // Initialise gui task.
    case STATE_INIT:
        if (error_check(_init()) == SUCCESS) {
            state = STATE_RUN;
        }
        // Remain in INIT state until initialisation successful.
        break;

    case STATE_RUN:
        _run();
        break;

    case STATE_ERROR:
        error_check(UNRECOVERABLE_ERROR);
        break;

    default:
        if (error_check(UNHANDLED_STATE_ERROR) != SUCCESS) {
            state = STATE_ERROR;
        }
        break;
    }
}

/**
 * @brief   Print a string to the uGUI console.
 *
 * @param[in]   text    String to print.
 */
void gui_post(char *text) {

    t_gui_event event;

    event.type = GUI_POST_STRING;

    strncpy(event.text, text, GUI_MAX_STRING_LEN);

    ring_buffer_put_force(g_gui_rbd, &event);
}

/**
 * @brief   Print a string to the GUI.
 *
 * @param[in]   text    String to print.
 */
void gui_print(uint8_t x_start, uint8_t y_start, char *text) {

    t_gui_event event;

    event.type = GUI_PRINT_STRING;
    event.x_start = x_start;
    event.y_start = y_start;

    /// TODO: This is jank.
    //
    uint32_t length = (uint32_t)fmin(GUI_MAX_STRING_LEN, strlen(text)) + 1;

    strncpy(event.text, text, length);

    ring_buffer_put_force(g_gui_rbd, &event);
}

void gui_print_int(uint8_t x_start, uint8_t y_start, uint8_t value) {

    static char text[4];
    itoa(value, text, 10);
    text[3] = 0x00;

    gui_print(x_start, y_start, "   ");

    if (value < 100) {
        if (value < 10) {
            x_start +=
                (g_gui.currentFont.char_width * 2) + (g_gui.char_h_space * 2);
        } else {
            x_start += g_gui.currentFont.char_width + g_gui.char_h_space;
        }
    }

    gui_print(x_start, y_start, text);
}

void gui_post_label(const char *label) {
    char cat_string[GUI_MAX_STRING_LEN];

    // copiar el texto, truncando si es necesario
    strncpy(cat_string, label, GUI_MAX_STRING_LEN - 1);
    cat_string[GUI_MAX_STRING_LEN - 1] = '\0'; // asegurar terminación

    // agregar salto de línea si hay espacio
    strncat(cat_string, "\n", GUI_MAX_STRING_LEN - strlen(cat_string) - 1);

    gui_print(15, 15, "                  ");
    gui_print(15, 15, cat_string);
}

void gui_post_label_xy(const char *label, int x, int y) {

    int step = 1;
    char *colon = ":";
    int tick_in_step = 23;
    char *event_type = "n"; // note events
    // int note = 60;
    char *note = "C";
    char *flat = "b";
    char *space = " ";
    int octave = 2;
    int gate = 156;
    int vel = 120;
    int is_flat = 1;

    char cat_string[GUI_MAX_STRING_LEN];
    // char* template = "1:00mC 1 0:43 1056" ;
    // char* template =   "-................-" ;
    // strncpy(cat_string, template, GUI_MAX_STRING_LEN - 1);

    itoa(step, &cat_string[0], 10);
    strncpy(&cat_string[1], colon, 1);
    itoa(tick_in_step / 10, &cat_string[2], 10); //
    itoa(tick_in_step - (tick_in_step / 10 * 10), &cat_string[3], 10);

    strncpy(&cat_string[4], event_type, 1);
    strncpy(&cat_string[5], note, 1);
    strncpy(&cat_string[6], is_flat ? flat : space, 1); // flat indicator
    itoa(octave, &cat_string[7], 10);
    strncpy(&cat_string[8], space, 1);
    itoa(gate / 100, &cat_string[9], 10);
    itoa((gate - (gate / 100 * 100)) / 10, &cat_string[10], 10);
    itoa(gate - (gate / 10 * 10), &cat_string[11], 10);
    strncpy(&cat_string[12], space, 1);
    itoa(vel / 100, &cat_string[13], 10);
    itoa((vel - (vel / 100 * 100)) / 10, &cat_string[14], 10);
    itoa(vel - (vel / 10 * 10), &cat_string[15], 10);

    cat_string[GUI_MAX_STRING_LEN - 1] = '\0'; // asegurar terminación

    gui_print(x, y, cat_string);
}

void gui_post_param(const char *label, int32_t value) {
    // espacio suficiente para cualquier int32 (-2147483648 -> 11 chars + '\0')
    char val_string[12];
    itoa(value, val_string, 10);

    char cat_string[GUI_MAX_STRING_LEN];

    // copiar label truncando si hace falta
    strncpy(cat_string, label, GUI_MAX_STRING_LEN - 1);
    cat_string[GUI_MAX_STRING_LEN - 1] = '\0'; // aseguramos terminación

    // concatenar valor si entra
    strncat(cat_string, val_string,
            GUI_MAX_STRING_LEN - strlen(cat_string) - 1);

    // concatenar salto de línea si entra
    strncat(cat_string, "\n", GUI_MAX_STRING_LEN - strlen(cat_string) - 1);

    gui_post(cat_string);
}

char *int_to_char(int32_t value) {
    // buffer estático para almacenar el resultado (-2147483648 = 11 chars +
    // '\0')
    static char str_buf[12];

    // conversión usando itoa, base 10 (decimal)
    itoa(value, str_buf, 10);

    // devolver puntero al buffer resultante
    return str_buf;
}

void gui_draw_line(uint8_t x_start, uint8_t y_start, uint8_t x_end,
                   uint8_t y_end, bool colour) {

    t_gui_event event;

    event.type = GUI_DRAW_LINE;
    event.x_start = x_start;
    event.y_start = y_start;
    event.x_end = x_end;
    event.y_end = y_end;
    event.colour = colour;

    ring_buffer_put_force(g_gui_rbd, &event);
}

/*----- Static function implementations ------------------------------*/

static t_status _init(void) {

    t_status result = TASK_INIT_ERROR;

    // Rx ring buffer attributes.
    rb_attr_t rb_attr = {sizeof(g_gui_rbmem[0]), ARRAY_SIZE(g_gui_rbmem),
                         g_gui_rbmem};

    if (ring_buffer_init(&g_gui_rbd, &rb_attr) == SUCCESS) {

        g_device.x_dim = 128;
        g_device.y_dim = 64;
        g_device.pset = _put_pixel;
        //
        /// TODO: How does flush function work?

        // Initialise uGUI
        UG_Init(&g_gui, &g_device);

        /// TODO: Handle partial pages in fill_frame accelerator.
        //
        UG_DriverRegister(DRIVER_FILL_FRAME, ft_fill_frame);

        // Configure uGUI
        // UG_FontSelect(FONT_arial_6X6);
        UG_FontSelect(FONT_6X8);
        // UG_ConsoleSetArea(4, 4, 123, 12);
        UG_ConsoleSetArea(4, 32, 123, 40);
        // UG_ConsoleSetArea(4, 27, 123, 40);
        UG_ConsoleSetBackcolor(C_BLACK);
        UG_ConsoleSetForecolor(C_WHITE);
        UG_FillScreen(C_BLACK);

        // gui_window_main_init();
        // gui_window_main_show();
    }

    result = SUCCESS;

    return result;
}

static void _run(void) {

    t_gui_event event;

    UG_Update();

    if (ring_buffer_get(g_gui_rbd, &event) == SUCCESS) {
        _parse_event(&event);
    }
}

t_status _parse_event(t_gui_event *event) {

    t_status result = ERROR;

    switch (event->type) {

    case GUI_PRINT_STRING:
        UG_PutString(event->x_start, event->y_start, event->text);
        result = SUCCESS;
        break;

    case GUI_POST_STRING:
        UG_ConsolePutString(event->text);
        result = SUCCESS;
        break;

    case GUI_DRAW_LINE:
        UG_DrawLine(event->x_start, event->y_start, event->x_end, event->y_end,
                    event->colour);
        result = SUCCESS;
        break;

    case GUI_DRAW_FRAME:
        result = SUCCESS;
        break;

    case GUI_DRAW_CIRCLE:
        result = SUCCESS;
        break;

    default:
        result = WARNING;
        break;
    }

    return result;
}

/**
 * @brief   Put pixel functon for uGUI library.
 *
 * @param[in]   x   Horizontal position of pixel.
 * @param[in]   y   Vertical position of pixel.
 * @param[in]   c   Colour value, in this case 0 or 1.
 */
static void _put_pixel(UG_S16 x, UG_S16 y, UG_COLOR c) {

    ft_put_pixel(x, y, !c);
}

void win_callback(UG_MESSAGE *msg) { /* Handle window events here if needed */ }

/* GUI instance */
UG_GUI gui;

/* Window */
UG_WINDOW window;

/* Textbox */
UG_TEXTBOX textbox;

void window_init(void) {

    /* Create window */
    UG_WindowCreate(&window, win_callback, 1, "NULL");
    UG_WindowSetTitleText(&window, "Demo");

    /* Smaller textbox to fit in 63px height */
    /*UG_TextboxCreate(&window, &textbox, 1,
                     2, 12,
                     120, 30
    );*/
    // UG_TextboxSetFont(&window, 1, &FONT_8X12);
    // UG_TextboxSetText(&window, 1, "Hello uGUI");

    /* Show window */
    UG_WindowShow(&window);
}

/*----- End of file --------------------------------------------------*/
