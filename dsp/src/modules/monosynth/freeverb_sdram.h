// freeverb_q31.h

#ifndef FREEVERB_Q31_H
#define FREEVERB_Q31_H

#include <stdint.h>

/* ================= TYPES ================= */

typedef int32_t q31;

/* ================= CONFIG ================= */

#define NUM_COMB 4
#define NUM_AP   2

/* ================= STRUCTS ================= */

typedef struct {
    q31 feedback;
    q31 filterstore;
    q31 damp1, damp2;
    q31 *buf;
    int size;
    int idx;
} Comb;

typedef struct {
    q31 feedback;
    q31 *buf;
    int size;
    int idx;
} Allpass;

typedef struct {

    q31 gain;
    q31 room;
    q31 damp;

    q31 wet1, wet2;
    q31 dry;

    Comb cl[NUM_COMB];
    Comb cr[NUM_COMB];
    Allpass al[NUM_AP];
    Allpass ar[NUM_AP];

} Reverb;

/* ================= API ================= */

// init + alloc en SDRAM
void rv_init(Reverb *r);

// procesa buffer stereo interleaved (Q31)
void rv_process(Reverb *r, q31 *buf, int n);

#endif