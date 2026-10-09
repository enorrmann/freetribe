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
 * @file    panel_buttons.h
 *
 * @brief   Electribe 2 panel control indices used by the tracker.
 *
 * These values are the `index` argument delivered to the Freetribe panel
 * callbacks.  They mirror the hardware and are shared by the input and
 * LED feedback code so both sides stay in sync.
 */

#ifndef TRACKER_PANEL_BUTTONS_H
#define TRACKER_PANEL_BUTTONS_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

/*----- Macros -------------------------------------------------------*/

// Transport controls.
#define BUTTON_RECORD 0x00
#define BUTTON_STOP 0x01
#define BUTTON_PLAY_PAUSE 0x02
#define BUTTON_TAP 0x03

// Performance controls.
#define BUTTON_GATE_ARP 0x04
#define BUTTON_TOUCH_SCALE 0x05
#define BUTTON_MFX 0x06
#define BUTTON_MFX_HOLD 0x07

// Navigation controls.
#define BUTTON_BACK 0x08
#define BUTTON_MENU 0x09
#define BUTTON_SHIFT 0x0a
#define BUTTON_LEFT 0x0b
#define BUTTON_FORWARD 0x0c
#define BUTTON_EXIT 0x0d
#define BUTTON_WRITE 0x0e
#define BUTTON_RIGHT 0x0f

// Edit controls.
#define BUTTON_MUTE 0x10
#define BUTTON_ERASE 0x11

// Filter controls.
#define BUTTON_LPF 0x12
#define BUTTON_HPF 0x14
#define BUTTON_BPF 0x16

// Mode controls.
#define BUTTON_TRIGGER 0x13
#define BUTTON_SEQUENCER 0x15
#define BUTTON_KEYBOARD 0x17

// Function controls.
#define BUTTON_CHORD 0x18
#define BUTTON_STEP_JUMP 0x19
#define BUTTON_MFX_SEND 0x1a
#define BUTTON_PATTERN_SET 0x1b

// Bar selection.
#define BUTTON_BAR_1 0x1c
#define BUTTON_BAR_2 0x1d
#define BUTTON_BAR_3 0x1e
#define BUTTON_BAR_4 0x1f

// Additional controls.
#define BUTTON_AMP_EG 0x20
#define BUTTON_IFX_ON 0x21

// Encoders.
#define ENCODER_MAIN 0x00
#define ENCODER_OSC 0x01
#define ENCODER_CUTOFF 0x02
#define ENCODER_MOD 0x03
#define ENCODER_IFX 0x04

// Knobs.
#define KNOB_LEVEL 0x00
#define KNOB_PAN 0x01
#define KNOB_PITCH 0x02
#define KNOB_RESONANCE 0x03
#define KNOB_EG_INT 0x04
#define KNOB_MOD_DEPTH 0x05
#define KNOB_ATTACK 0x06
#define KNOB_IFX 0x07
#define KNOB_DECAY 0x08
#define KNOB_OSC_EDIT 0x09
#define KNOB_MOD_SPEED 0x0a

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_PANEL_BUTTONS_H */

/*----- End of file --------------------------------------------------*/
