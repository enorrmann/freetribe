
#include "lut.h"
#include <stdint.h>

#include "param_scale.h"

float g_midi_pitch_cv_lut[128];
float g_amp_cv_lut[256];
float g_knob_cv_lut[256];

float g_midi_hz_lut[128];
float g_octave_tune_lut[256];
float g_filter_res_lut[256];

void lut_init() {

    int i;
    float scaled;

    for (i = 0; i <= 127; i++) {

        g_midi_pitch_cv_lut[i] = note_to_cv(i);
    }

    for (i = 0; i <= 255; i++) {

        scaled = i / 255.0;
        g_amp_cv_lut[i] = powf(scaled, 2);
    }

    for (i = 0; i <= 255; i++) {
        g_knob_cv_lut[i] = i / 255.0;
    }

    /// TODO: Should be log.
    //
    // Initialise pitch mod lookup table.
    float tune;
    for (i = 0; i <= 255; i++) {

        if (i <= 128) {
            // 0.5...1.
            tune = (i / 256.0) + 0.5;

        } else {
            // >1...2.0
            tune = ((i - 128) / 127.0) + 1;
        }

        // // Convert to fix16,
        // tune *= (1 << 16);
        // g_octave_tune_lut[i] = (int32_t)tune;

        g_octave_tune_lut[i] = tune;
    }

    int32_t res;
    for (i = 0; i <= 255; i++) {

        res = 0x7fffffff - (i * (1 << 23));

        g_filter_res_lut[i] = res;
    }
}