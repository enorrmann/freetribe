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

#include "event_seq.h"
#include "freetribe.h"
#include "panel_buttons.h"
#include "seq_event_pool.h"

void _button_callback(uint8_t index, bool state);
static void _tick_callback(void);
void callback1(int chan, int note, int vel);
void callback2(int chan, int note, int vel);

static void _note_on_callback(char chan, char note, char vel);
static void _note_off_callback(char chan, char note, char vel);
static void _trigger_callback(uint8_t pad, uint8_t vel, bool state);
static Sequencer my_sequencer;
SeqEventPool event_pool;

static void simulate_midi_tick();
void on_start_callback(int beat_index);
void on_stop_callback(int beat_index);
void on_record_toggle_callback(int recording_state);

void on_start_callback(int beat_index) { ft_set_led(LED_PLAY, 255); }

void on_stop_callback(int beat_index) {
    for (int ch = 0; ch < 16; ch++) {
        ft_send_cc(ch, 123, 0); // All Notes Off
    }
    ft_set_led(LED_PLAY, 0);
}

void on_record_toggle_callback(int recording_state) {
    ft_set_led(LED_REC, 255 * recording_state); // turn on/off record LED
}

static char *int_to_char(int32_t value) {
    // buffer estático para almacenar el resultado (-2147483648 = 11 chars +
    // '\0')
    static char str_buf[12];

    // conversión usando itoa, base 10 (decimal)
    itoa(value, str_buf, 10);

    // devolver puntero al buffer resultante
    return str_buf;
}

void beat_callback(uint32_t beat_index) {
    int pad_index = beat_index % 16;
    // pad 0 led LED_PAD_0_RED = 44
    int pad_0 = LED_PAD_0_BLUE;
    static int previous_pad = 0;
    if (previous_pad != 0) {
        ft_set_led(previous_pad, 0);
    }
    int current_pad = pad_0 + (pad_index * 2);
    previous_pad = current_pad;
    ft_set_led(current_pad, 255);

    // int led_bar_0 = LED_BAR_0_BLUE;
    int led_bar_0 = LED_BAR_0_RED;
    int led_bar_index = beat_index / 16;
    if (led_bar_index > 3) {
        led_bar_0 = LED_BAR_0_BLUE;
    }
    led_bar_index = led_bar_index % 4;

    static int previous_led_bar = 0;
    if (previous_led_bar != 0) {
        ft_set_led(previous_led_bar, 0);
    }
    int current_led_bar = led_bar_0 + led_bar_index;
    previous_led_bar = current_led_bar;
    ft_set_led(current_led_bar, 255);

    /*ft_print("Pad ");
    ft_print(int_to_char(pad_index));
    ft_print(" step ");
    ft_print(int_to_char(beat_index));
    ft_print(" bar ");
    ft_print(int_to_char(current_led_bar));
    ft_print(" \n");*/
}

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
    ft_register_midi_callback(EVT_CHAN_NOTE_ON, _note_on_callback);
    ft_register_midi_callback(EVT_CHAN_NOTE_OFF, _note_off_callback);
    ft_register_panel_callback(TRIGGER_EVENT, _trigger_callback);

    ft_register_tick_callback(0, _tick_callback);

    SEQ_POOL_init(&event_pool);
    SeqEvent *event1 = SEQ_POOL_get_event(&event_pool);
    if (!event1) {
        ft_print("No event1\n");
    } else {
    }
    SeqEvent *event2 = SEQ_POOL_get_event(&event_pool);
    SeqEvent *event3 = SEQ_POOL_get_event(&event_pool);
    SeqEvent *event4 = SEQ_POOL_get_event(&event_pool);

    struct MidiEventParams mep;

    mep.chan = 0;
    mep.data1 = 72;  // C4
    mep.data2 = 100; // velocity

    event1->midi_event_callback = ft_send_note_on;
    event2->midi_event_callback = ft_send_note_off;
    event3->midi_event_callback = ft_send_note_on;
    event4->midi_event_callback = ft_send_note_off;

    event1->callback = callback1;
    event2->callback = callback2;
    event3->callback = callback1;
    event4->callback = callback2;

    event1->midi_params = mep;
    event2->midi_params = mep;
    event3->midi_params = mep;
    event4->midi_params = mep;

    int sequencer_beats = 4; // negras / quarter notes
    SEQ_init(&my_sequencer, sequencer_beats);
    SEQ_set_beat_callback(&my_sequencer, beat_callback);
    my_sequencer.on_start_callback = on_start_callback;
    my_sequencer.on_stop_callback = on_stop_callback;
    my_sequencer.on_record_toggle_callback = on_record_toggle_callback;

    // "metronome"
    SEQ_record_toggle(&my_sequencer); // start in recording mode
    SEQ_add_event_at_timestamp(&my_sequencer, 0, event1);
    SEQ_add_event_at_timestamp(&my_sequencer, 0.1f * MIDI_PPQN, event2);
    SEQ_add_event_at_timestamp(&my_sequencer, 1 * MIDI_PPQN, event3);
    SEQ_add_event_at_timestamp(&my_sequencer, 1.1 * MIDI_PPQN, event4);
    SEQ_record_toggle(&my_sequencer); // stop

    ft_print("sequencer");

    return SUCCESS;
}

void callback1(int chan, int note, int vel) { ft_set_led(LED_TAP, 255); }

void callback2(int chan, int note, int vel) { ft_set_led(LED_TAP, 0); }

/**
 * @brief   Run application.
 *
 * Test if 0.5 seconds have passed  since we started the delay.
 * If so, toggle an LED and restart delay.
 */
void app_run(void) {}

/*----- Static function implementations ------------------------------*/

/**
 * @brief   Callback triggered by panel button events.
 *
 * @param[in]   index   Index of button.
 * @param[in]   state   State of button.
 */
void _button_callback(uint8_t index, bool state) {

    switch (index) {

    case BUTTON_RECORD:
        if (state == 1) {
            SEQ_record_toggle(&my_sequencer);
        }
        break;
    case BUTTON_PLAY_PAUSE:
        if (state == 1) {
            SEQ_start(&my_sequencer);
        }
        break;
    case BUTTON_STOP:
        SEQ_stop(&my_sequencer);
        break;

    case BUTTON_EXIT:
        if (state == 1) {
            on_stop_callback(0); // send all notes off before shutdown
            ft_shutdown();
        }
        break;
    case BUTTON_ERASE:
        if (state == 1) {
            SEQ_clear(&my_sequencer);
            SEQ_POOL_init(&event_pool);
        }
        break;
    default:
        break;
    }
}

static void _tick_callback(void) { simulate_midi_tick(); }

static void simulate_midi_tick() {
    static float count = 0.0f;

    const float bpm = 60.0f;
    const float interval_ms =
        (60000.0f / bpm) / MIDI_PPQN; // tiempo entre ticks MIDI

    count += 1.0f; // se llama cada 1 ms

    if (count >= interval_ms) {
        count -= interval_ms;    // mantiene la fase
        SEQ_tick(&my_sequencer); // simula un pulso MIDI PPQN
    }
}

/**
 * @brief   Callback triggered by MIDI note on events.
 *
 * Echo received note on messages to MIDI output.
 *
 * @param[in]   chan    MIDI channel.
 * @param[in]   note    MIDI note number.
 * @param[in]   vel     MIDI note velocity.
 */
static void _note_on_callback(char chan, char note, char vel) {
    ft_send_note_on(chan, note, vel);
    MidiEventParams mep;
    mep.chan = chan;
    mep.data1 = note;
    mep.data2 = vel;
    mep.note_on = true;
    SeqEvent *event = SEQ_POOL_get_event(&event_pool);
    // thru
    if (!event) {
        ft_print("No event\n");
        return;
    }
    event->midi_event_callback = ft_send_note_on;
    event->midi_params = mep;
    // SEQ_add_event(&my_sequencer, event);
    SEQ_insert_before_current(&my_sequencer, event);
}

/**
 * @brief   Callback triggered by MIDI note off events.
 *
 * Echo received note on messages to MIDI output.
 *
 * @param[in]   chan    MIDI channel.
 * @param[in]   note    MIDI note number.
 * @param[in]   vel     MIDI note velocity.
 */
static void _note_off_callback(char chan, char note, char vel) {
    ft_send_note_off(chan, note, vel);
    MidiEventParams mep;
    mep.chan = chan;
    mep.data1 = note;
    mep.data2 = vel;
    mep.note_on = false;
    SeqEvent *event = SEQ_POOL_get_event(&event_pool);
    // thru
    if (!event) {
        ft_print("No event\n");
        return;
    }

    event->midi_event_callback = ft_send_note_off;
    event->midi_params = mep;
    // SEQ_add_event(&my_sequencer, event);
    SEQ_insert_note_off(&my_sequencer, event);
}

// simulates keyboard
static void _trigger_callback(uint8_t pad, uint8_t vel, bool state) {
    if (state) {
        _note_on_callback(0, 60, 120);
    } else {
        _note_off_callback(0, 60, 120);
    }
}