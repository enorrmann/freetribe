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

/*----- Includes -----------------------------------------------------*/

#include "freetribe.h"
#include "keyboard.h"


#define BUTTON_EXIT 0x0d

#define DEFAULT_SCALE_NOTES NOTES_PHRYGIAN_DOMINANT
#define DEFAULT_SCALE_TONES 12
#define DEFAULT_SCALE_MODE 0

uint32_t prog [] =  {1060581412, 1060646948, 1094333734, 1127886375, 1128019752, 1144796457, 1161573673, 1178482475, 1009986592, 1010119457, 1043672610, 1043805475};

uint32_t pack_chord_notes(uint8_t n1, uint8_t n2, uint8_t n3, uint8_t n4) {
    return ((uint32_t)n1)
         | ((uint32_t)n2 << 8)
         | ((uint32_t)n3 << 16)
         | ((uint32_t)n4 << 24);
}

void unpack_chord_notes(uint32_t chord, uint8_t *n1, uint8_t *n2, uint8_t *n3, uint8_t *n4) {
    *n1 = chord & 0xFF;
    *n2 = (chord >> 8) & 0xFF;
    *n3 = (chord >> 16) & 0xFF;
    *n4 = (chord >> 24) & 0xFF;
}

char* int_to_char(int32_t value) {
    // buffer estático para almacenar el resultado (-2147483648 = 11 chars + '\0')
    static char str_buf[12];

    // conversión usando itoa, base 10 (decimal)
    itoa(value, str_buf, 10);

    // devolver puntero al buffer resultante
    return str_buf;
}

static t_keyboard g_kbd;
static t_scale g_scale;
void _button_callback(uint8_t index, bool state) ;
static void _trigger_callback(uint8_t pad, uint8_t vel, bool state) {

    uint8_t note;
    int chan = 0;
    

    note = keyboard_map_note(&g_kbd, pad);
    uint32_t chord = prog[pad];
    uint8_t n1,n2,n3,n4;
    unpack_chord_notes(chord, &n1, &n2, &n3, &n4);

    if (state) {
        ft_send_note_on(chan, n1, vel);
        ft_send_note_on(chan, n2, vel);
        ft_send_note_on(chan, n3, vel);
        ft_send_note_on(chan, n4, vel);
        
    } else {
        ft_send_note_off(chan, n1, vel);
        ft_send_note_off(chan, n2, vel);
        ft_send_note_off(chan, n3, vel);
        ft_send_note_off(chan, n4, vel);
    
    }
}

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

// #include "midi_fsm.h"

/**
 * @brief   Initialise application.
 *
 * Initialise a delay.
 * Status is assumed successful.
 *
 * @return status   Status code indicating success:
 *                  - SUCCESS
 *                  - WARNING
 *                  - ERROR
 */
t_status app_init(void) {
    scale_init(&g_scale, DEFAULT_SCALE_NOTES, DEFAULT_SCALE_TONES);
keyboard_init(&g_kbd, &g_scale);
    ft_register_panel_callback(BUTTON_EVENT, _button_callback);
ft_register_panel_callback(TRIGGER_EVENT, _trigger_callback);
    ft_print("inicializ");

    return SUCCESS;
}

/**
 * @brief   Run application.
 *
 * Test if 0.5 seconds have passed  since we started the delay.
 * If so, toggle an LED and restart delay.
 */
void app_run(void) { 
    

}

/*----- Static function implementations ------------------------------*/

/**
 * @brief   Callback triggered by panel button events.
 *
 * @param[in]   index   Index of button.
 * @param[in]   state   State of button.
 */
void _button_callback(uint8_t index, bool state) {

    switch (index) {

    case BUTTON_EXIT:
        if (state == 1) {
            ft_shutdown();
        }
        break;

    default:
        break;
    }
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
