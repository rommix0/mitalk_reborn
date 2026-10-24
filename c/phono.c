/* PHONO1/PHONO2: words -> broad phonetic transcription (chapter 8).
 * Port of mitalk/phono.py.
 *
 * Word level: dictionary lookup, function-word stress reduction (8.2.3),
 * and the phonemic recoding of 8.2.1 (r-colored vowels, syllabic
 * consonants) plus the reduced-vowel choices of 8.7.
 * Sentence level: the six segmental rules of 8.5, velar fronting, and the
 * AXP release vowel before pauses.  The result is a PHONO2 string.
 */
#include <stdlib.h>
#include <string.h>
#include "mitalk.h"

#define IS(sym, cls) ((SEG_CLASSES[sym] >> (cls)) & 1u)
#define SYLLABIC(sym) IS(sym, C_SYLLABIC)

typedef struct { int sym, st; } Ph;

#define MAXPH 1024              /* phones per word (63 spelled letters * 6 + 1) */

static const char *const DETERMINERS[] = {
    "a", "an", "the", "this", "that", "these", "those", "every", "any", "each", "no",
    "some", "my", "your", "his", "her", "its", "our", "their" };

static const char *const LETTERS[26] = {
    "EY", "BB IY", "SS IY", "DD IY", "IY", "EH FF", "JJ IY", "EY CH", "AY", "JJ EY",
    "KK EY", "EH LL", "EH MM", "EH NN", "OW", "PP IY", "KK YU", "AA RR", "EH SS",
    "TT IY", "YU", "VV IY", "DD AH BB AX LL YU", "EH KK SS", "WW AY", "ZZ IY" };

/* ------------------------------------------------------------ helpers */
static void lower_copy(char *dst, const char *src, size_t max)
{
    size_t i;
    for (i = 0; src[i] && i + 1 < max; i++)
        dst[i] = src[i] >= 'A' && src[i] <= 'Z' ? (char)(src[i] + 32) : src[i];
    dst[i] = 0;
}

static int ends_with(const char *s, const char *suf)
{
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && !memcmp(s + n - m, suf, m);
}

static int lower_is(const char *text, const char *word)
{
    size_t i;
    for (i = 0; text[i] && word[i]; i++) {
        char c = text[i] >= 'A' && text[i] <= 'Z' ? (char)(text[i] + 32) : text[i];
        if (c != word[i])
            return 0;
    }
    return !text[i] && !word[i];
}

static int in_determiners(const char *text)
{
    for (size_t i = 0; i < sizeof DETERMINERS / sizeof *DETERMINERS; i++)
        if (lower_is(text, DETERMINERS[i]))
            return 1;
    return 0;
}

static int sym_of(const char *name)
{
    return seg_lookup(name);
}

/* Parse "BB IY" into syms; returns count. */
static int parse_syms(const char *s, int *syms)
{
    int n = 0;
    char name[8];
    while (*s) {
        while (*s == ' ')
            s++;
        size_t k = 0;
        while (*s && *s != ' ') {
            if (k + 1 < sizeof name)
                name[k++] = *s;
            s++;
        }
        name[k] = 0;
        if (k)
            syms[n++] = sym_of(name);
    }
    return n;
}

/* Unknown word: spell it letter by letter, secondary stress on each letter
 * and primary on the last. */
static int spell(const char *word, size_t len, Ph *out, int max)
{
    int n = 0;
    for (size_t c = 0; c < len; c++) {
        unsigned char ch = (unsigned char)word[c];
        int letter = -1;
        if (ch >= 'a' && ch <= 'z')
            letter = ch - 'a';
        else if (ch >= 'A' && ch <= 'Z')
            letter = ch - 'A';
        else if (ch == 0xE2 && c + 2 < len && (unsigned char)word[c + 1] == 0x84 &&
                 (unsigned char)word[c + 2] == 0xAA)
            letter = 'k' - 'a';                     /* KELVIN SIGN lowers to k */
        else if (ch == 0xC4 && c + 1 < len && (unsigned char)word[c + 1] == 0xB0)
            letter = 'i' - 'a';                     /* U+0130 lowers to i + dot */
        if (letter < 0)
            continue;
        int syms[8];
        int k = parse_syms(LETTERS[letter], syms), first_vowel = 0;
        while (first_vowel < k && !SYLLABIC(syms[first_vowel]))
            first_vowel++;
        for (int i = 0; i < k && n < max; i++) {
            out[n].sym = syms[i];
            out[n].st = i == first_vowel ? 2 : 0;
            n++;
        }
    }
    if (!n) {
        out[0].sym = S_AX;
        out[0].st = 0;
        return 1;
    }
    int last = n - 1;
    while (last > 0 && !out[last].st)
        last--;
    out[last].st = 1;
    return n;
}

static int lookup(const char *word, Ph *out, int max)
{
    int syms[MAXPH], st[MAXPH];
    int n = lexicon_lookup(word, syms, st, max < MAXPH ? max : MAXPH);
    for (int i = 0; i < n; i++) {
        out[i].sym = syms[i];
        out[i].st = st[i];
    }
    return n;
}

/* Authors' 8.7 corrections: IX only in -es, -ed and -ish endings; other
 * unstressed IX is IH in a closed final syllable, else AX. */
static void reduced_vowels(Ph *ph, int n, const char *spelling)
{
    Ph orig[MAXPH];
    memcpy(orig, ph, n * sizeof *ph);
    int last_nucleus = -1;
    for (int i = 0; i < n; i++)
        if (SYLLABIC(orig[i].sym))
            last_nucleus = i;
    for (int i = 0; i < n; i++) {
        int s = orig[i].sym;
        if (orig[i].st || (s != S_IX && s != S_IH))
            continue;
        int final = last_nucleus >= 0 && i == last_nucleus;
        int nrest = n - i - 1;
        int ending = final && nrest == 1 &&
            ((orig[i + 1].sym == S_ZZ && (ends_with(spelling, "es") || ends_with(spelling, "'s"))) ||
             (orig[i + 1].sym == S_DD && ends_with(spelling, "ed")) ||
             (orig[i + 1].sym == S_SH && ends_with(spelling, "ish")));
        if (ending) {
            ph[i].sym = S_IX;
            ph[i].st = 0;
        } else if (s == S_IX) {
            ph[i].sym = final && nrest ? S_IH : S_AX;
            ph[i].st = 0;
        }
    }
}

/* 8.2.1 #5: word-final unstressed AX+LL -> EL; AX+NN after TT/DD -> EN. */
static int syllabics(Ph *ph, int n)
{
    int end = n;
    if (end >= 4 && (ph[n - 1].sym == S_ZZ || ph[n - 1].sym == S_SS || ph[n - 1].sym == S_DD) &&
        (ph[n - 2].sym == S_LL || ph[n - 2].sym == S_NN))
        end--;
    if (end >= 3 && ph[end - 2].sym == S_AX && ph[end - 2].st == 0) {
        int prev = ph[end - 3].sym, rep = -1;
        if (ph[end - 1].sym == S_LL && !SYLLABIC(prev))
            rep = S_EL;
        else if (ph[end - 1].sym == S_NN && (prev == S_TT || prev == S_DD))
            rep = S_EN;
        if (rep >= 0) {
            ph[end - 2].sym = rep;
            ph[end - 2].st = 0;
            memmove(&ph[end - 1], &ph[end], (n - end) * sizeof *ph);
            n--;
        }
    }
    return n;
}

static int r_color(int s)
{
    switch (s) {
    case S_IH: case S_IY: return S_IXR;
    case S_EH: case S_EY: case S_AE: return S_EXR;
    case S_AA: case S_AH: return S_AXR;
    case S_AO: case S_OW: return S_OXR;
    case S_UH: case S_UW: return S_UXR;
    default: return -1;
    }
}

/* 8.2.1 #3: vowel + RR (not followed by a vowel) -> r-colored vowel; after
 * AY/AW the R is the postvocalic RX ("fire"). */
static int r_colored(Ph *ph, int n, const char *spelling)
{
    Ph in[MAXPH];
    memcpy(in, ph, n * sizeof *ph);
    int m = 0, i = 0;
    int ir = strstr(spelling, "ir") || strstr(spelling, "our") || strstr(spelling, "yr");
    while (i < n) {
        int s = in[i].sym, st = in[i].st;
        int nxt = i + 1 < n ? in[i + 1].sym : -1;
        int after = i + 2 < n ? in[i + 2].sym : -1;
        if (nxt == S_RR && (after < 0 || !SYLLABIC(after))) {
            int rc = r_color(s);
            if (rc >= 0) {
                ph[m].sym = rc;
                ph[m++].st = st;
                i += 2;
                continue;
            }
            if (s == S_AY || s == S_AW) {
                ph[m].sym = s;
                ph[m++].st = st;
                ph[m].sym = S_RX;
                ph[m++].st = 0;
                i += 2;
                continue;
            }
        }
        if ((s == S_AY || s == S_AW) && nxt == S_ER && in[i + 1].st == 0 && ir) {
            ph[m].sym = s;
            ph[m++].st = st;
            ph[m].sym = S_RX;
            ph[m++].st = 0;
            i += 2;
            continue;
        }
        ph[m++] = in[i++];
    }
    return m;
}

/* [(sym, stress)] for one word, recoded at the word level. */
static int word_phones(const char *word, int content, int keep_secondary, Ph *ph)
{
    size_t wlen = strlen(word);
    char *spelling = xmalloc(wlen + 1);
    lower_copy(spelling, word, wlen + 1);
    int n;

    /* Function-word forms printed in the book's 8.7 output. */
    static const Ph WAS[] = { { S_WW, 0 }, { S_AH, 0 }, { S_ZZ, 0 } };
    static const Ph AND[] = { { S_AE, 0 }, { S_NN, 0 } };
    if (!content && (!strcmp(spelling, "was") || !strcmp(spelling, "and"))) {
        int was = spelling[0] == 'w';
        n = was ? 3 : 2;
        memcpy(ph, was ? WAS : AND, n * sizeof *ph);
        free(spelling);
        return n;
    }
    int poss = ends_with(spelling, "'s");
    n = lookup(word, ph, MAXPH - 2);
    if (!n && poss) {
        char *base = xmalloc(wlen + 1);
        memcpy(base, word, wlen - 2);
        base[wlen - 2] = 0;
        n = lookup(base, ph, MAXPH - 2);
        free(base);
        if (n) {
            int last = ph[n - 1].sym;
            if (last == S_SS || last == S_ZZ || last == S_SH || last == S_ZH || last == S_CH ||
                last == S_JJ) {
                ph[n++] = (Ph){ S_IX, 0 };
                ph[n++] = (Ph){ S_ZZ, 0 };
            } else if (last == S_PP || last == S_TT || last == S_KK || last == S_FF || last == S_TH)
                ph[n++] = (Ph){ S_SS, 0 };
            else
                ph[n++] = (Ph){ S_ZZ, 0 };
        }
    }
    if (!n) {
        if (poss && wlen > 2) {
            n = spell(word, wlen - 2, ph, MAXPH - 1);
            ph[n++] = (Ph){ S_ZZ, 0 };
        } else
            n = spell(word, wlen, ph, MAXPH);
    }

    if (!content) {
        /* 8.2.3: function words lose primary stress; polysyllabic ones keep
         * a secondary stress on their strongest syllable. */
        int nuclei = 0;
        for (int i = 0; i < n; i++)
            nuclei += SYLLABIC(ph[i].sym);
        for (int i = 0; i < n; i++)
            if (ph[i].st)
                ph[i].st = (nuclei > 1 || keep_secondary) && ph[i].st == 1 ? 2 : 0;
        for (int i = 0; i < n; i++)
            if (ph[i].sym == S_AH && !ph[i].st)
                ph[i].sym = S_AX;
    }

    reduced_vowels(ph, n, spelling);
    n = syllabics(ph, n);
    n = r_colored(ph, n, spelling);
    free(spelling);
    return n;
}

/* ------------------------------------------------------- sentence level */
typedef struct { int sym, st, word, first, last; } Seg;

typedef struct { char *s; size_t n, cap; } Buf;

static void buf_puts(Buf *b, const char *s)
{
    size_t k = strlen(s);
    if (b->n + k + 1 > b->cap) {
        b->cap = (b->n + k + 1) * 2;
        b->s = xrealloc(b->s, b->cap);
    }
    memcpy(b->s + b->n, s, k + 1);
    b->n += k;
}

static int is_pause_boundary(const char *b)       /* , . )? )C */
{
    return !strcmp(b, ",") || !strcmp(b, ".") || !strcmp(b, ")?") || !strcmp(b, ")C");
}

char *phono_phono2(const PWord *pw, int nw)
{
    Seg *segs = NULL;
    int n = 0, cap = 0;
    Ph ph[MAXPH];

    for (int wi = 0; wi < nw; wi++) {
        /* 8.2.2: secondary stress on non-personal pronouns and demonstratives */
        int k = word_phones(pw[wi].text, pw[wi].content, pw[wi].level == 6, ph);
        if (lower_is(pw[wi].text, "the")) {
            ph[0] = (Ph){ S_DH, 0 };
            ph[1] = (Ph){ S_AX, 0 };
            k = 2;
        }
        if (n + k > cap) {
            cap = (n + k) * 2;
            segs = xrealloc(segs, cap * sizeof *segs);
        }
        for (int i = 0; i < k; i++)
            segs[n++] = (Seg){ ph[i].sym, ph[i].st, wi, i == 0, i == k - 1 };
    }

#define BOUNDARY_AFTER(i) (segs[i].last ? pw[segs[i].word].boundary : "")
#define PHRASE_BREAK(i) (is_pause_boundary(BOUNDARY_AFTER(i)) || !strcmp(BOUNDARY_AFTER(i), ")N"))

    for (int i = 0; i < n; i++) {
        Seg *s = &segs[i];
        Seg *nx = i + 1 < n ? &segs[i + 1] : NULL;
        Seg *p = i > 0 ? &segs[i - 1] : NULL;
        /* Rule 6: "the" -> DH IY before a vowel. */
        if (lower_is(pw[s->word].text, "the") && s->sym == S_AX && nx && SYLLABIC(nx->sym) &&
            nx->sym != S_YU && !PHRASE_BREAK(i))
            s->sym = S_IY;
        /* Rule 1: postvocalic LL -> LX unless a stressed vowel follows in the word. */
        if (s->sym == S_LL && p && SYLLABIC(p->sym) && p->word == s->word) {
            if (!(nx && nx->word == s->word && SYLLABIC(nx->sym) && nx->st))
                s->sym = S_LX;
        }
        /* Rule 2: flapping (not across phrase/clause boundaries, not word-initial). */
        if ((s->sym == S_TT || s->sym == S_DD) && p && nx && !PHRASE_BREAK(i) && !s->first &&
            SYLLABIC(nx->sym) && nx->st != 1 && IS(p->sym, C_SONORANT) &&
            !IS(p->sym, C_NASAL) && nx->sym != S_EN)
            s->sym = S_DX;
        /* 8.7 correction: T before syllabic EN is glottalized ("certain"). */
        if (s->sym == S_TT && nx && nx->sym == S_EN)
            s->sym = S_TQ;
        /* Rule 3: word-final TT after a sonorant before a stressed sonorant -> TQ. */
        if (s->sym == S_TT && s->last && p && IS(p->sym, C_SONORANT) && nx && nx->first &&
            IS(nx->sym, C_SONORANT) && !is_pause_boundary(BOUNDARY_AFTER(i))) {
            /* stressed_onset(i + 1) */
            int onset = 0;
            for (int j = i + 1; j < n && segs[j].word == segs[i + 1].word; j++)
                if (SYLLABIC(segs[j].sym)) {
                    onset = segs[j].st > 0;
                    break;
                }
            if (onset)
                s->sym = S_TQ;
        }
    }

    /* Rule 5: glottal stop before a word-initial stressed vowel after a
     * syllabic segment (not a determiner), or after a voiced nonplosive
     * across a phrase boundary. */
    Seg *out = xmalloc((2 * (size_t)n + 1) * sizeof *out);
    int m = 0;
    for (int i = 0; i < n; i++) {
        Seg s = segs[i];
        if (s.first && s.st && SYLLABIC(s.sym) && s.sym != S_YU && i > 0) {
            const Seg *p = &segs[i - 1];
            if ((SYLLABIC(p->sym) && !in_determiners(pw[p->word].text)) ||
                (IS(p->sym, C_VOICED) && !IS(p->sym, C_PLOSIVE) && *BOUNDARY_AFTER(i - 1))) {
                out[m++] = (Seg){ S_QQ, 0, s.word, 1, 0 };
                s.first = 0;
            }
        }
        out[m++] = s;
    }
    free(segs);
    segs = out;
    n = m;

    /* Velar fronting before front vowels in the same word. */
    for (int i = 0; i + 1 < n; i++) {
        int f = segs[i + 1].sym;
        if ((segs[i].sym == S_KK || segs[i].sym == S_GG) && segs[i + 1].word == segs[i].word &&
            (f == S_AE || f == S_EH || f == S_EXR || f == S_EY || f == S_IH || f == S_IX ||
             f == S_IXR || f == S_IY || f == S_YU))
            segs[i].sym = segs[i].sym == S_KK ? S_KP : S_GP;
    }

    /* Build the PHONO2 string. */
    Buf b = { 0 };
    buf_puts(&b, "");
    for (int i = 0; i < n; i++) {
        const Seg *s = &segs[i];
        const PWord *w = &pw[s->word];
        if (s->first) {
            buf_puts(&b, b.n ? " " : "");
            buf_puts(&b, w->content ? "C:" : "F:");
        }
        buf_puts(&b, b.n ? " " : "");
        if (SYLLABIC(s->sym))
            buf_puts(&b, s->st == 1 ? "'" : s->st == 2 ? "\"" : "");
        buf_puts(&b, s->sym == S_CH ? "CH SH" : s->sym == S_JJ ? "JJ ZH" : SEG_NAME[s->sym]);
        if (s->last && *w->boundary) {
            /* 8.2.1 #7: release a plosive into a pause with AXP. */
            if (is_pause_boundary(w->boundary) && IS(s->sym, C_PLOSIVE) &&
                s->sym != S_CH && s->sym != S_JJ)
                buf_puts(&b, " AXP");
            buf_puts(&b, " ");
            buf_puts(&b, w->boundary);
        }
    }
    free(segs);
    return b.s;
}
