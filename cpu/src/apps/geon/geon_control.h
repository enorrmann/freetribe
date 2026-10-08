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
 * @file    geon_control.h
 *
 * @brief   Public API for Geonkick control, evaluated on the CPU.
 *
 * One control layer per trigger pad.  Every instrument is evaluated at
 * CONTROL_RATE and its values are sent to the DSP only when they
 * change, as in the monosynth example.
 */

#ifndef GEON_CONTROL_H
#define GEON_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "geon_envelope.h"
#include "geon_interface.h"

/*----- Macros -------------------------------------------------------*/

#define GEON_OSCILLATORS_MAX GEON_OSCILLATORS

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   Per oscillator state, held on the CPU.
 */
typedef struct {
    bool enabled;
    e_geon_osc_func func;
    float frequency;
    float amplitude;
    float noise_density;
    bool is_fm;

    /* Pitch envelope end value, the Geonkick pitch sweep amount. */
    float pitch_amount;

    bool filter_enabled;
    e_geon_filter_type filter_type;
    float filter_cutoff;
    float filter_q;

    /* Envelope points, evaluated here. */
    t_geon_envelope env_frequency;
    t_geon_envelope env_amplitude;
    t_geon_envelope env_noise_density;

    /* Last values sent to the DSP. */
    float last_frequency;
    float last_amplitude;
    float last_noise_density;
    float last_filter_cutoff;
} t_geon_osc_control;

/**
 * @brief   State of one instrument, held on the CPU.
 */
typedef struct {
    bool active;
    bool note_on;

    float length;
    float time;

    /* Amplitude and drive envelopes, evaluated here. */
    t_geon_envelope env_amplitude;

    float amplitude;
    float distortion_drive;
    float distortion_volume;
    bool distortion_enabled;

    bool filter_enabled;
    e_geon_filter_type filter_type;
    float filter_cutoff;
    float filter_q;

    t_geon_osc_control osc[GEON_OSCILLATORS_MAX];

    /* Last values sent to the DSP. */
    float last_amplitude;
    float last_filter_cutoff;
} t_geon_control;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void geon_control_init(void);

/**
 * @brief   Evaluate all sounding instruments, at CONTROL_RATE.
 */
void geon_control_process(void);

t_geon_control *geon_control_get(uint8_t instrument);

/**
 * @brief   Start an instrument and send its configuration to the DSP.
 */
void geon_control_note_on(uint8_t instrument);

void geon_control_note_off(uint8_t instrument);

void geon_control_all_notes_off(void);

/**
 * @brief   Send the whole configuration of an instrument to the DSP.
 */
void geon_control_send_all(uint8_t instrument);

#ifdef __cplusplus
}
#endif
#endif /* GEON_CONTROL_H */

/*----- End of file --------------------------------------------------*/
