/* PHONET: phonetic segments -> synthesizer control parameters -- port of
 * mitalk/phonet.py (Appendix C rules, Figure 11-6 smoothing templates).
 *
 * Tracks are drawn on a 1 ms grid and sampled every 5 ms for the
 * synthesizer.  F0 is drawn separately by the f0 module.  The
 * RECONSTRUCTION notes are in the Python reference.
 */
#if defined(__GNUC__) && defined(__i386__)
#pragma GCC target("sse2,fpmath=sse")
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mitalk.h"

enum { SETSMO, SMODIS, DISSMO, DISCON };

typedef struct {
    int sym, dur, stress, start;
} Seg;

static const Seg SILENCE = { S_SIL, 0, 0, 0 };

/* Parameter order of Table C-3, which defines ranges such as av..ab. */
static const int ORDER[NPAR] = {
    P_AV, P_AVC, P_AF, P_ASP, P_AN, P_A2P, P_A3P, P_A4P, P_A5P, P_A6P, P_AB,
    P_F1, P_F2, P_F3, P_F4, P_FNZ, P_B1, P_B2, P_B3
};

/* Table C-1/C-2 column of each PHONET parameter, -1 if none (an, fnz). */
static const int TCOL[NPAR] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, 10, 11, 12, 13, -1, 14, 15, 16 };
#define COL_F1 10
#define COL_F2 11
#define COL_F3 12

#define IS_AMP(p) ((p) <= P_AN)

typedef struct { int n; int p[NPAR]; } Range;

static Range prange(int first, int last)
{
    Range r;
    int a = 0, b = 0;
    for (int i = 0; i < NPAR; i++) {
        if (ORDER[i] == first) a = i;
        if (ORDER[i] == last) b = i;
    }
    r.n = 0;
    for (int i = a; i <= b; i++)
        r.p[r.n++] = ORDER[i];
    return r;
}

static Range F1_B3, F2_B3, AV_AB, AF_B3, F1_B2, AV_F1, A2P_B3;
static int ranges_ready;

static void init_ranges(void)
{
    if (ranges_ready)
        return;
    F1_B3 = prange(P_F1, P_B3);
    F2_B3 = prange(P_F2, P_B3);
    AV_AB = prange(P_AV, P_AB);
    AF_B3 = prange(P_AF, P_B3);
    F1_B2 = prange(P_F1, P_B2);
    AV_F1 = prange(P_AV, P_F1);
    A2P_B3 = prange(P_A2P, P_B3);
    ranges_ready = 1;
}

static int has(const Seg *s, const char *f)
{
    return seg_has(s->sym, s->stress, f);
}

static int manner(const Seg *s)
{
    if (has(s, "vowel")) return M_VOWEL;
    if (has(s, "stop")) return M_STOP;
    if (has(s, "fricative")) return M_FRICATIVE;
    return M_SONORANT;
}

/* MANNER_RANK: vowel 0, sonorant 1, fricative 2, stop 3 */
static int manner_rank(int m)
{
    return m == M_VOWEL ? 0 : m == M_SONORANT ? 1 : m == M_FRICATIVE ? 2 : 3;
}

/* MANNER_NUM: vowel 1, stop 2, fricative 3, sonorant 4 */
static int manner_num(int m)
{
    return m == M_VOWEL ? 1 : m == M_STOP ? 2 : m == M_FRICATIVE ? 3 : 4;
}

/* Initial Target values from Tables C-1/C-2 (C.4). */
static void table_targets(const Seg *seg, const Seg *nxt, double *tgt)
{
    for (int p = 0; p < NPAR; p++)
        tgt[p] = TCOL[p] >= 0 ? (double)TARGET[seg->sym][TCOL[p]] : 0.0;
    tgt[P_FNZ] = 250.0;
    tgt[P_AN] = 0.0;
    if ((seg->sym == S_HH || seg->sym == S_HX) && nxt) {
        static const int fp[] = { P_F1, P_F2, P_F3, P_F4, P_B1, P_B2, P_B3 };
        for (int i = 0; i < 7; i++)
            tgt[fp[i]] = TARGET[nxt->sym][TCOL[fp[i]]];
    }
}

typedef struct {
    double target[NPAR], oldval[NPAR], nextar[NPAR], diptar[3];
    double cumdur[NPAR], segdur[NPAR], mintime[NPAR];
    int trantype[NPAR];
    double tcf[NPAR], tcb[NPAR], bper[NPAR], bvalf[NPAR], bvalb[NPAR];
    int bvalb_set[NPAR];
    double aspdur, aspdux, aspamp, aspaml, buramp, burdur;
    int ndip;                       /* tcenter/tcdips/dip_bval set for f1..f3 */
    double tcenter[3], tcdips[3], dip_bval[3];
    int burst;
} State;

/* diptar index of f1/f2/f3 */
#define D(p) ((p) - P_F1)

/* ------------------------------------------------------------ tracks */

static double at(const Tracks *tr, int p, double t)
{
    double x = t > 0 ? t : 0;
    if (x > tr->n - 1)
        x = tr->n - 1;
    return tr->v[p][(int)x];
}

static void line(Tracks *tr, int p, double ft0, double v0, double ft1, double v1)
{
    int t0 = (int)rint(ft0), t1 = (int)rint(ft1);
    if (t0 < 0) t0 = 0;
    if (t1 > tr->n - 1) t1 = tr->n - 1;
    if (t1 <= t0)
        return;
    int num = t1 - t0 + 1, div = num - 1;
    double delta = v1 - v0, step = delta / div;
    double *v = tr->v[p] + t0;
    for (int i = 0; i < num - 1; i++)
        v[i] = (step == 0 ? (double)i / div * delta : (double)i * step) + v0;
    v[num - 1] = v1;
}

static void hold(Tracks *tr, int p, double ft0, double ft1, double val)
{
    int t0 = (int)rint(ft0), t1 = (int)rint(ft1);
    if (t0 < 0) t0 = 0;
    if (t1 > tr->n - 1) t1 = tr->n - 1;
    for (int t = t0; t <= t1; t++)
        tr->v[p][t] = val;
}

/* ------------------------------------------------------------- rules */

static void init_state(State *st, const Seg *cur, const Seg *prev, const Seg *nxt,
                       const Tracks *tr)
{
    double sil[NPAR];
    memset(st, 0, sizeof *st);
    table_targets(cur, nxt, st->target);
    table_targets(&SILENCE, NULL, sil);
    for (int p = 0; p < NPAR; p++)
        st->oldval[p] = cur->start > 0 ? at(tr, p, cur->start - 1) : sil[p];
    table_targets(nxt, NULL, st->nextar);
    const short *dip = DIPTAR[cur->sym];
    if (dip[0] || dip[1] || dip[2])
        for (int i = 0; i < 3; i++)
            st->diptar[i] = dip[i];
    else
        for (int i = 0; i < 3; i++)
            st->diptar[i] = st->target[P_F1 + i];
    double prev_start = prev != &SILENCE ? prev->start : 0;
    double prev_mid = prev_start + (prev != &SILENCE ? prev->dur / 2.0 : 0);
    for (int p = 0; p < NPAR; p++) {
        st->cumdur[p] = cur->start;
        st->segdur[p] = cur->dur;
        st->mintime[p] = IS_AMP(p) ? prev_mid : prev_start;
        st->trantype[p] = SETSMO;
        st->tcf[p] = TCF_DEFAULT[p];
    }
    double bp = BPER_DEFAULT[manner(prev)][manner(cur)];
    for (int p = 0; p < NPAR; p++)
        st->bper[p] = bp;
    st->aspamp = 51.0;
    st->aspaml = 51.0;
    st->buramp = 57.0;
    st->burdur = BURDUR[cur->sym];
}

static void general_rules(State *st, const Seg *cur, const Seg *prev)
{
    if (has(prev, "plosive"))
        st->mintime[P_AF] = st->cumdur[P_AF];
    if (!has(prev, "voiced"))
        st->trantype[P_AV] = DISSMO;
    if (prev->sym != S_SIL && cur->sym == S_SIL) {
        st->aspdux = 30;
        st->oldval[P_AVC] += 6;
        st->aspaml = st->aspamp;
    }
    if (has(prev, "sonorant") && !has(cur, "voiced"))
        st->aspdux = 10;
    if (has(prev, "sonorant") && !has(cur, "voiced") && has(cur, "fricative")) {
        st->cumdur[P_AF] -= 20;
        st->segdur[P_AF] += 20;
    }
    if (has(prev, "sonorant") && !has(cur, "voiced") && has(cur, "stop"))
        st->aspaml -= 6;
    if (!has(cur, "voiced"))
        st->trantype[P_AV] = SMODIS;
    if (has(prev, "stop") && !has(cur, "stop"))
        for (int i = 0; i < F1_B3.n; i++) {
            int p = F1_B3.p[i];
            st->tcf[p] = TCOBST[prev->sym] >= 0 ? TCOBST[prev->sym] : st->tcf[p];
        }
    if (has(prev, "stop") && has(prev, "nasal") && !has(cur, "stop") && has(cur, "voiced")) {
        st->trantype[P_AV] = st->trantype[P_AVC] = DISCON;
        for (int i = 0; i < AF_B3.n; i++)
            st->trantype[AF_B3.p[i]] = DISSMO;
    } else {
        for (int i = 0; i < AV_AB.n; i++)
            if (st->trantype[AV_AB.p[i]] == SETSMO)
                st->trantype[AV_AB.p[i]] = DISSMO;
        for (int i = 0; i < F1_B3.n; i++)
            st->trantype[F1_B3.p[i]] = SETSMO;
    }
}

static void sonorant_rules(State *st, const Seg *cur, const Seg *prev, const Seg *prev2,
                           const Seg *nxt, Tracks *tr)
{
    double *tg = st->target;
    if (has(cur, "round"))
        tg[P_F4] -= 100;
    if (has(cur, "retro") || has(cur, "lateral"))
        tg[P_F4] += 300;
    if (has(prev, "YY") && has(cur, "round")) {
        for (int i = 0; i < F1_B3.n; i++)
            st->tcf[F1_B3.p[i]] = 110;
    } else if (has(prev, "liqglide")) {
        for (int i = 0; i < F1_B2.n; i++)
            st->tcf[F1_B2.p[i]] = 75;
    }
    if (has(prev, "liqglide") && has(prev, "retro"))
        st->tcf[P_F3] = 90;
    if (has(prev, "wglide") || has(prev, "yglide"))
        st->bper[P_F1] = st->bper[P_F2] = st->bper[P_F3] = 35;
    if (has(prev, "palatal") && has(cur, "round"))
        st->tcf[P_F2] += 50;

    /* C.6.1 aspiration of a preceding unvoiced obstruent */
    if (!has(prev, "voiced") && !has(prev, "glotstop")
        && prev->sym != S_SIL && prev->sym != S_HH) {
        st->aspdur = 40;
        st->aspaml = st->aspamp;
        if (!prev->stress) {
            st->aspdur = 20;
            st->aspaml -= 3;
        }
        if (!has(prev, "labial") && !has(cur, "vowel") && !has(cur, "lateral"))
            st->aspdur = 55;
        else if (!has(cur, "vowel"))
            st->aspdur = 50;
        if (has(prev2, "voiced") && !cur->stress)
            st->aspdur -= 10;
        if (cur->sym == S_AXP)
            st->aspdur = 70;
        double overlap = 0.0;
        if (has(prev, "plosive") && BURDUR[prev->sym] > 5)
            overlap = BURDUR[prev->sym] - 5;
        if (has(prev, "fricative"))
            st->aspdur = has(cur, "vowel") ? 10 : st->segdur[P_ASP] / 2;
        if (st->segdur[P_ASP] < st->aspdur) st->aspdur = st->segdur[P_ASP];
        if (st->segdur[P_AV] < st->aspdur) st->aspdur = st->segdur[P_AV];
        if (has(prev, "alveolar") && has(cur, "retro"))
            st->aspaml += 9;
        if (st->aspdur > 0) {
            double t0 = st->cumdur[P_ASP] - overlap;
            double t1 = st->cumdur[P_ASP] + st->aspdur;
            hold(tr, P_ASP, t0, t1, st->aspaml);
            hold(tr, P_AV, t0, t1, 0.0);
            hold(tr, P_AVC, t0, t1, 0.0);
            hold(tr, P_B1, t0, t1, 150.0);
            if (has(prev, "fricative"))
                line(tr, P_ASP, t0 - 30, at(tr, P_ASP, t0 - 30), t0, st->aspaml);
            static const int ap[] = { P_ASP, P_AV, P_AVC, P_B1 };
            for (int i = 0; i < 4; i++) {
                int p = ap[i];
                st->cumdur[p] = t1;
                st->segdur[p] = cur->start + cur->dur - t1;
                st->oldval[p] = at(tr, p, t1 - 1);
                st->mintime[p] = t1;
            }
        }
    }

    /* C.6.2 nonnasal sonorant consonants */
    if (!has(cur, "vowel") && !has(cur, "nasal")) {
        const double *x = st->nextar;
        if (cur->sym == S_LL)
            tg[P_AV] -= 3;
        if (cur->sym == S_LL && has(nxt, "sonorant") && !has(nxt, "nasal"))
            tg[P_F2] = .9 * tg[P_F2] + .1 * x[P_F2];
        if (cur->sym == S_RR) {
            st->tcf[P_F3] = 75;
            tg[P_F3] = tg[P_F2] + 250;
        }
        if (cur->sym == S_RR && has(nxt, "sonorant") && !has(nxt, "nasal"))
            tg[P_F2] = .75 * tg[P_F2] + .25 * x[P_F2];
        if (cur->sym == S_LL && (!has(nxt, "sonorant") || has(nxt, "nasal")))
            tg[P_F2] = .9 * tg[P_F2] + .1 * 1450;
        if (cur->sym == S_RR && (!has(nxt, "sonorant") || has(nxt, "nasal")))
            tg[P_F2] = .75 * tg[P_F2] + .25 * 1450;
        if (has(prev, "sonorant") && !has(prev, "nasal") && has(cur, "lateral"))
            st->bper[P_F1] = st->bper[P_F2] = st->bper[P_F3] = 90;
        double v = !has(cur, "liqglide") ? 90 : has(prev, "liqglide") ? 50 : 70;
        for (int i = 0; i < F1_B3.n; i++)
            st->tcf[F1_B3.p[i]] = v;
        if (has(cur, "retro"))
            st->tcf[P_F3] = 90;
    }
}

static void vowel_rules(State *st, const Seg *cur, const Seg *prev, const Seg *prev2,
                        const Seg *nxt)
{
    double *tg = st->target, *dp = st->diptar;
    if (cur->sym == S_AXP && has(prev2, "sonorant") && !has(prev2, "nasal"))
        tg[P_F2] = (tg[P_F2] + TARGET[prev2->sym][COL_F2]) / 2;
    if (has(cur, "front") && has(cur, "lax") && has(nxt, "velar"))
        dp[D(P_F2)] = tg[P_F2];
    if ((has(cur, "front") || has(cur, "yglide")) && has(nxt, "lateral"))
        dp[D(P_F2)] -= 300;
    if ((has(cur, "front") || has(cur, "yglide")) && !has(cur, "diphthong")
        && has(nxt, "lateral"))
        dp[D(P_F2)] -= 300;
    if (cur->sym == S_YU && has(nxt, "alveolar") && has(nxt, "plosive"))
        dp[D(P_F2)] += 200;
    if (has(prev, "f2back") && has(cur, "round"))
        st->tcf[P_F2] += 50;
    if (has(cur, "yglide") && has(nxt, "alveolar"))
        dp[D(P_F2)] -= 150;
    if (!has(cur, "retro") && !has(cur, "rglide")) {
        double k = exp(-cur->dur / 60.0);
        static const double neutral[3] = { 490, 1450, 2500 };
        for (int i = 0; i < 3; i++) {
            tg[P_F1 + i] += (neutral[i] - tg[P_F1 + i]) * k;
            dp[i] += (neutral[i] - dp[i]) * k;
        }
    }
    if (has(cur, "schwa"))
        tg[P_F3] = (st->oldval[P_F3] + tg[P_F3] + st->nextar[P_F3]) / 3;
    if (has(cur, "f2back"))
        st->tcf[P_F2] -= 20;

    if (has(cur, "diphthong") && DIPHTHONG[cur->sym][0] >= 0) {
        double tcdiph = DIPHTHONG[cur->sym][0], tdmid = DIPHTHONG[cur->sym][1];
        double seg = st->segdur[P_F1];
        for (int i = 0; i < 3; i++) {
            st->tcenter[i] = tdmid / 100.0 * seg;
            st->tcdips[i] = tcdiph / 10.0 * .5 * (1 + seg / 45.0);
        }
        if (prev->sym == S_HH && has(cur, "lax"))
            for (int i = 0; i < 3; i++)
                st->tcenter[i] *= .67;
        double bv[3];
        for (int i = 0; i < 3; i++)
            bv[i] = (tg[P_F1 + i] + dp[i]) / 2;
        if (cur->sym == S_OY)
            bv[0] += 70;
        if (cur->sym == S_AY || cur->sym == S_OY) {
            bv[1] += 200;
            bv[2] -= 200;
        }
        if (cur->sym == S_YU)
            st->tcenter[2] *= .3;
        if (cur->sym == S_EXR || cur->sym == S_IXR)
            st->tcenter[2] *= .6;
        memcpy(st->dip_bval, bv, sizeof bv);
        st->ndip = 3;
    }
}

static void obstruent_rules(State *st, const Seg *cur, const Seg *prev, const Seg *nxt)
{
    double *tg = st->target;
    if (has(cur, "plosive") || has(cur, "fricative")) {        /* C.8.1 */
        if (has(cur, "voiced") && !has(nxt, "vowel"))
            tg[P_AVC] -= 30;
        if (has(cur, "palatal"))
            tg[P_F4] -= 150;
        if (has(cur, "alveolar") && has(cur, "plosive") && !has(nxt, "alveolar")) {
            static const double retro[5] = { 0, 67, 60, 65, 65 }, other[5] = { 0, 52, 60, 70, 73 };
            const double *vals = has(nxt, "retro") ? retro : other;
            for (int i = 0; i < 5; i++)
                tg[P_A2P + i] = vals[i];
        }
        if (has(cur, "velar") && !has(nxt, "vowel"))
            st->buramp -= 5;
        if ((!has(prev, "voiced") || !has(prev, "sonorant"))
            && has(cur, "voiced") && has(cur, "plosive"))
            tg[P_AVC] = 0;
    }
    if (has(cur, "fricative")) {                               /* C.8.2 */
        if (has(cur, "voiced")) {
            tg[P_AF] -= 10;
            if (tg[P_AV] < 47)
                tg[P_AV] = 47;
        }
        if (nxt->sym == S_SIL)
            tg[P_AF] -= 4;
        if (!has(prev, "fricative") && !has(prev, "stop")
            && has(nxt, "vowel") && st->segdur[P_AF] < 80)
            tg[P_AF] += (80 - st->segdur[P_AF]) / 7;
        if (has(prev, "vowel") && has(cur, "voiced") && has(nxt, "vowel"))
            tg[P_AVC] = 60;
        if (has(cur, "palatal") && has(nxt, "vowel") && has(nxt, "round")) {
            tg[P_F2] -= 50;
            tg[P_F3] -= 200;
        }
    }
    if (has(cur, "stop")) {                                    /* C.8.3 */
        if (!has(prev, "stop")) {
            for (int i = 0; i < AV_F1.n; i++)
                st->trantype[AV_F1.p[i]] = SMODIS;
            for (int i = 0; i < F2_B3.n; i++)
                st->trantype[F2_B3.p[i]] = SETSMO;
        }
        double cob = TCOBST[cur->sym] >= 0 ? TCOBST[cur->sym] : 80;
        if (has(prev, "high")) {
            for (int i = 0; i < F2_B3.n; i++)
                st->tcf[F2_B3.p[i]] = 10;
            st->tcf[P_F1] = 15;
        } else {
            for (int i = 0; i < F2_B3.n; i++)
                st->tcf[F2_B3.p[i]] = cob;
            st->tcf[P_F1] = (cob + 20) / 2;
        }
        /* C.8.3.1 burst at the expense of closure duration */
        if (has(cur, "plosive") && !has(cur, "glotstop") &&
            !has(nxt, "stop") && nxt->sym != S_SIL) {
            if (cur->sym == S_TT && has(nxt, "retro"))
                st->burdur += 10;
            double m = st->segdur[P_AF] < st->burdur ? st->segdur[P_AF] : st->burdur;
            st->burdur = m != 0 ? m : 5;
            if (!has(cur, "voiced"))
                st->buramp += 6;
            if (has(prev, "nasal") && !cur->stress)
                st->buramp -= 6;
            else if (!cur->stress)
                st->buramp -= 3;
            if (nxt->sym == S_AXP || has(nxt, "fricative"))
                st->buramp -= 3;
            st->burst = 1;
        }
    }
    if (has(cur, "nasal")) {                                   /* C.8.4 */
        tg[P_FNZ] = 450;
        st->bper[P_FNZ] = 100;
        for (int i = 0; i < AF_B3.n; i++)
            st->trantype[AF_B3.p[i]] = SMODIS;
        st->trantype[P_AV] = DISCON;
        if (!has(prev, "voiced"))
            st->trantype[P_AVC] = st->trantype[P_AV] = DISSMO;
    }
}

static void adjustments(State *st, const Seg *cur, const Seg *prev)
{
    int swap = manner_rank(manner(cur)) > manner_rank(manner(prev));
    const Seg *c, *v;
    double *c_val, *v_tgt;          /* aliases of the state arrays */
    if (swap) {
        c = cur; v = prev;
        c_val = st->target; v_tgt = st->oldval;
    } else {
        c = prev; v = cur;
        c_val = st->oldval; v_tgt = st->target;
    }
    double *bp = st->bper;

    if (has(v, "retro"))
        bp[P_F2] = bp[P_F3] = 75;
    if (has(c, "palatal") && !has(c, "sonorant")) {
        bp[P_F2] = 20;
        st->tcf[P_F2] += 30;
    }

    if (has(c, "stop") && !has(v, "stop")) {                   /* C.9.1 */
        bp[P_F2] = 50;
        bp[P_F2] = bp[P_F3] = 0;
        if (has(c, "nasal")) {
            int mancur = manner_num(manner(cur)), manlas = manner_num(manner(prev));
            bp[P_FNZ] = mancur - manlas > 1 ? 100 : 0;
            st->tcf[P_FNZ] = 150;
            bp[P_F1] = 0;
        }
        if (has(c, "labial") && !has(v, "glottal")) {
            if (has(v, "front")) {
                bp[P_F2] = 65;
                bp[P_F3] = 20;
            } else {
                bp[P_F2] = 20;
                bp[P_F3] = 70;
            }
            if (has(v, "retro")) {
                c_val[P_F3] = 1750;
                bp[P_F3] = 20;
            }
        }
        if (has(c, "alveolar")) {
            if (has(v, "lateral"))
                c_val[P_F2] = 1050;
            else if (c->sym == S_NN)
                c_val[P_F2] = 1400;
            else
                c_val[P_F2] = 1600;
            c_val[P_F3] = has(v, "retro") ? 2300 : 2620;
        }
        if (has(c, "velar")) {
            c_val[P_F2] = v_tgt[P_F2] + (v_tgt[P_F1] - 300) * 2;
            c_val[P_F3] = c_val[P_F2] + (has(v, "labial") ? 800 : 400);
            if (has(v, "front") || v->sym == S_AA) {
                c_val[P_F2] = TARGET[S_KP][COL_F2];
                c_val[P_F3] = TARGET[S_KP][COL_F3];
            }
            if (c->sym == S_NG) {
                c_val[P_F2] = (c_val[P_F2] + v_tgt[P_F2]) / 2;
                c_val[P_F3] = (c_val[P_F3] + v_tgt[P_F3]) / 2;
            }
            if (v->sym == S_IY || v->sym == S_IH) {
                c_val[P_F2] += 250;
                c_val[P_F3] += 50;
            }
        }
        if (has(c, "palvel")) {
            c_val[P_F2] = TARGET[c->sym][COL_F2];
            c_val[P_F3] = TARGET[c->sym][COL_F3];
        }
    }

    /* C.9.2 all segments (previous -> current, unswapped) */
    if (has(prev, "alveolar") && has(prev, "stop") && cur->sym == S_WW) {
        st->oldval[P_F2] = 1200;
        st->oldval[P_F3] = 2050;
        st->oldval[P_F4] = 2500;
        st->bper[P_F4] = 0;
        st->tcf[P_F4] = 30;
    }
    if ((has(prev, "rglide") || has(prev, "retro")) && has(cur, "alveolar")) {
        st->target[P_F2] = 1850;
        st->target[P_F3] = 2200;
    }
    if ((has(prev, "rglide") || has(prev, "retro")) && has(cur, "velar")) {
        st->target[P_F2] = 1700;
        st->target[P_F3] = 1900;
    }
    if (has(prev, "velar") && has(cur, "retro")) {
        st->oldval[P_F2] = 1100;
        st->oldval[P_F3] = 1500;
    }
    if (has(prev, "alveolar") && has(cur, "retro"))
        st->oldval[P_F3] = 2100;
    if (has(prev, "labial") && has(cur, "retro"))
        st->oldval[P_F3] = 1700;
    if (has(prev, "wglide") && has(cur, "velar")) {
        st->target[P_F2] = 900;
        st->target[P_F3] = 2000;
    }
    if (has(prev, "alveolar") && has(prev, "stop") && has(cur, "f2back"))
        st->oldval[P_F2] = 1850;

    for (int i = 0; i < F1_B3.n; i++) {
        int p = F1_B3.p[i];
        st->bvalf[p] = (bp[p] * v_tgt[p] + (100 - bp[p]) * c_val[p]) / 100;
    }
}

static void modifications(State *st, const Seg *cur, const Seg *prev)
{
    double *tg = st->target;
    if (cur->sym == S_DX)
        for (int p = P_F1; p <= P_F3; p++)
            tg[p] = (tg[p] + st->oldval[p] + st->nextar[p]) / 3;
    if (has(prev, "f2back"))
        st->tcf[P_F2] -= 20;
    for (int lo = P_F1; lo < P_F4; lo++)
        if (tg[lo + 1] < tg[lo] + 200)
            tg[lo + 1] = tg[lo] + 200;
    memcpy(st->tcb, st->tcf, sizeof st->tcb);
    for (int p = P_AV; p <= P_AN; p++)
        st->bvalf[p] = (st->bper[p] * tg[p] + (100 - st->bper[p]) * st->oldval[p]) / 100;
    if (has(prev, "lateral"))
        for (int p = P_F1; p <= P_F2; p++) {
            st->bvalb[p] = st->bvalf[p] - 50;
            st->bvalb_set[p] = 1;
            st->bvalf[p] += 50;
        }
    for (int i = 0; i < F1_B3.n; i++) {
        int p = F1_B3.p[i];
        if (!st->bvalb_set[p]) {
            st->bvalb[p] = st->bvalf[p];
            st->bvalb_set[p] = 1;
        }
    }
    if (has(prev, "nasal") && has(cur, "diphthong"))
        for (int p = P_F1; p <= P_F3; p++)
            st->bvalb[p] = st->oldval[p];
    for (int i = 0; i < AV_AB.n; i++) {
        int p = AV_AB.p[i];
        st->bvalf[p] = (tg[p] + st->oldval[p]) / 2;
        st->bvalb[p] = st->bvalf[p];
        st->bvalb_set[p] = 1;
    }
    if (has(prev, "vowel"))
        st->tcb[P_AV] = 30;
    if (has(prev, "voiced") && !has(cur, "voiced") && !has(cur, "sonorant")) {
        st->bvalb[P_AV] -= 6;
        st->bvalf[P_AV] = st->bvalb[P_AV];
    }
    if ((has(prev, "voiced") && (has(prev, "stop") || has(prev, "fricative")))
        && has(cur, "sonorant") && has(cur, "nasal")) {
        st->bvalb[P_AV] -= 4;
        st->bvalf[P_AV] = st->bvalb[P_AV];
    }
    if (has(prev, "glottal"))
        for (int i = 0; i < A2P_B3.n; i++) {
            int p = A2P_B3.p[i];
            st->bvalb[p] = st->bvalf[p] = st->oldval[p] = tg[p];
        }
    if (has(prev, "plosive"))
        st->bvalf[P_AF] = tg[P_AF];
}

/* Draw every parameter track for the current segment (Figure 11-6). */
static void draw(const State *st, const Seg *cur, Tracks *tr)
{
    double end = cur->start + cur->dur;
    for (int p = 0; p < NPAR; p++) {
        double t1 = st->cumdur[p];
        int kind = st->trantype[p];
        if ((kind == SETSMO || kind == SMODIS) && st->tcb[p] > 0) {
            double tb = t1 - st->tcb[p];
            if (st->mintime[p] > tb)
                tb = st->mintime[p];
            line(tr, p, tb, at(tr, p, tb), t1, st->bvalb[p]);
        }
        double tgt = st->target[p];
        if (kind == SETSMO || kind == DISSMO) {
            double tcf = st->tcf[p];
            if (IS_AMP(p) && (end - t1) / 2 < tcf)
                tcf = (end - t1) / 2;
            double tf = t1 + tcf;
            double v0 = st->bvalf[p];
            if (tf > end) {
                double d = tf - t1 > 1 ? tf - t1 : 1;
                double v_end = v0 + (tgt - v0) * (end - t1) / d;
                line(tr, p, t1, v0, end, v_end);
            } else {
                line(tr, p, t1, v0, tf, tgt);
                hold(tr, p, tf, end, tgt);
            }
        } else
            hold(tr, p, t1, end, tgt);
        hold(tr, p, end, tr->n - 1, at(tr, p, end));
    }

    /* Diphthong offglide (C.7.1, Figure C-2). */
    for (int i = 0; i < st->ndip; i++) {
        int p = P_F1 + i;
        double dip = st->diptar[i], tc = st->tcenter[i];
        if (fabs(dip - st->target[p]) < 1)
            continue;
        double tcen = cur->start + tc;
        double half = st->tcdips[i];
        if (tc < half) half = tc;
        if (end - tcen < half) half = end - tcen;
        if (half <= 0)
            continue;
        line(tr, p, tcen - half, at(tr, p, tcen - half), tcen, st->dip_bval[i]);
        line(tr, p, tcen, st->dip_bval[i], tcen + half, dip);
        hold(tr, p, tcen + half, end, dip);
        hold(tr, p, end, tr->n - 1, dip);
    }

    /* Pre-aspiration into a voiceless segment (Figure C-1). */
    if (st->aspdux > 0) {
        double ta = cur->start - st->aspdux;
        line(tr, P_ASP, ta - 30, at(tr, P_ASP, ta - 30), ta, st->aspaml);
        hold(tr, P_ASP, ta, cur->start, st->aspaml);
    }

    /* Plosive burst at the end of the closure (C.8.3.1). */
    if (st->burst) {
        double tb = end - st->burdur;
        double t0 = st->cumdur[P_AF];
        double lo = st->oldval[P_AF] - 20;
        line(tr, P_AF, t0 - 30, at(tr, P_AF, t0 - 30), t0, lo > 0 ? lo : 0);
        hold(tr, P_AF, t0 + 1, tb, 0.0);
        hold(tr, P_AF, tb, end, st->buramp);
        hold(tr, P_AF, end, tr->n - 1, st->buramp);
    }
}

int phonet_run(Utterance *u, Tracks *tr)
{
    init_ranges();
    int nseg = u->nflat;
    Seg *segs = xmalloc((nseg ? nseg : 1) * sizeof *segs);
    int t = 0;
    for (int i = 0; i < nseg; i++) {
        segs[i].sym = u->flat[i].sym;
        segs[i].dur = u->flat[i].dur;
        segs[i].stress = u->flat[i].stress;
        segs[i].start = t;
        u->flat[i].start = t;
        t += segs[i].dur;
    }
    int total = t;
    tr->n = total + 1;
    for (int p = 0; p < NPAR; p++)
        tr->v[p] = xmalloc(tr->n * sizeof(double));

    double sil[NPAR];
    table_targets(&SILENCE, NULL, sil);
    for (int p = 0; p < NPAR; p++)
        hold(tr, p, 0, total, sil[p]);

    State st;
    for (int i = 0; i < nseg; i++) {
        const Seg *cur = &segs[i];
        const Seg *prev = i > 0 ? &segs[i - 1] : &SILENCE;
        const Seg *prev2 = i > 1 ? &segs[i - 2] : &SILENCE;
        const Seg *nxt = i + 1 < nseg ? &segs[i + 1] : &SILENCE;
        init_state(&st, cur, prev, nxt, tr);
        general_rules(&st, cur, prev);
        if (has(cur, "sonorant") || cur->sym == S_AXP)
            sonorant_rules(&st, cur, prev, prev2, nxt, tr);
        if (has(cur, "vowel") || cur->sym == S_AXP)
            vowel_rules(&st, cur, prev, prev2, nxt);
        if (!has(cur, "sonorant") || has(cur, "nasal"))
            obstruent_rules(&st, cur, prev, nxt);
        adjustments(&st, cur, prev);
        modifications(&st, cur, prev);
        draw(&st, cur, tr);
    }
    free(segs);
    return total;
}

void tracks_free(Tracks *tr)
{
    for (int p = 0; p < NPAR; p++) {
        free(tr->v[p]);
        tr->v[p] = NULL;
    }
}

/* Sample tracks every 5 ms into synthesizer frames (klatt PARAMS order). */
double (*phonet_frames(const Tracks *tr, int total_ms, const double *f0,
                       int *nframes))[NKPAR]
{
    static const int KMAP[NKPAR] = {
        P_AV, P_AVC, P_ASP, P_AF, P_A2P, P_A3P, P_A4P, P_A5P, P_A6P, P_AB,
        P_AN, -1, P_F1, P_F2, P_F3, P_F4, P_FNZ, P_B1, P_B2, P_B3
    };
    int n = total_ms > 0 ? (total_ms + 4) / 5 : 0;
    double (*rows)[NKPAR] = xmalloc((n ? n : 1) * sizeof *rows);
    for (int i = 0; i < n; i++) {
        int t = 5 * i;
        for (int k = 0; k < NKPAR; k++)
            rows[i][k] = KMAP[k] < 0 ? f0[t] : tr->v[KMAP[k]][t];
    }
    *nframes = n;
    return rows;
}
