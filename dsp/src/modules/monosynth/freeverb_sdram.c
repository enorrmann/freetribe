#include "freeverb_sdram.h"
#include <string.h>


/* ================= SDRAM ================= */

#define SDRAM_ADDRESS 0xC0000000  // <-- ajustá a tu linker
#define SDRAM_SIZE    (1024*1024) // opcional

static uint8_t *sdram_ptr = (uint8_t *)SDRAM_ADDRESS;

static void *sdram_alloc(uint32_t size)
{
    void *p;

    size = (size + 7) & ~7; // align

    p = sdram_ptr;
    sdram_ptr += size;

    return p;
}
/* ================= FIXED ================= */

#define Q31(x) ((q31)((x) * 2147483647.0f))

static q31 mul_q31(q31 a, q31 b)
{
    return (q31)(((int64_t)a * b) >> 31);
}

static q31 sat_add(q31 a, q31 b)
{
    int64_t s = (int64_t)a + b;
    if (s > 0x7FFFFFFF) return 0x7FFFFFFF;
    if (s < -0x80000000) return -0x80000000;
    return (q31)s;
}

/* ================= CONFIG ================= */

#define NUM_COMB 4
#define NUM_AP   2
#define STEREO_SPREAD 23

static int comb_tuning[NUM_COMB] = {
    1116, 1277, 1422, 1557
};

static int ap_tuning[NUM_AP] = {
    556, 341
};


/* ================= INIT ================= */

static void comb_init(Comb *c, int size)
{
    c->size = size;
    c->buf = (q31*)sdram_alloc(sizeof(q31)*size);
    memset(c->buf, 0, sizeof(q31)*size);
    c->idx = 0;
    c->filterstore = 0;
}

static void ap_init(Allpass *a, int size)
{
    a->size = size;
    a->buf = (q31*)sdram_alloc(sizeof(q31)*size);
    memset(a->buf, 0, sizeof(q31)*size);
    a->idx = 0;
    a->feedback = Q31(0.5f);
}

void rv_init(Reverb *r)
{
    int i;

    memset(r, 0, sizeof(*r));

    for (i = 0; i < NUM_COMB; i++) {
        comb_init(&r->cl[i], comb_tuning[i]);
        comb_init(&r->cr[i], comb_tuning[i] + STEREO_SPREAD);
    }

    for (i = 0; i < NUM_AP; i++) {
        ap_init(&r->al[i], ap_tuning[i]);
        ap_init(&r->ar[i], ap_tuning[i] + STEREO_SPREAD);
    }

    r->gain = Q31(0.015f);
    r->room = Q31(0.75f);
    r->damp = Q31(0.4f);
    r->dry  = Q31(0.7f);

    r->wet1 = Q31(0.3f);
    r->wet2 = Q31(0.1f);

    /* apply params */
    for (i = 0; i < NUM_COMB; i++) {
        r->cl[i].feedback = r->room;
        r->cr[i].feedback = r->room;

        r->cl[i].damp1 = r->damp;
        r->cr[i].damp1 = r->damp;

        r->cl[i].damp2 = Q31(1.0f) - r->damp;
        r->cr[i].damp2 = Q31(1.0f) - r->damp;
    }
}

/* ================= DSP ================= */

static q31 comb_process(Comb *c, q31 input)
{
    q31 output;
    q31 tmp;

    output = c->buf[c->idx];

    c->filterstore =
        sat_add(
            mul_q31(output, c->damp2),
            mul_q31(c->filterstore, c->damp1)
        );

    tmp = sat_add(input, mul_q31(c->filterstore, c->feedback));
    c->buf[c->idx] = tmp;

    c->idx++;
    if (c->idx >= c->size) c->idx = 0;

    return output;
}

static q31 ap_process(Allpass *a, q31 input)
{
    q31 bufout;
    q31 output;

    bufout = a->buf[a->idx];

    output = bufout - input;

    a->buf[a->idx] =
        sat_add(input, mul_q31(bufout, a->feedback));

    a->idx++;
    if (a->idx >= a->size) a->idx = 0;

    return output;
}

/* ================= PROCESS ================= */

void rv_process(Reverb *r, q31 *buf, int n)
{
    int i, j;

    for (i = 0; i < n; i++) {

        q31 inL = buf[2*i];
        q31 inR = buf[2*i+1];
        q31 input;

        q31 outL = 0;
        q31 outR = 0;

        input = mul_q31((inL + inR) >> 1, r->gain);

        for (j = 0; j < NUM_COMB; j++) {
            outL = sat_add(outL, comb_process(&r->cl[j], input));
            outR = sat_add(outR, comb_process(&r->cr[j], input));
        }

        for (j = 0; j < NUM_AP; j++) {
            outL = ap_process(&r->al[j], outL);
            outR = ap_process(&r->ar[j], outR);
        }

        buf[2*i] =
            sat_add(
                sat_add(mul_q31(outL, r->wet1), mul_q31(outR, r->wet2)),
                mul_q31(inL, r->dry)
            );

        buf[2*i+1] =
            sat_add(
                sat_add(mul_q31(outR, r->wet1), mul_q31(outL, r->wet2)),
                mul_q31(inR, r->dry)
            );
    }
}