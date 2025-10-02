#ifndef PANEL_BUTTONS_H
#define PANEL_BUTTONS_H 


// Transport Controls
#define BUTTON_RECORD        0x00
#define BUTTON_STOP       0x01
#define BUTTON_PLAY_PAUSE 0x02
#define BUTTON_TAP        0x03

// Performance Controls
#define BUTTON_GATE_ARP    0x04
#define BUTTON_TOUCH_SCALE 0x05
#define BUTTON_MFX         0x06
#define BUTTON_MFX_HOLD    0x07

// Navigation Controls
#define BUTTON_BACK    0x08
#define BUTTON_MENU    0x09
#define BUTTON_SHIFT   0x0A
#define BUTTON_LEFT    0x0B
#define BUTTON_FORWARD 0x0C
#define BUTTON_EXIT    0x0D
#define BUTTON_WRITE   0x0E
#define BUTTON_RIGHT   0x0F

// Edit Controls
#define BUTTON_MUTE  0x10
#define BUTTON_ERASE 0x11

// Filter Controls
#define BUTTON_LPF 0x12
#define BUTTON_HPF 0x14
#define BUTTON_BPF 0x16

// Mode Controls
#define BUTTON_TRIGGER   0x13
#define BUTTON_SEQUENCER 0x15
#define BUTTON_KEYBOARD  0x17

// Function Controls
#define BUTTON_CHORD       0x18
#define BUTTON_STEP_JUMP   0x19
#define BUTTON_MFX_SEND    0x1A
#define BUTTON_PATTERN_SET 0x1B

// Bar Selection
#define BUTTON_BAR_1 0x1C
#define BUTTON_BAR_2 0x1D
#define BUTTON_BAR_3 0x1E
#define BUTTON_BAR_4 0x1F

// Additional Controls
#define BUTTON_AMP_EG 0x20
#define BUTTON_IFX_ON 0x21

// Encoders
#define ENCODER_MAIN   0x00
#define ENCODER_OSC    0x01
#define ENCODER_CUTOFF 0x02
#define ENCODER_MOD    0x03
#define ENCODER_IFX    0x04

// Knobs
#define KNOB_LEVEL     0x00
#define KNOB_PAN       0x01
#define KNOB_PITCH     0x02
#define KNOB_RESONANCE 0x03
#define KNOB_EG_INT    0x04
#define KNOB_MOD_DEPTH 0x05
#define KNOB_ATTACK    0x06
#define KNOB_IFX       0x07
#define KNOB_DECAY     0x08
#define KNOB_OSC_EDIT  0x09
#define KNOB_MOD_SPEED 0x0A

#endif // PANEL_BUTTONS_H