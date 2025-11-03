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
#include "sync.h"
#include "gui_task.h"
#define BUTTON_EXIT 0x0d
#define BUTTON_PLAY_PAUSE 0x02

static int g_midi_running = 0;

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/



static volatile uint32_t midi_clock_counter = 0;
void _button_callback(uint8_t index, bool state);
 static void _tick_callback(void) ;

static int pulse_period_ms = 60000 / (SYNC_INTERNAL_BPM  * SYNC_PPQN );

static void on_midi_clock(char ch, char a, char b) {
//    midi_clock_counter++;
    static int count = 0;
    static int sent_on_count = 0;
    count ++;

    // enviar pulso si corresponde
    if (count == 1 || ((count - 1) % (MIDI_SYNC_PPQN / SYNC_PPQN) == 0)) {
        send_sync_out_pulse_start();
        sent_on_count = count;
    } else 
    if (count > sent_on_count ) {
        send_sync_out_pulse_end();

    }

    // reiniciar cada beat
    if (count >= MIDI_SYNC_PPQN) {
        count = 0;
    }
}

static void process_tempo_tick() {
    static int g_toggle_led = 0;

    if (midi_clock_counter >= 24) { // midi syn is 24 ppqn
        midi_clock_counter = 0;
        g_toggle_led = 1;
        
        if (g_midi_running && g_toggle_led) {
            ft_toggle_led(LED_TAP);

            g_toggle_led = 0;
        }
        
    }
}

static void on_midi_start(char ch, char a, char b) {
    (void)ch;
    (void)a;
    (void)b;
    midi_clock_counter = 12;
    ft_print("on_midi_start\n");
    g_midi_running = 1;
}

static void on_midi_stop(char ch, char a, char b) {
    (void)ch;
    (void)a;
    (void)b;
    // detener reproducción, etc.
    ft_print("on_midi_stop\n");
    g_midi_running = 0;
    ft_set_led(LED_TAP,0);
}

/*----- Static variable definitions ----------------------------------*/

static t_delay_state g_blink_delay;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

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

    midi_init_fsm();
    midi_register_event_handler(EVT_SYS_REALTIME_TIMING_CLOCK, on_midi_clock);
  //  midi_register_event_handler(EVT_SYS_REALTIME_SEQ_START, on_midi_start);
   // midi_register_event_handler(EVT_SYS_REALTIME_SEQ_STOP, on_midi_stop);

    ft_register_panel_callback(BUTTON_EVENT, _button_callback);
    ft_register_tick_callback(0, _tick_callback);
gui_task();
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
    gui_task();
  //  process_tempo_tick(); 

 }

 static void _tick_callback(void) {

   //send_sync_out(SYNC_INTERNAL_BPM,SYNC_PPQN);
    //send_sync_out2(pulse_period_ms); 
    //check_sync_out_pulse_end(); /// not used
    poll_sync_gpio();
    send_sync_out_midi  ();

    static int counter = 0;
    counter++;
    if (counter >= 1000) { // cada 4 negras
        print_bpm();
        counter = 0;    
    }


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
    case BUTTON_PLAY_PAUSE:
        if (state == 1) {
            
        }
        break;

    default:
        break;
    }
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
