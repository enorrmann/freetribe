
#include "sync.h"
#include "gui_task.h"
#include "per_gpio.h"

static int detections = 0;
static int total_detection_timer = 0;
static int sync_out_timer = 0;

static float midi_pulse_period_ms = 60000.0f / (SYNC_INTERNAL_BPM * MIDI_SYNC_PPQN);
static float sync_pulse_period_ms = 60000.0f / (SYNC_INTERNAL_BPM * SYNC_PPQN);

char *float_to_char(float value) {
    static char str_buf[20];
    int32_t int_part = (int32_t)value;
    int32_t frac_part;

    float frac = (value - (float)int_part);
    if (frac < 0)
        frac = -frac;
    frac_part = (int32_t)(frac * 1000.0f);

    char int_buf[12], frac_buf[6];
    itoa(int_part, int_buf, 10);
    itoa(frac_part, frac_buf, 10);

    char *p = str_buf;
    char *s = int_buf;
    while (*s)
        *p++ = *s++;
    *p++ = '.';
    s = frac_buf;
    while (*s)
        *p++ = *s++;
    *p = '\0';

    return str_buf;
}

void SYNC_send_sync_out() {

    static float count = 0.0f;
    count += 1.0f;

    if (count >= sync_pulse_period_ms - PULSE_LENGHT && count < sync_pulse_period_ms) {
        per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 1);
    }

    if (count >= sync_pulse_period_ms) {
        per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 0);
        count -= sync_pulse_period_ms;
    }
}

void SYNC_poll_sync_gpio() {
    static int last_detection_timer = 0;
    last_detection_timer++;

    static int cur_value = 0;
    int value = per_gpio_get(rising_edge_bank, rising_edge_pin);

    if (cur_value != value && last_detection_timer > PULSE_LENGHT) {
        cur_value = value;
        if (value == 1) {
            detections++;
            total_detection_timer += last_detection_timer;
            last_detection_timer = 0;
        }
    }
}

void SYNC_print_bpm() {
    float avg_ms_per_pulse = (float)total_detection_timer / detections;
    float bpm = 60000.0f / (avg_ms_per_pulse * SYNC_PPQN);
    gui_post_param("BPM Detected: ", (int32_t)bpm);

    total_detection_timer = 0;
    detections = 0;
}

void SYNC_send_sync_out_pulse_start() {
    per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 1);
    sync_out_timer = PULSE_LENGHT;
}

void SYNC_send_sync_out_pulse_end() {
    per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 0);
}

void SYNC_midi_send_clock() { svc_midi_send_byte(0xF8); }

void SYNC_midi_send_start() { svc_midi_send_byte(0xFA); }

void SYNC_midi_send_continue() { svc_midi_send_byte(0xFB); }

void SYNC_midi_send_stop() { svc_midi_send_byte(0xFC); }

void SYNC_send_sync_out_midi() {
    static float count = 0.0f;

    count += 1.0f;
    if (count >= midi_pulse_period_ms) {
        SYNC_midi_send_clock();
        count -= midi_pulse_period_ms;
    }
}