#ifndef RESAMPLER_H
#define RESAMPLER_H

#include "types.h"

#define FP_SHIFT 16
#define FP_ONE (1 << FP_SHIFT)
#define FP_MASK (FP_ONE - 1)

#define STEP_44100 ((uint32_t)(((uint64_t)44100 << FP_SHIFT) / 48000))
#define STEP_22050 ((uint32_t)(((uint64_t)22050 << FP_SHIFT) / 48000))
#define STEP_11025 ((uint32_t)(((uint64_t)11025 << FP_SHIFT) / 48000))

static inline int32_t cubic_interp(int32_t y0, int32_t y1, int32_t y2,
                                   int32_t y3, uint32_t frac) {
    // frac: Q16.16

    int32_t a0 = -y0 + 3 * y1 - 3 * y2 + y3;
    int32_t a1 = 2 * y0 - 5 * y1 + 4 * y2 - y3;
    int32_t a2 = -y0 + y2;
    int32_t a3 = 2 * y1;

    // Horner form
    int64_t t = frac;
    int64_t res =
        (((a0 * t >> FP_SHIFT) + a1) * t >> FP_SHIFT + a2) * t >> FP_SHIFT + a3;

    return (int32_t)(res >> 1); // normalize
}

static inline int32_t cubic_interp_best(int32_t y0, int32_t y1, int32_t y2,
                                        int32_t y3, uint32_t frac) {
    // frac Q16.16
    int32_t t = frac;
    int32_t t2 = (int32_t)(((int64_t)t * t) >> FP_SHIFT);
    int32_t t3 = (int32_t)(((int64_t)t2 * t) >> FP_SHIFT);

    // Catmull-Rom optimizado (menos ops que tu versión anterior)
    int32_t a = (y2 - y0) >> 1;
    int32_t b = y0 - (5 * y1 >> 1) + 2 * y2 - (y3 >> 1);
    int32_t c = ((3 * (y1 - y2) + y3 - y0) >> 1);

    return y1 + ((int64_t)a * t >> FP_SHIFT) + ((int64_t)b * t2 >> FP_SHIFT) +
           ((int64_t)c * t3 >> FP_SHIFT);
}

static inline uint32_t get_step(uint32_t input_rate) {
    switch (input_rate) {
    case 44100:
        return STEP_44100;
    case 22050:
        return STEP_22050;
    case 11025:
        return STEP_11025;
    default:
        return STEP_44100;
    }
}

static inline uint32_t compute_out_len(int in_len, uint32_t input_rate) {
    switch (input_rate) {
    case 44100:
        return (uint64_t)in_len * 48000 / 44100;
    case 22050:
        return (uint64_t)in_len * 48000 / 22050;
    case 11025:
        return (uint64_t)in_len * 48000 / 11025;
    default:
        return (uint64_t)in_len * 48000 / 44100;
    }
}

static inline void resample_48000_cubic_best(const int32_t *in, int in_len,
                                             int32_t *out,
                                             uint32_t input_rate) {
    uint32_t step = get_step(input_rate);
    uint32_t out_len = compute_out_len(in_len, input_rate);

    uint32_t pos = 0;

    for (uint32_t i = 0; i < out_len; i++) {

        uint32_t idx = pos >> FP_SHIFT;
        uint32_t frac = pos & FP_MASK;

        // clamp mínimo necesario
        if (idx < 1)
            idx = 1;
        if (idx > in_len - 3)
            idx = in_len - 3;

        int32_t y0 = in[idx - 1];
        int32_t y1 = in[idx];
        int32_t y2 = in[idx + 1];
        int32_t y3 = in[idx + 2];

        out[i] = cubic_interp_best(y0, y1, y2, y3, frac);

        pos += step;
    }
}

static inline void resample_48000_fast(const int32_t *in, int in_len,
                                       int32_t *out, uint32_t input_rate) {
    uint32_t step = get_step(input_rate);
    uint32_t out_len = compute_out_len(in_len, input_rate);

    uint32_t pos = 0;
    int limit = in_len - 1;

    for (uint32_t i = 0; i < out_len; i++) {

        uint32_t idx = pos >> FP_SHIFT;
        uint32_t frac = pos & FP_MASK;

        if (idx >= limit)
            idx = limit - 1;

        int32_t s0 = in[idx];
        int32_t s1 = in[idx + 1];

        int32_t diff = s1 - s0;
        out[i] = s0 + ((diff * (int32_t)frac) >> FP_SHIFT);

        pos += step;
    }
}

#endif