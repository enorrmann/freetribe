#include <chorus.h>
#include <math.h>

#define CHORUS_DELAY_SIZE 1024
#define CHORUS_MAX_DELAY 900
#define CHORUS_MIN_DELAY 600

typedef struct {
    fract32 delay_buffer[CHORUS_DELAY_SIZE];
    int write_index;
    fract32 lpf_state;
} chorus_channel_t;

typedef struct {
    chorus_channel_t left;
    chorus_channel_t right;
    int lfo_phase_L;
    int lfo_phase_R;
    int lfo_increment;
    fract32 depth;
    fract32 feedback;
    fract32 mix;
} chorus_state_t;

static chorus_state_t chorus_state;

static inline fract32 osc_next(uint16_t phase) {
    phase = phase & 0xFFFF;
    uint16_t quarter = phase >> 14;
    uint16_t offset = phase & 0x3FFF;
    int32_t value;
    switch (quarter) {
        case 0:
            value = offset << 17;
            break;
        case 1:
            value = (0x3FFF - offset) << 17;
            break;
        case 2:
            value = - (offset << 17);
            break;
        case 3:
            value = - ((0x3FFF - offset) << 17);
            break;
    }
    return (fract32)value;
}

void chorus_init(void) {
    int i;
    for (i = 0; i < CHORUS_DELAY_SIZE; i++) {
        chorus_state.left.delay_buffer[i] = 0;
        chorus_state.right.delay_buffer[i] = 0;
    }
    chorus_state.left.write_index = 0;
    chorus_state.right.write_index = 0;
    chorus_state.left.lpf_state = 0;
    chorus_state.right.lpf_state = 0;
    chorus_state.lfo_phase_L = 0;
    chorus_state.lfo_phase_R = 0x40000000;
    chorus_state.lfo_increment = 1;
    chorus_state.depth = 0x20000000;
    chorus_state.feedback = 0x20000000;
    chorus_state.mix = 0x30000000;
}

void chorus_set_params(fract32 depth, fract32 rate_multiplier, fract32 feedback, fract32 mix) {
    chorus_state.depth = depth;
    chorus_state.lfo_increment = mult_fr1x32x32(rate_multiplier, 0x0010) + 0x0004;
    chorus_state.feedback = feedback;
    chorus_state.mix = mix;
}

static fract32 _chorus_process(fract32 input, fract32 lfo, chorus_channel_t* channel) {
    fract32 depth_scaled = mult_fr1x32x32(lfo, chorus_state.depth);
    fract32 lfo_unipolar = (depth_scaled >> 1) + 0x40000000;
    int32_t delay_range = CHORUS_MAX_DELAY - CHORUS_MIN_DELAY;
    int64_t temp_delay = (int64_t)lfo_unipolar * delay_range;
    int32_t delay_int_part = (int32_t)(temp_delay >> 31);
    uint32_t delay_frac_part = (uint32_t)(temp_delay << 1) & 0xFFFF0000;
    int32_t total_delay_samples = CHORUS_MIN_DELAY + delay_int_part;
    int32_t read_index_base = channel->write_index - total_delay_samples;
    uint32_t idx0 = (read_index_base + CHORUS_DELAY_SIZE) % CHORUS_DELAY_SIZE;
    uint32_t idx1 = (idx0 + 1) % CHORUS_DELAY_SIZE;
    fract32 s0 = channel->delay_buffer[idx0];
    fract32 s1 = channel->delay_buffer[idx1];
    fract32 delayed = s0 + mult_fr1x32x32(delay_frac_part, (s1 - s0));
    const fract32 lpf_alpha = 0x59999999;
    delayed = add_fr1x32(mult_fr1x32x32(delayed, lpf_alpha), mult_fr1x32x32(channel->lpf_state, 0x7FFFFFFF - lpf_alpha));
    channel->lpf_state = delayed;
    fract32 feedback_signal = mult_fr1x32x32(delayed, chorus_state.feedback);
    fract32 input_with_feedback = add_fr1x32(input, feedback_signal);
    channel->delay_buffer[channel->write_index] = input_with_feedback;
    channel->write_index = (channel->write_index + 1) % CHORUS_DELAY_SIZE;
    fract32 output = add_fr1x32(
        mult_fr1x32x32(input, 0x7FFFFFFF - chorus_state.mix),
        mult_fr1x32x32(delayed, chorus_state.mix)
    );
    return output;
}

fract32 chorus_process_L(fract32 input) {
    fract32 lfo = osc_next(chorus_state.lfo_phase_L);
    chorus_state.lfo_phase_L += chorus_state.lfo_increment;
    return _chorus_process(input, lfo, &chorus_state.left);
}

fract32 chorus_process_R(fract32 input) {
    fract32 lfo = osc_next(chorus_state.lfo_phase_R);
    chorus_state.lfo_phase_R += chorus_state.lfo_increment;
    return _chorus_process(input, lfo, &chorus_state.right);
}

void chorus_clear_buffers(void) {
    int i;
    for (i = 0; i < CHORUS_DELAY_SIZE; i++) {
        chorus_state.left.delay_buffer[i] = 0;
        chorus_state.right.delay_buffer[i] = 0;
    }
    chorus_state.left.lpf_state = 0;
    chorus_state.right.lpf_state = 0;
} 
