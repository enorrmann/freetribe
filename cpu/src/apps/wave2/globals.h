#ifndef GLOBALS_H
#define GLOBALS_H   

#define MEMPOOL_SIZE (0x1000)

#include "leaf.h"
#include "keyboard.h"
#include "module_interface.h"

static LEAF g_leaf;
static char g_mempool[MEMPOOL_SIZE];
static t_keyboard g_kbd;
static t_scale g_scale;
static e_mod_type g_mod_type;
static bool g_amp_eg;
static bool g_retrigger;
static float g_midi_pitch_cv_lut[128];
static float g_amp_cv_lut[256];
static float g_knob_cv_lut[256];
static float g_midi_hz_lut[128];
static float g_octave_tune_lut[256];
static float g_filter_res_lut[256];
static int g_current_editing_sample_parameter;
static int g_current_editing_sample_parameter_value[SAMPLE_PARAM_COUNT];

static bool g_shift_held;
static bool g_menu_held;

static bool g_button_bar_1_held;
static bool g_button_bar_2_held;
static bool g_button_bar_3_held;
static bool g_button_bar_4_held;


#endif