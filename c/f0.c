/* F0: fundamental frequency contour (chapter 10, O'Shaughnessy algorithm
 * as adjusted for MITalk in sec. 10.5) -- port of mitalk/f0.py.
 * Output is an F0 value per millisecond.
 */
#if defined(__GNUC__) && defined(__i386__)
#pragma GCC target("sse2,fpmath=sse")
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mitalk.h"

#define BASE_A 110.0
#define BASE_B 125.0
#define PEAK_MAX 190.0
#define FINAL_A (75.0 - 10.0)
#define UNACCENTED_BUMP 5.0
#define CONT_RISE_BIG 16.0
#define CONT_RISE_SMALL 8.0

typedef struct {
    double valley_t, valley_v, peak_t, peak_v, fall_t, fall_v, v0;
    int has_fall;
} Mark;

typedef struct {
    const Utterance *u;
    double peak0, base0, top_drop, base_drop;
    int t_beg, t_end;
} Ctx;

static double top(const Ctx *c, double tm)
{
    int d = c->t_end - c->t_beg;
    return c->peak0 * (1 - c->top_drop * (tm - c->t_beg) / (d > 1 ? d : 1));
}

static double base(const Ctx *c, double tm)
{
    int d = c->t_end - c->t_beg;
    return c->base0 * (1 - c->base_drop * (tm - c->t_beg) / (d > 1 ? d : 1));
}

/* Index of the word that closes the phrase containing word wi. */
static int phrase_end(const Utterance *u, int wi)
{
    for (int j = wi; j < u->nwords; j++)
        if (u->words[j].boundary[0])
            return j;
    return u->nwords - 1;
}

/* np.linspace(start, stop, num)[i] */
static double linspace_at(double start, double stop, int num, int i)
{
    int div = num - 1;
    double delta = stop - start, y;
    if (num > 1 && i == num - 1)
        return stop;
    if (div > 0) {
        double step = delta / div;
        if (step == 0)
            y = (double)i / div * delta;
        else
            y = (double)i * step;
    } else
        y = (double)i * delta;
    return y + start;
}

/* Position of syllable key (word, syl) in order[], or -1. */
static int order_index(const int *ow, const int *os, int n, int word, int syl)
{
    for (int i = 0; i < n; i++)
        if (ow[i] == word && os[i] == syl)
            return i;
    return -1;
}

double *f0_contour(Utterance *u, int total_ms)
{
    int question = u->nwords > 0 && !strcmp(u->words[u->nwords - 1].boundary, ")?");
    double base0 = question ? BASE_B : BASE_A;
    double *f0 = xmalloc((total_ms + 1) * sizeof *f0);

    /* start times on the flat list, copied to the word phones */
    int t = 0, nvoiced = 0, first_v = -1, last_v = -1;
    int *flat_of = xmalloc((u->nphones ? u->nphones : 1) * sizeof *flat_of);
    for (int k = 0; k < u->nflat; k++) {
        Phone *p = &u->flat[k];
        p->start = t;
        t += p->dur;
        if (!p->pause) {
            u->phones[nvoiced].start = p->start;
            flat_of[nvoiced] = k;
            if (first_v < 0)
                first_v = k;
            last_v = k;
            nvoiced++;
        }
    }
    if (!nvoiced) {
        for (int i = 0; i <= total_ms; i++)
            f0[i] = base0;
        free(flat_of);
        return f0;
    }

    /* One accent per content word, on its primary-stressed nucleus. */
    int *acc_w = xmalloc((u->nwords + 1) * sizeof *acc_w);
    int *acc_p = xmalloc((u->nwords + 1) * sizeof *acc_p);   /* index in u->phones */
    int nacc = 0;
    for (int wi = 0; wi < u->nwords; wi++) {
        const Word *w = &u->words[wi];
        if (!w->content)
            continue;
        int best1 = -1, best2 = -1, first = -1;
        for (int i = 0; i < w->n; i++) {
            const Phone *p = &u->phones[w->first + i];
            if (!seg_has(p->sym, p->stress, "syllabic"))
                continue;
            if (first < 0) first = w->first + i;
            if (best1 < 0 && p->lex_stress == 1) best1 = w->first + i;
            if (best2 < 0 && p->lex_stress == 2) best2 = w->first + i;
        }
        if (first < 0)
            continue;
        acc_w[nacc] = wi;
        acc_p[nacc] = best1 >= 0 ? best1 : best2 >= 0 ? best2 : first;
        nacc++;
    }

    int nsyl = 0;
    for (int wi = 0; wi < u->nwords; wi++)
        nsyl += u->words[wi].nsyl;
    Ctx c;
    c.u = u;
    c.base0 = base0;
    c.peak0 = 150.0 + 1.5 * nsyl;
    if (c.peak0 > PEAK_MAX)
        c.peak0 = PEAK_MAX;
    c.t_end = u->flat[last_v].start + u->flat[last_v].dur;
    c.t_beg = u->flat[first_v].start;
    c.top_drop = !question ? 0.30 : 0.15;
    c.base_drop = !question ? 0.20 : 0.05;

    /* syllable keys of the syllabic voiced phones, in order */
    int *ow = xmalloc(u->nflat * sizeof *ow), *os = xmalloc(u->nflat * sizeof *os), norder = 0;
    for (int k = 0; k < u->nflat; k++) {
        const Phone *p = &u->flat[k];
        if (!p->pause && seg_has(p->sym, p->stress, "syllabic")) {
            ow[norder] = p->word;
            os[norder] = p->syllable;
            norder++;
        }
    }

    Mark *marks = xmalloc((nacc + 1) * sizeof *marks);
    for (int k = 0; k < nacc; k++) {
        int wi = acc_w[k];
        const Word *w = &u->words[wi];
        const Phone *nuc = &u->phones[acc_p[k]];
        int idx = order_index(ow, os, norder, nuc->word, nuc->syllable);
        int prev_idx = -1, next_idx = norder;
        if (k) {
            const Phone *q = &u->phones[acc_p[k - 1]];
            prev_idx = order_index(ow, os, norder, q->word, q->syllable);
        }
        if (k + 1 < nacc) {
            const Phone *q = &u->phones[acc_p[k + 1]];
            next_idx = order_index(ow, os, norder, q->word, q->syllable);
        }
        int gap_before = idx - prev_idx - 1, gap_after = next_idx - idx - 1;

        double peak = top(&c, nuc->start);
        double span = peak - base(&c, nuc->start);
        if (gap_before == 2 || gap_before == 3)
            peak -= span * (gap_before == 2 ? 0.15 : 0.25);
        if (gap_after == 2 || gap_after == 3)
            peak += span * (gap_after == 2 ? 0.10 : 0.15);
        if (peak > PEAK_MAX)
            peak = PEAK_MAX;

        int pe = phrase_end(u, wi);
        int phrase_initial = k == 0 || phrase_end(u, acc_w[k - 1]) != pe;
        int phrase_final = k + 1 == nacc || phrase_end(u, acc_w[k + 1]) != pe;
        const char *pb = u->words[pe].boundary;
        int nuclear = phrase_final && (!strcmp(pb, ",") || !strcmp(pb, ".") ||
                                       !strcmp(pb, ")?") || !strcmp(pb, ")C") ||
                                       pe == u->nwords - 1);

        double rise = 0.40 * (peak - base0);
        double fall = 0.20 * (peak - base(&c, nuc->start));
        if (!phrase_initial && !phrase_final) {
            rise *= 0.70;
            fall *= 0.70;
        }
        if (phrase_final)
            fall *= 2.0;
        if (gap_before == 0 && k)
            rise *= 0.60;
        rise *= gap_before == 2 ? 1.15 : gap_before == 3 ? 1.20 : gap_before >= 4 ? 1.30 : 1.0;

        /* syllable onset: first phone of the nucleus's syllable, unless it
         * is the nucleus itself */
        const Phone *onset = NULL;
        for (int i = 0; i < w->n; i++) {
            const Phone *p = &u->phones[w->first + i];
            if (p->syllable == nuc->syllable) {
                onset = p == nuc ? NULL : p;
                break;
            }
        }
        int unvoiced_onset = onset && !seg_has(onset->sym, onset->stress, "voiced");
        double v0 = nuc->start, v1 = nuc->start + nuc->dur;
        double valley_t = unvoiced_onset ? v0 : (onset ? onset->start : v0);
        Mark *m = &marks[k];
        m->has_fall = 1;
        if (nuclear) {
            m->peak_t = valley_t < v0 ? v0 : v0 + (10 < nuc->dur / 2 ? 10 : nuc->dur / 2);
            m->valley_v = peak - rise;
            m->fall_t = v1;
        } else {
            m->peak_t = v1;
            m->valley_v = peak - rise;
            if (unvoiced_onset)
                m->valley_v += 0.20 * rise;
            int fk = flat_of[acc_p[k]] + 1;
            if (fk < u->nflat)
                m->fall_t = u->flat[fk].start + u->flat[fk].dur / 2.0;
            else
                m->fall_t = v1 + 20;
        }
        m->valley_t = valley_t;
        m->peak_v = peak;
        m->fall_v = peak - fall;
        m->v0 = v0;
    }

    for (int k = 0; k + 1 < nacc; k++) {
        Mark *cur = &marks[k], *nxt = &marks[k + 1];
        if (nxt->valley_t <= cur->peak_t + 10)
            nxt->valley_t = (cur->peak_t + nxt->v0) / 2;
        if (cur->fall_t >= nxt->valley_t)
            cur->has_fall = 0;
        else if (cur->fall_v < nxt->valley_v)
            cur->fall_v = nxt->valley_v = (cur->fall_v + nxt->valley_v) / 2;
    }

    int npts = 0;
    double *pt = xmalloc((3 * nacc + 2) * sizeof *pt), *pv = xmalloc((3 * nacc + 2) * sizeof *pv);
    pt[npts] = c.t_beg;
    pv[npts++] = (c.peak0 + base0) / 2;
    for (int k = 0; k < nacc; k++) {
        pt[npts] = marks[k].valley_t;
        pv[npts++] = marks[k].valley_v;
        pt[npts] = marks[k].peak_t;
        pv[npts++] = marks[k].peak_v;
        if (marks[k].has_fall) {
            pt[npts] = marks[k].fall_t;
            pv[npts++] = marks[k].fall_v;
        }
    }
    double final = FINAL_A;
    if (question) {
        double mx = pv[0];
        for (int i = 1; i < npts; i++)
            if (pv[i] > mx)
                mx = pv[i];
        final = 1.2 * mx;
    }
    pt[npts] = c.t_end;
    pv[npts++] = final;

    /* keep time order as built; nudge equal times */
    for (int i = 1; i < npts; i++)
        if (pt[i - 1] + 1 > pt[i])
            pt[i] = pt[i - 1] + 1;

    /* np.interp over the anchor points */
    {
        int j = 0;
        for (int i = 0; i <= total_ms; i++) {
            double x = i;
            if (x < pt[0])
                f0[i] = pv[0];
            else if (x > pt[npts - 1])
                f0[i] = pv[npts - 1];
            else {
                while (j > 0 && pt[j] > x)
                    j--;
                while (j + 1 < npts && pt[j + 1] <= x)
                    j++;
                if (j == npts - 1 || pt[j] == x)
                    f0[i] = pv[j];
                else {
                    double slope = (pv[j + 1] - pv[j]) / (pt[j + 1] - pt[j]);
                    f0[i] = slope * (x - pt[j]) + pv[j];
                }
            }
        }
    }

    /* Continuation rises over the last syllable of a nonfinal phrase. */
    int last_cont = -10, len = total_ms + 1;
    for (int j = 0; j + 1 < u->nwords; j++) {
        const Word *w = &u->words[j];
        if (!w->n)
            continue;
        int big = !strcmp(w->boundary, ",") || !strcmp(w->boundary, ")C");
        int small = !strcmp(w->boundary, ")N") || !strcmp(w->boundary, ")P");
        if (big || (small && j - last_cont > 5)) {
            double amount = big ? CONT_RISE_BIG : CONT_RISE_SMALL;
            int lo = -1;
            for (int i = 0; i < w->n; i++)
                if (u->phones[w->first + i].syllable == w->nsyl - 1) {
                    lo = u->phones[w->first + i].start;
                    break;
                }
            const Phone *lastp = &u->phones[w->first + w->n - 1];
            int hi = lastp->start + lastp->dur;
            int n = hi - lo > 0 ? hi - lo : 0;
            for (int i = 0; i < n && lo + i < len; i++)
                f0[lo + i] += linspace_at(0, amount, n, i);
            int tl = len - hi;
            if (tl > 60) tl = 60;
            if (tl < 0) tl = 0;
            for (int i = 0; i < tl; i++)
                f0[hi + i] += linspace_at(amount, 0, tl, i);
            last_cont = j;
        }
    }

    /* 5 Hz perturbation about the midpoint of flat or falling unaccented
     * syllables (10.5). */
    for (int k = 0; k < u->nflat; k++) {
        const Phone *p = &u->flat[k];
        if (p->pause)
            continue;
        int lo = p->start, hi = p->start + p->dur;
        if (!seg_has(p->sym, p->stress, "syllabic") || hi <= lo)
            continue;
        int accented = 0;
        for (int a = 0; a < nacc; a++)
            if (u->phones[acc_p[a]].word == p->word && u->phones[acc_p[a]].syllable == p->syllable)
                accented = 1;
        if (accented || !(f0[hi < total_ms ? hi : total_ms] <= f0[lo]))
            continue;
        int mid = p->start + p->dur / 2;
        double half = p->dur / 2.0;
        double d = half > 1 ? half : 1;
        for (int x = lo; x < hi && x < len; x++)
            f0[x] += UNACCENTED_BUMP * (1 - abs(x - mid) / d);
    }
    /* Dip at points of glottalization. */
    for (int k = 0; k < u->nflat; k++) {
        const Phone *p = &u->flat[k];
        if (p->pause || (p->sym != S_QQ && p->sym != S_TQ))
            continue;
        for (int x = p->start; x < p->start + p->dur && x < len; x++)
            f0[x] *= 0.75;
    }

    free(flat_of);
    free(acc_w);
    free(acc_p);
    free(ow);
    free(os);
    free(marks);
    free(pt);
    free(pv);
    return f0;
}
