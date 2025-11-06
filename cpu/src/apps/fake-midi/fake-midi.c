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

#define rising_edge_bank  7
#define rising_edge_pin   9
#define rising_edge_out_bank  6
#define rising_edge_out_pin  11

#define BUTTON_EXIT 0x0d
// --- MIDI output pin configuration ---
#define MIDI_OUT_BANK  rising_edge_out_bank
#define MIDI_OUT_PIN   rising_edge_out_pin

// --- Constants ---
#define MIDI_BAUD       31250
#define BIT_TIME_US     (1000000 / MIDI_BAUD) // 32 µs
#define TX_BUFFER_SIZE  64

// --- Transmit state ---
static volatile bool tx_active = false;
static volatile uint8_t tx_byte = 0;
static volatile uint8_t tx_bit_index = 0;
static volatile uint32_t bit_timer = 0;
static volatile uint8_t stop_bits = 0;

// --- TX circular buffer ---
static volatile uint8_t tx_buffer[TX_BUFFER_SIZE];
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;


void _button_callback(uint8_t index, bool state);
static void _tick_callback(void) ;
void foo_callback();
void microtick(void) ;
// --- Internal helpers ---
static inline void midi_line_high(void) {
    per_gpio_set(MIDI_OUT_BANK, MIDI_OUT_PIN, 1);
}
static inline void midi_line_low(void) {
    per_gpio_set(MIDI_OUT_BANK, MIDI_OUT_PIN, 0);
}

// BPM configuration (change this to your tempo)
#define MIDI_BPM        120.0f
#define MIDI_CLOCKS_PER_BEAT  24

// Calculate ms per MIDI clock tick = (60s / BPM) * 1000 / 24
#define CLOCK_INTERVAL_MS  ((60000.0f / MIDI_BPM) / MIDI_CLOCKS_PER_BEAT)


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
    midi_line_high(); // idle state
    ft_print("fakemidi");

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


 static void _tick_callback(void) {
    static float clock_timer = 0.0f;

    // Run 1000 microticks = 1 ms
    for (int i = 0; i < 1000; i++) {
        microtick();
    }

    // Example: send MIDI Clock (0xF8) periodically based on BPM
    clock_timer += 1.0f; // add 1 ms
    if (clock_timer >= CLOCK_INTERVAL_MS) {
        clock_timer -= CLOCK_INTERVAL_MS;
        midi_send_byte(0xF8); // MIDI Clock
    }
}



// --- Enqueue a byte to send ---
void midi_send_byte(uint8_t b) {
    uint8_t next = (tx_head + 1) % TX_BUFFER_SIZE;
    if (next != tx_tail) {
        tx_buffer[tx_head] = b;
        tx_head = next;
    }
    // If idle, start immediately
    if (!tx_active) {
        tx_active = true;
        tx_byte = tx_buffer[tx_tail];
        tx_tail = (tx_tail + 1) % TX_BUFFER_SIZE;

        tx_bit_index = 0;
        stop_bits = 0;
        bit_timer = BIT_TIME_US;

        midi_line_low(); // start bit
    }
}

// --- Called every microsecond ---
void microtick(void) {
    if (!tx_active)
        return;

    if (--bit_timer == 0) {
        bit_timer = BIT_TIME_US;

        if (tx_bit_index < 8) {
            // Send data bits LSB first
            if (tx_byte & (1u << tx_bit_index)) {
                midi_line_high();
            } else {
                midi_line_low();
            }
            tx_bit_index++;
        } else if (stop_bits == 0) {
            // Stop bit
            midi_line_high();
            stop_bits = 1;
        } else {
            // Byte complete
            if (tx_tail != tx_head) {
                // Next byte available
                tx_byte = tx_buffer[tx_tail];
                tx_tail = (tx_tail + 1) % TX_BUFFER_SIZE;
                tx_bit_index = 0;
                stop_bits = 0;
                midi_line_low(); // start next start bit
            } else {
                // No more bytes
                tx_active = false;
                midi_line_high(); // idle
            }
        }
    }
}

