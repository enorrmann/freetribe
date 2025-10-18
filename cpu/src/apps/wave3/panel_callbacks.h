#ifndef PANEL_CALLBACKS_H
#define PANEL_CALLBACKS_H

#include <stdint.h>
#include <stdbool.h>
#include "panel_buttons.h"
#include "common/parameters.h"
#include "freetribe.h"
#include "module_interface.h"
#include "gui_task.h"
#include "autosampler.h"
#include "lut.h"


void PANEL_button_callback(uint8_t index, bool state) ;
void PANEL_knob_callback(uint8_t index, uint8_t value);
void PANEL_encoder_callback(uint8_t index, uint8_t value) ;
void PANEL_set_current_sample(int sample_number) ;
void PANEL_init();

#endif