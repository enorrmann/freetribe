#ifndef LUT_H
#define LUT_H   

extern float g_midi_pitch_cv_lut[128];
extern float g_amp_cv_lut[256];
extern float g_knob_cv_lut[256];
extern float g_midi_hz_lut[128];
extern float g_octave_tune_lut[256];
extern float g_filter_res_lut[256];
void lut_init();
#endif