/* PROSOD: segmental durations (chapter 9), plus parsing of the phonemic
 * input notation used by the book -- port of mitalk/prosod.py.
 *
 *     C: RR 'AA KK * - ER .
 *
 * C: / F: begin a content / function word; ' and " (or 1 and 2) mark
 * primary and secondary stress on the following vowel; ! marks emphasis;
 * * is a morpheme boundary and - a syllable boundary (both carry no
 * duration); , . )? )N )C )P are syntactic boundaries.
 */
/* Python floats are IEEE doubles without excess precision; on i386 make
 * gcc use SSE2 arithmetic so the results are bit-identical. */
#if defined(__GNUC__) && defined(__i386__)
#pragma GCC target("sse2,fpmath=sse")
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "mitalk.h"

#define INITIAL_SILENCE 30                     /* Figure 9-1 */

static const char *const BORDER[] = { "", ")P", ")N", ")C", ",", ")?", "." };
#define NBORDER 7

/* Legal word-initial consonant clusters, used for consonant stress (8.4).
 * RECONSTRUCTION in the Python reference. */
static const char *const ONSET[] = {
    "PP RR", "PP LL", "BB RR", "BB LL", "TT RR", "TT WW", "DD RR", "DD WW",
    "KK RR", "KK LL", "KK WW", "GG RR", "GG LL", "GG WW", "FF RR", "FF LL",
    "TH RR", "TH WW", "SH RR", "SS PP", "SS TT", "SS KK", "SS MM", "SS NN",
    "SS LL", "SS WW", "SS FF", "VV RR", "PP YY", "BB YY", "KK YY", "GG YY",
    "MM YY", "FF YY", "VV YY", "HH YY",
    "SS PP RR", "SS PP LL", "SS TT RR", "SS KK RR", "SS KK WW",
    "SS KK LL", "SS PP YY", "SS KK YY",
};

static int border_index(const char *b)
{
    for (int i = 0; i < NBORDER; i++)
        if (!strcmp(BORDER[i], b))
            return i;
    return -1;
}

static int is_punct(const char *t)
{
    return border_index(t) > 0;
}

/* Rule 1 / Figure 9-1 pause after a boundary, or 0. */
static int pausing(const char *b)
{
    if (!strcmp(b, ",") || !strcmp(b, ")C")) return 200;
    if (!strcmp(b, ".") || !strcmp(b, ")?")) return 400;
    return 0;
}

static int has(const Phone *p, const char *f)
{
    return seg_has(p->sym, p->stress, f);
}

/* Python str.split() whitespace (ASCII part). */
static int pyspace(int c)
{
    return c == ' ' || (c >= '\t' && c <= '\r') || (c >= 0x1c && c <= 0x1f);
}

static Word *add_word(Utterance *u, int content)
{
    if (u->nwords == u->capwords) {
        u->capwords = u->capwords ? 2 * u->capwords : 16;
        u->words = xrealloc(u->words, u->capwords * sizeof *u->words);
    }
    Word *w = &u->words[u->nwords++];
    memset(w, 0, sizeof *w);
    w->content = content;
    w->first = u->nphones;
    return w;
}

static void syllabify(Utterance *u, Word *w)
{
    Phone *ph = u->phones + w->first;
    int nnuc = 0;
    for (int i = 0; i < w->n; i++)
        nnuc += has(&ph[i], "syllabic") != 0;
    w->nsyl = nnuc > 1 ? nnuc : 1;
    for (int i = 0; i < w->n; i++) {
        /* index of the first nucleus at or after i */
        int k = 0, syl = -1;
        for (int j = 0; j < w->n; j++) {
            if (!has(&ph[j], "syllabic"))
                continue;
            if (j >= i) {
                syl = k;
                break;
            }
            k++;
        }
        ph[i].syllable = syl >= 0 ? syl : w->nsyl - 1;
    }
}

static int legal_cluster(const int *syms, int n)
{
    char buf[64];
    if (n == 1)
        return 1;
    if (n > 3)
        return 0;
    buf[0] = 0;
    for (int i = 0; i < n; i++) {
        if (i)
            strcat(buf, " ");
        strcat(buf, SEG_NAME[syms[i]]);
    }
    for (size_t i = 0; i < sizeof ONSET / sizeof *ONSET; i++)
        if (!strcmp(ONSET[i], buf))
            return 1;
    return 0;
}

static void segmental_stress(Utterance *u, Word *w)
{
    Phone *ph = u->phones + w->first;
    for (int i = 0; i < w->n; i++) {
        Phone *p = &ph[i];
        if (!has(p, "syllabic"))
            continue;
        p->stress = p->lex_stress > 0;
        if (!p->stress)
            continue;
        int cluster[8], nc = 0;
        for (int j = i - 1; j >= 0 && !has(&ph[j], "syllabic") && ph[j].morph == p->morph; j--) {
            int cand[8], n = 0;
            if (nc + 1 > 7)
                break;                  /* cannot be a legal cluster */
            cand[n++] = ph[j].sym;
            for (int c = 0; c < nc; c++)
                cand[n++] = cluster[c];
            if (!legal_cluster(cand, n))
                break;
            memcpy(cluster, cand, n * sizeof *cand);
            nc = n;
            ph[j].stress = 1;
            ph[j].lex_stress = p->lex_stress;
        }
    }
}

static int handle_token(const char *tok, Utterance *u, int *cur, int *stress,
                        int *emph, int *morph)
{
    if (!strcmp(tok, "C:") || !strcmp(tok, "F:")) {
        add_word(u, !strcmp(tok, "C:"));
        *cur = u->nwords - 1;
        *morph = 0;
    } else if (is_punct(tok)) {
        if (u->nwords) {
            Word *w = &u->words[u->nwords - 1];
            if (border_index(w->boundary) < border_index(tok))
                strcpy(w->boundary, tok);
        }
    } else if (!strcmp(tok, "'") || !strcmp(tok, "1")) {
        *stress = 1;
    } else if (!strcmp(tok, "\"") || !strcmp(tok, "2")) {
        *stress = 2;
    } else if (!strcmp(tok, "!")) {
        *emph = 1;
    } else if (!strcmp(tok, "*")) {
        (*morph)++;
    } else if (!strcmp(tok, "-")) {
        /* syllable boundary: no effect */
    } else {
        char sym[16];
        size_t n = strlen(tok);
        int s = -1;
        if (n < sizeof sym) {
            for (size_t i = 0; i <= n; i++)
                sym[i] = (char)toupper((unsigned char)tok[i]);
            s = seg_lookup(sym);
        }
        if (s < 0) {
            fprintf(stderr, "mitalk: unknown segment '%s'\n", tok);
            return -1;
        }
        if (*cur < 0) {
            add_word(u, 1);
            *cur = u->nwords - 1;
        }
        if (u->nphones == u->capphones) {
            u->capphones = u->capphones ? 2 * u->capphones : 64;
            u->phones = xrealloc(u->phones, u->capphones * sizeof *u->phones);
        }
        Phone *p = &u->phones[u->nphones++];
        memset(p, 0, sizeof *p);
        p->sym = s;
        p->lex_stress = *stress;
        p->emphasis = *emph;
        p->morph = *morph;
        u->words[*cur].n++;
        *stress = 0;
        *emph = 0;
    }
    return 0;
}

int prosod_parse(const char *text, Utterance *u)
{
    int cur = -1, stress = 0, emph = 0, morph = 0;
    size_t len = strlen(text);
    char *tok = xmalloc(len + 1);
    size_t n = 0;
    for (size_t i = 0; i <= len; i++) {
        int c = (unsigned char)text[i];
        int special = c == '\'' || c == '"' || c == '!';
        if (c == 0 || pyspace(c) || special) {
            if (n) {
                tok[n] = 0;
                if (handle_token(tok, u, &cur, &stress, &emph, &morph)) {
                    free(tok);
                    return -1;
                }
                n = 0;
            }
            if (special) {
                char one[2] = { (char)c, 0 };
                handle_token(one, u, &cur, &stress, &emph, &morph);
            }
        } else
            tok[n++] = (char)c;
    }
    free(tok);
    for (int i = 0; i < u->nwords; i++) {
        syllabify(u, &u->words[i]);
        segmental_stress(u, &u->words[i]);
    }
    return 0;
}

/* ------------------------------------------------------------ durations */

static void add_flat(Utterance *u, const Phone *p)
{
    if (u->nflat == u->capflat) {
        u->capflat = u->capflat ? 2 * u->capflat : 64;
        u->flat = xrealloc(u->flat, u->capflat * sizeof *u->flat);
    }
    u->flat[u->nflat++] = *p;
}

static void add_pause(Utterance *u, int dur, int word)
{
    Phone p;
    memset(&p, 0, sizeof p);
    p.sym = S_SIL;
    p.dur = dur;
    p.pause = 1;
    p.word = word;
    add_flat(u, &p);
}

/* True if no syllabic phone lies at or after index i in the word, and one
 * lies before it. */
static int after_last_nucleus(const Utterance *u, const Word *w, int i)
{
    const Phone *ph = u->phones + w->first;
    for (int j = i; j < w->n; j++)
        if (has(&ph[j], "syllabic"))
            return 0;
    for (int j = 0; j < i; j++)
        if (has(&ph[j], "syllabic"))
            return 1;
    return 0;
}

/* Only non-syllabic segments of the same word lie between k and a pause. */
static int before_pause(const Utterance *u, int k)
{
    int j = k + 1;
    while (j < u->nflat && !u->flat[j].pause) {
        if (has(&u->flat[j], "syllabic") || u->flat[j].word != u->flat[k].word)
            return 0;
        j++;
    }
    return j < u->nflat;
}

/* Rule 9a table: effect of the consonant(s) after the vowel at index i. */
static int postvocalic(const Utterance *u, const Word *w, int i)
{
    const Phone *rest = u->phones + w->first + i + 1;
    int nrest = w->n - i - 1;
    if (nrest <= 0)
        return 120;
    const Phone *c = &rest[0];
    if (has(c, "sonorant") && nrest > 1 && !has(&rest[1], "sonorant"))
        c = &rest[1];
    if (has(c, "syllabic"))
        return 100;
    if (has(c, "fricative") && has(c, "voiced"))
        return 160;
    if (has(c, "plosive") && has(c, "voiced"))
        return 120;
    if (has(c, "nasal"))
        return 85;
    if (has(c, "plosive"))
        return 70;
    return 100;
}

/* Adjacent segment for Rule 10, or NULL across a pause or word boundary. */
static const Phone *cluster_neighbour(const Utterance *u, int k, int step)
{
    int j = k + step;
    if (j < 0 || j >= u->nflat || u->flat[j].pause || u->flat[j].word != u->flat[k].word)
        return NULL;
    return &u->flat[j];
}

void prosod_durations(Utterance *u)
{
    u->nflat = 0;
    add_pause(u, INITIAL_SILENCE, -1);
    for (int wi = 0; wi < u->nwords; wi++) {
        Word *w = &u->words[wi];
        for (int i = 0; i < w->n; i++) {
            u->phones[w->first + i].word = wi;
            add_flat(u, &u->phones[w->first + i]);
        }
        int pause = pausing(w->boundary);
        if (pause)
            add_pause(u, pause, wi);
    }
    if (!u->flat[u->nflat - 1].pause)
        add_pause(u, 400, -1);

    /* phone index (into u->phones) of each flat entry */
    int *pidx = xmalloc(u->nflat * sizeof *pidx);
    for (int k = 0, n = 0; k < u->nflat; k++)
        pidx[k] = u->flat[k].pause ? -1 : n++;

    for (int k = 0; k < u->nflat; k++) {
        Phone *p = &u->flat[k];
        if (p->pause)
            continue;
        const Word *w = &u->words[p->word];
        int idx = pidx[k] - w->first;            /* w.phones.index(p) */
        double mindur, inhdur;
        if (DURATIONS[p->sym][0] >= 0) {
            mindur = DURATIONS[p->sym][0];
            inhdur = DURATIONS[p->sym][1];
        } else {
            mindur = 40;
            inhdur = 60;
        }
        double prcnt = 100.0;
        int syll = has(p, "syllabic");
        const Phone *prev = &u->flat[k - 1];
        const Phone *nxt = &u->flat[k + 1];
        int bp = before_pause(u, k);
        int phrase_final = p->syllable == w->nsyl - 1 &&
                           (w->boundary[0] || p->word == u->nwords - 1);
        int word_final_syl = p->syllable == w->nsyl - 1;

        /* Rule 2: clause-final lengthening. */
        if (bp && (syll || after_last_nucleus(u, w, idx)))
            prcnt *= 1.40;
        /* Rule 3: non-phrase-final shortening. */
        if (syll && !phrase_final)
            prcnt *= 0.60;
        if (phrase_final && !syll && after_last_nucleus(u, w, idx)
            && (has(p, "liqglide") || has(p, "nasal")))
            prcnt *= 1.40;
        /* Rule 4: non-word-final shortening. */
        if (syll && !word_final_syl)
            prcnt *= 0.85;
        /* Rule 5: polysyllabic shortening. */
        if (syll && w->nsyl > 1)
            prcnt *= 0.80;
        /* Rule 6: non-initial-consonant shortening. */
        if (!syll && idx > 0)
            prcnt *= 0.85;
        /* Rule 7: unstressed shortening. */
        if (p->lex_stress == 0 && !p->stress)
            mindur /= 2;
        if (p->lex_stress != 1 || !p->stress) {
            if (syll) {
                int medial = 0 < p->syllable && p->syllable < w->nsyl - 1;
                prcnt *= medial ? 0.50 : 0.70;
            } else if (has(p, "liqglide") && has(nxt, "vowel"))
                prcnt *= 0.10;
            else
                prcnt *= 0.70;
        }
        /* Rule 8: emphasis. */
        if (p->emphasis && has(p, "vowel"))
            prcnt *= 1.40;
        /* Rule 9: postvocalic context. */
        const Phone *wp = u->phones + w->first;
        int cluster_son = !syll && has(p, "sonorant") && 0 < idx && idx < w->n - 1
                          && has(&wp[idx - 1], "vowel") && !has(&wp[idx + 1], "sonorant");
        if (has(p, "vowel") || cluster_son) {
            double f = postvocalic(u, w, cluster_son ? idx - 1 : idx);
            if (!phrase_final)
                f = 70 + 0.3 * f;
            prcnt *= f / 100.0;
        }
        /* Rule 10: shortening in clusters. */
        const Phone *prev_c = cluster_neighbour(u, k, -1);
        const Phone *next_c = cluster_neighbour(u, k, +1);
        if (has(p, "vowel")) {
            if (next_c && has(next_c, "vowel"))
                prcnt *= 1.20;
            if (prev_c && has(prev_c, "vowel"))
                prcnt *= 0.70;
        } else if (!syll) {
            int pc = prev_c && !has(prev_c, "syllabic");
            int nc = next_c && !has(next_c, "syllabic");
            if (pc && nc)
                prcnt *= 0.50;
            else if (pc || nc)
                prcnt *= 0.70;
        }

        double dur = (inhdur - mindur) * prcnt / 100.0 + mindur;     /* Eq. 1 */
        /* Rule 11: lengthening due to plosive aspiration. */
        if (p->stress && (has(p, "vowel") || has(p, "sonorant"))
            && has(prev, "plosive") && !has(prev, "voiced"))
            dur += 25;
        p->dur = (int)(ceil(dur / 5.0) * 5);
        u->phones[pidx[k]].dur = p->dur;
    }
    free(pidx);
}
