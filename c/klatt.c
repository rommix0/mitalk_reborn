/* Klatt (1980) cascade/parallel formant synthesizer -- port of
 * mitalk/klatt.py.  See that file for the design notes; the structure
 * follows Figure 12-6 of the book:
 *
 *   impulse gen -> RGP -+-> AV  -> RGZ -+-> (cascade) RNP RNZ R1..R5 -+
 *                       +-> AVS -> RGS -+                             |
 *   noise -> MOD -+-> AH  -----------------^                          +-> out
 *                 +-> AF  -> parallel R2..R6, bypass AB --------------+
 *
 * The radiation first difference is moved into the voicing source and the
 * noise low-pass filter it would cancel is removed (book p.136).
 */
/* Python floats are IEEE doubles without excess precision; on i386 make
 * gcc use SSE2 arithmetic so the results match. */
#if defined(__GNUC__) && defined(__i386__)
#pragma GCC target("sse2,fpmath=sse")
#endif
#include <math.h>
#include <string.h>
#include "mitalk.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Constant parameters (Table 12-1; FNP = 270 Hz per ch. 11). */
#define FGP 0
#define BGP 100
#define FGZ 1500
#define BGZ 6000
#define BGS 200
#define F5 3850
#define B4 250
#define B5 200
#define F6 4900
#define B6 1000
#define FNP 250
#define BNP 100
#define BNZ 100
#define G0 48

/* Gains: see the calibration notes in mitalk/klatt.py. */
static const double K_PAR[5] = {0.15, 0.06, 0.04, 0.022, 0.03};   /* a2..a6 */
#define GAIN_VOICE 2.0e4
#define GAIN_ASP 1.45
#define GAIN_FRIC 4.626e-3
#define BURST_SEED 4242u

void rng_seed(Rng *r, unsigned int seed) { r->state = seed; }

double rng_uniform(Rng *r)
{
    r->state = 1664525u * r->state + 1013904223u;   /* mod 2^32 */
    return r->state / 4294967296.0;
}

static double db2lin(double db)
{
    return db <= 0 ? 0.0 : pow(10.0, (db - 87.0) / 20.0) * 32767.0;
}

static double parwav_tilt(double f)
{
    double w = 2 * M_PI * f / SR;
    double re = 1 - 0.75 * cos(w), im = 0.75 * sin(w);
    return 1.0 / sqrt(re * re + im * im);
}

typedef struct { double a, b, c, y1, y2; } Reson;      /* Eq. 1 */
typedef struct { double a, b, c, x1, x2; } AntiReson;

static void reson_set(Reson *r, double f, double bw, double gain)
{
    double rr = exp(-M_PI * bw / SR);
    r->c = -rr * rr;
    r->b = 2.0 * rr * cos(2.0 * M_PI * f / SR);
    r->a = (1.0 - r->b - r->c) * gain;
}

static double reson_step(Reson *r, double x)
{
    double y = r->a * x + r->b * r->y1 + r->c * r->y2;
    r->y2 = r->y1;
    r->y1 = y;
    return y;
}

static void anti_set(AntiReson *z, double f, double bw)
{
    double rr = exp(-M_PI * bw / SR);
    double c = -rr * rr, b = 2.0 * rr * cos(2.0 * M_PI * f / SR);
    double a = 1.0 - b - c;
    z->a = 1.0 / a;
    z->b = -b / a;
    z->c = -c / a;
}

static double anti_step(AntiReson *z, double x)
{
    double y = z->a * x + z->b * z->x1 + z->c * z->x2;
    z->x2 = z->x1;
    z->x1 = x;
    return y;
}

void klatt_synthesize(const double (*frames)[NKPAR], int nframes, double *out)
{
    Reson rgp = {0}, rgps = {0}, rgs = {0}, rnp = {0}, casc[5], par[5];
    AntiReson rgz = {0}, rnz = {0};
    Rng rng;
    int t0 = 0, nper = 0;
    double av_lin = 0, avs_lin = 0, last_voice = 0, prev_af = 0, prev_asp = 0;
    double gain_out = db2lin(G0) / 32767.0;

    memset(casc, 0, sizeof casc);
    memset(par, 0, sizeof par);
    rng_seed(&rng, BURST_SEED);
    anti_set(&rgz, FGZ, BGZ);

    for (int fi = 0; fi < nframes; fi++) {
        const double *P = frames[fi];
        double af0 = db2lin(prev_af), af1 = db2lin(P[K_AF]);
        double as0 = db2lin(prev_asp), as1 = db2lin(P[K_ASP]);
        int af_jump = P[K_AF] - prev_af > 50;
        const double fc[5][2] = {{P[K_F1], P[K_B1]}, {P[K_F2], P[K_B2]},
                                 {P[K_F3], P[K_B3]}, {P[K_F4], B4}, {F5, B5}};
        const double fp[5][2] = {{P[K_F2], P[K_B2]}, {P[K_F3], P[K_B3]},
                                 {P[K_F4], B4}, {F5, B5}, {F6, B6}};
        double ab_gain = db2lin(P[K_AB]) * 0.05 / sqrt(1 - 0.75 * 0.75);

        /* Coefficients change once per frame; state carries over. */
        reson_set(&rgp, FGP, BGP, 1.0);
        reson_set(&rgps, FGP, BGP, 1.0);
        reson_set(&rgs, 0, BGS, 1.0);
        reson_set(&rnp, FNP, BNP, 1.0);
        anti_set(&rnz, P[K_FNZ], BNZ);
        for (int k = 0; k < 5; k++) {
            reson_set(&casc[k], fc[k][0], fc[k][1], 1.0);
            reson_set(&par[k], fp[k][0], fp[k][1],
                      db2lin(P[K_A2 + k]) * K_PAR[k] * parwav_tilt(fp[k][0]));
        }
        if (af_jump)
            rng_seed(&rng, BURST_SEED);   /* same burst waveform every time */

        for (int n = 0; n < NWS; n++) {
            double imp_av = 0, imp_avs = 0, modulate = 1.0;
            /* Voicing source: AV/AVS latch at each impulse (12.1.16) and
             * scale the impulse entering RGP. */
            if (nper >= t0) {
                if (P[K_F0] > 0 && P[K_AV] + P[K_AVC] > 0) {
                    t0 = (int)floor(SR / P[K_F0] + 0.5);
                    if (t0 < 2) t0 = 2;
                    av_lin = db2lin(P[K_AV]) * GAIN_VOICE;
                    avs_lin = db2lin(P[K_AVC]) * GAIN_VOICE;
                    imp_av = av_lin;
                    imp_avs = avs_lin;
                } else {
                    t0 = 1;
                    av_lin = avs_lin = 0;
                }
                nper = 0;
            }
            if (av_lin > 0 && nper >= t0 / 2)
                modulate = 0.5;            /* 50% noise modulation (12.1.14) */
            nper++;

            double voice = anti_step(&rgz, reson_step(&rgp, imp_av))
                         + reson_step(&rgs, reson_step(&rgps, imp_avs));
            double voice_d = voice - last_voice;
            last_voice = voice;

            double noise = -8.0;
            for (int k = 0; k < 16; k++)
                noise += rng_uniform(&rng);
            noise *= modulate;
            double af_amp = af_jump ? af1 : af0 + (af1 - af0) * n / NWS;
            double asp_amp = as0 + (as1 - as0) * n / NWS;

            /* Cascade branch: voicing + aspiration. */
            double x = voice_d + noise * asp_amp * GAIN_ASP;
            x = anti_step(&rnz, reson_step(&rnp, x));
            for (int k = 0; k < 5; k++)
                x = reson_step(&casc[k], x);

            /* Parallel branch (SW=0): frication only, alternating signs. */
            double fric = noise * af_amp * GAIN_FRIC;
            double parallel = ab_gain * fric, sign = -1.0;
            for (int k = 0; k < 5; k++) {
                parallel += sign * reson_step(&par[k], fric);
                sign = -sign;
            }
            out[fi * NWS + n] = (x + parallel) * gain_out;
        }
        prev_af = P[K_AF];
        prev_asp = P[K_ASP];
    }
}
