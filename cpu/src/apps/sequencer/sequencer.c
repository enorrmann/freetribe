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
#include "event_seq.h"

#define BUTTON_EXIT 0x0d

#define MIDI_PPQN 24

void _button_callback(uint8_t index, bool state);
static void _tick_callback(void) ;
void callback1();
void callback2();
static Sequencer my_sequencer;
 SeqEvent event1, event2, event3, event4;
static void simulate_midi_tick() ;

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/


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
    
    
    ft_register_panel_callback(BUTTON_EVENT, _button_callback);
    
    ft_register_tick_callback(0, _tick_callback);

    // step sequencer
    /*SEQ_init(120.0f, 4, 4, 8, 16); // 15 BPM, 4/4, 1 note steps, 16 steps
    SEQ_fill_every_n(4);            // Activate every 4 steps
    SEQ_set_update_mode(SEQ_MODE_IMMEDIATE);
    SEQ_start();
    SEQ_set_step_callback(0, foo_callback);*/
    int ticks = 4 * MIDI_PPQN; //  beats times  24 PPQN
    event1.callback = callback1;
    event2.callback = callback2;
    event3.callback = callback1;
    event4.callback = callback1;

    SEQ_init(&my_sequencer, ticks);
    
    SEQ_add_event_at_timestamp(&my_sequencer, 0, &event1);
    SEQ_add_event_at_timestamp(&my_sequencer, 0.25*MIDI_PPQN, &event2);
    SEQ_add_event_at_timestamp(&my_sequencer, 1*MIDI_PPQN, &event3);
    SEQ_add_event_at_timestamp(&my_sequencer, 2*MIDI_PPQN, &event4);
    SEQ_start(&my_sequencer);

    ft_print("sequencer");

    return SUCCESS;
}

void callback1(){
    ft_print("callback1  \n");
    ft_toggle_led(LED_TAP);
}
void callback2(){
    ft_print("callback2 \n");
    ft_toggle_led(LED_TAP);
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


 static void _tick_callback(void) {
    simulate_midi_tick();
}



static void simulate_midi_tick() {
    static float count = 0.0f;

    const float bpm = 120.0f;
    const uint16_t midi_ppqn = 24;
    const float interval_ms = (60000.0f / bpm) / MIDI_PPQN; // tiempo entre ticks MIDI

    count += 1.0f; // se llama cada 1 ms

    if (count >= interval_ms) {
        count -= interval_ms;  // mantiene la fase
        SEQ_tick(&my_sequencer);          // simula un pulso MIDI PPQN
    }
}
/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
