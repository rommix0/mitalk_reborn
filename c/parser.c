/* PARSER: parts of speech and phrase structure (chapter 4, Appendix A).
 * Port of mitalk/parser.py.
 *
 * Closed-class words come from a word list, every other word gets the
 * part-of-speech processor's default set refined by its suffix (sec. 4.5).
 * Phrases are found with the book's global strategy (sec. 4.6.1): at each
 * word take the longest noun group, else the longest verb group, else skip.
 *
 * Python's POS sets are bit sets here (PWord.pos), and PWord.tag is the
 * POS_* index of the single part of speech chosen in a phrase.
 */
#include <stdlib.h>
#include <string.h>
#include "mitalk.h"

enum {
    POS_ART, POS_DEM, POS_PRN, POS_DETQ, POS_QUANT, POS_PREP, POS_CONJ, POS_SCONJ,
    POS_PREPLIKE, POS_MOD, POS_BE, POS_HAVE, POS_DO, POS_TO, POS_NOT,
    POS_VERB, POS_NOUN, POS_ADJ, POS_ADV, POS_VERBING, POS_VERBEN, NPOS
};
#define B(p) (1u << POS_##p)

static const struct { int pos; const char *words; } CLOSED[] = {
    { POS_ART, "a an the" },
    { POS_DEM, "this that these those" },
    { POS_PRN, "i me my mine myself you your yours yourself yourselves he him his himself "
               "she her hers herself it its itself we us our ours ourselves they them "
               "their theirs themselves who whom whose what which anyone anybody anything "
               "everyone everybody everything someone somebody something nobody nothing none" },
    { POS_DETQ, "every any each no some either neither" },
    { POS_QUANT, "many much few several more most less least all both half" },
    { POS_PREP, "of in on at by for with from to into onto upon about above across after "
                "against along among around before behind below beneath beside between "
                "beyond during except inside near off out outside over past since through "
                "throughout toward towards under underneath until unto up within without via per" },
    { POS_CONJ, "and or but nor yet so" },
    { POS_SCONJ, "because although though whereas whether while unless if when whenever "
                 "where wherever" },
    { POS_PREPLIKE, "than as once" },
    { POS_MOD, "can could may might must shall should will would ought" },
    { POS_BE, "be am is are was were been being" },
    { POS_HAVE, "have has had having" },
    { POS_DO, "do does did" },
    { POS_TO, "" },
    { POS_NOT, "not never" },
};

static const char *const VERBS =
    "sat sit sits said say says went go goes got get gets made make makes took take "
    "takes came come comes saw see sees knew know knows thought think thinks told "
    "tell tells found find finds gave give gives left leave leaves felt feel feels "
    "kept keep keeps began begin begins ran brought bring brings bought buy buys "
    "stood stand stands held hold holds wrote write writes spoke speak speaks heard "
    "hear hears meant mean means met meet meets paid pay pays sent send sends built "
    "build builds lost lose loses fell grew grow grows drew draw draws flew wore "
    "wear wears broke break breaks chose choose chooses drove drive drives ate eat "
    "eats became become becomes seemed seem seems looked wanted want wants used "
    "worked called tried try tries asked ask asks needed liked like likes lived "
    "believed believe believes happened happen happens produced produce produces "
    "require requires required let lets";
static const char *const BOTH = "work works use uses call calls need needs look looks run runs";

static const struct { const char *ending; int pos; } CONTRACTIONS[] = {
    { "n't", POS_NOT }, { "'s", POS_BE }, { "'re", POS_BE }, { "'m", POS_BE },
    { "'ll", POS_MOD }, { "'d", POS_MOD }, { "'ve", POS_HAVE } };
static const struct { const char *word; unsigned pos; } CONTRACTION_BASE[] = {
    { "can't", B(MOD) | B(NOT) }, { "won't", B(MOD) | B(NOT) },
    { "shan't", B(MOD) | B(NOT) }, { "ain't", B(BE) | B(NOT) }, { "let's", B(VERB) } };
static const char *const WH_WORDS[] = {
    "who", "whom", "whose", "what", "which", "when", "where", "why", "how" };
static const char *const PERSONAL[] = {
    "i", "me", "my", "you", "your", "he", "him", "his", "she", "her", "it", "its",
    "we", "us", "our", "they", "them", "their" };

/* Table 10-1 peak levels, indexed by POS_*. */
static const int LEVEL[NPOS] = {
    [POS_ART] = 0, [POS_DEM] = 6, [POS_PRN] = 3, [POS_DETQ] = 2, [POS_QUANT] = 10,
    [POS_PREP] = 2, [POS_CONJ] = 1, [POS_SCONJ] = 1, [POS_PREPLIKE] = 7, [POS_MOD] = 9,
    [POS_BE] = 2, [POS_HAVE] = 2, [POS_DO] = 2, [POS_TO] = 2, [POS_NOT] = 12,
    [POS_VERB] = 6, [POS_NOUN] = 7, [POS_ADJ] = 7, [POS_ADV] = 7,
    [POS_VERBING] = 7, [POS_VERBEN] = 7 };

/* ------------------------------------------------------ the POS table */
#define MAXPOS 512
static struct { char word[24]; unsigned pos; } pos_tab[MAXPOS];
static int npos_tab;

static int pos_find(const char *w)
{
    for (int i = 0; i < npos_tab; i++)
        if (!strcmp(pos_tab[i].word, w))
            return i;
    return -1;
}

static int pos_entry(const char *w)
{
    int i = pos_find(w);
    if (i < 0 && npos_tab < MAXPOS) {
        i = npos_tab++;
        size_t len = strlen(w);
        if (len > sizeof pos_tab[i].word - 1)
            len = sizeof pos_tab[i].word - 1;
        memcpy(pos_tab[i].word, w, len);
        pos_tab[i].word[len] = 0;
        pos_tab[i].pos = 0;
    }
    return i;
}

/* for each word of list: POS[w] = bits (replace) or |= bits (add) */
static void pos_words(const char *list, unsigned bits, int replace)
{
    char w[24];
    while (*list) {
        while (*list == ' ')
            list++;
        size_t k = 0;
        while (*list && *list != ' ') {
            if (k + 1 < sizeof w)
                w[k++] = *list;
            list++;
        }
        w[k] = 0;
        if (k) {
            int i = pos_entry(w);
            if (i >= 0)
                pos_tab[i].pos = replace ? bits : pos_tab[i].pos | bits;
        }
    }
}

static void pos_init(void)
{
    if (npos_tab)
        return;
    for (size_t c = 0; c < sizeof CLOSED / sizeof *CLOSED; c++)
        pos_words(CLOSED[c].words, 1u << CLOSED[c].pos, 0);
    pos_words("to", B(TO), 0);
    pos_words(CLOSED[POS_PREPLIKE].words, B(PREP), 1);
    pos_words(VERBS, B(VERB), 1);
    pos_words(BOTH, B(VERB) | B(NOUN), 1);
}

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

/* len() in characters (UTF-8 continuation bytes are not counted) */
static size_t char_len(const char *s)
{
    size_t n = 0;
    for (; *s; s++)
        n += ((unsigned char)*s & 0xC0) != 0x80;
    return n;
}

static int in_list(const char *w, const char *const *list, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (!strcmp(w, list[i]))
            return 1;
    return 0;
}

/* Part-of-speech set for one word (closed list, else sec. 4.5 rules). */
static unsigned word_pos(const char *word)
{
    size_t len = strlen(word);
    char *w = xmalloc(len + 1);
    unsigned r;
    int i;
    lower_copy(w, word, len + 1);
    size_t clen = char_len(w);

    if ((i = pos_find(w)) >= 0) {
        r = pos_tab[i].pos;
        goto done;
    }
    for (size_t k = 0; k < sizeof CONTRACTION_BASE / sizeof *CONTRACTION_BASE; k++)
        if (!strcmp(w, CONTRACTION_BASE[k].word)) {
            r = CONTRACTION_BASE[k].pos;
            goto done;
        }
    for (size_t k = 0; k < sizeof CONTRACTIONS / sizeof *CONTRACTIONS; k++) {
        const char *e = CONTRACTIONS[k].ending;
        size_t el = strlen(e);
        if (ends_with(w, e) && clen > el) {
            char save = w[len - el];
            w[len - el] = 0;
            i = pos_find(w);
            w[len - el] = save;
            if (i >= 0)
                r = pos_tab[i].pos | 1u << CONTRACTIONS[k].pos;
            else
                r = k == 1 ? B(NOUN) : B(NOUN) | 1u << CONTRACTIONS[k].pos;
            goto done;
        }
    }
    if (ends_with(w, "ly") && clen > 4)
        r = B(ADV) | B(ADJ);
    else if (ends_with(w, "ing") && clen > 4)
        r = B(VERBING) | B(NOUN) | B(ADJ);
    else if (ends_with(w, "ed") && clen > 3)
        r = B(VERBEN) | B(VERB) | B(ADJ);
    else if (ends_with(w, "ness") || ends_with(w, "ment") || ends_with(w, "tion") ||
             ends_with(w, "sion") || ends_with(w, "ity") || ends_with(w, "ism") ||
             ends_with(w, "ist") || ends_with(w, "ance") || ends_with(w, "ence"))
        r = B(NOUN);
    else if (ends_with(w, "ous") || ends_with(w, "ful") || ends_with(w, "ive") ||
             ends_with(w, "able") || ends_with(w, "ible") || ends_with(w, "al") ||
             ends_with(w, "ic") || ends_with(w, "less"))
        r = B(ADJ) | B(NOUN);
    else if (ends_with(w, "s") && !ends_with(w, "ss") && clen > 3)
        r = B(NOUN) | B(VERB);
    else
        r = B(NOUN) | B(VERB) | B(ADJ);   /* the default routine */
done:
    free(w);
    return r;
}

typedef struct {
    const char *text;               /* full token */
    unsigned pos;
    int tag;
    int content, level;
    const char *boundary;           /* "", ")N", ")C", ",", ".", ")?" */
} W;

static int text_ends_s(const char *t)
{
    size_t n = strlen(t);
    return n && (t[n - 1] == 's' || t[n - 1] == 'S');
}

/* Length of the longest noun group at i (0 if none). */
static int noun_group(W *w, int n, int i)
{
    int j = i;
    if (j < n && (w[j].pos & B(PREP)))
        j++;
    int k = j;
    if (k < n && (w[k].pos & B(PRN)) && !(w[k].pos & (B(ART) | B(DEM))))
        return k + 1 - i;
    if (k < n && (w[k].pos & B(QUANT)))
        k++;
    if (k < n && (w[k].pos & (B(ART) | B(DEM) | B(DETQ) | B(PRN))))
        k++;
    int best = 0;
    while (k < n && (w[k].pos & (B(ADV) | B(ADJ) | B(NOUN) | B(VERBING) | B(VERBEN))) &&
           !(w[k].pos & (B(PREP) | B(CONJ) | B(SCONJ) | B(BE) | B(HAVE) | B(MOD) | B(DO) | B(NOT)))) {
        /* Subject-verb agreement: after a plural noun, an ambiguous word
         * without -s is taken as the verb ("messages work"). */
        if (best && (w[k].pos & B(VERB)) && text_ends_s(w[best - 1].text) &&
            !text_ends_s(w[k].text)) {
            w[k].pos = B(VERB);
            break;
        }
        k++;
        if (w[k - 1].pos & B(NOUN))
            best = k;
    }
    if (best)
        return best - i;
    /* a bare determiner + pronoun-like word ("this", "those") */
    if (k > j && (w[j].pos & (B(DEM) | B(QUANT))))
        return k - i;
    return 0;
}

/* Length of the longest verb group (or verbal) at i. */
static int verb_group(W *w, int n, int i)
{
    int k = i;
    unsigned aux = B(MOD) | B(BE) | B(HAVE) | B(DO) | B(NOT) | B(TO);
    while (k < n && ((w[k].pos & aux) || (w[k].pos == (B(ADV) | B(ADJ)) && k > i)))
        k++;
    if (k < n && (w[k].pos & (B(VERB) | B(VERBING) | B(VERBEN))) &&
        !(w[k].pos & (B(ART) | B(PREP) | B(PRN))))
        k++;
    return k > i ? k - i : 0;
}

static int pick(const W *w, int noun_group)
{
    static const int NG[] = { POS_PRN, POS_ART, POS_DEM, POS_DETQ, POS_QUANT, POS_PREP,
                              POS_NOUN, POS_ADJ, POS_ADV, POS_VERBING, POS_VERBEN, POS_VERB };
    static const int VG[] = { POS_MOD, POS_BE, POS_HAVE, POS_DO, POS_NOT, POS_TO, POS_PREP,
                              POS_CONJ, POS_SCONJ, POS_ART, POS_DEM, POS_PRN, POS_VERB,
                              POS_VERBING, POS_VERBEN, POS_ADV, POS_ADJ, POS_NOUN };
    const int *order = noun_group ? NG : VG;
    int n = noun_group ? (int)(sizeof NG / sizeof *NG) : (int)(sizeof VG / sizeof *VG);
    for (int k = 0; k < n; k++)
        if (w->pos & 1u << order[k])
            return order[k] == POS_VERBING || order[k] == POS_VERBEN ? POS_VERB : order[k];
    return POS_NOUN;
}

static int is_final_punct(const char *b)
{
    return !strcmp(b, ",") || !strcmp(b, ".") || !strcmp(b, ")?");
}

/* Tag a tokenized sentence and mark phrase boundaries. */
int parser_parse(const Sentence *s, PWord **out)
{
    pos_init();
    W *w = xmalloc((s->n ? (size_t)s->n : 1) * sizeof *w);
    int n = 0;
    for (int t = 0; t < s->n; t++) {
        const char *tok = s->tok[t];
        if (!strcmp(tok, ",") || !strcmp(tok, ".") || !strcmp(tok, "?")) {
            if (n)
                w[n - 1].boundary = !strcmp(tok, "?") ? ")?" : !strcmp(tok, ",") ? "," : ".";
            continue;
        }
        w[n].text = tok;
        w[n].pos = word_pos(tok);
        w[n].tag = 0;
        w[n].content = w[n].level = 0;
        w[n].boundary = "";
        n++;
    }

    int (*ng)[2] = xmalloc((n ? (size_t)n : 1) * sizeof *ng);
    int nng = 0;
    int i = 0;
    while (i < n) {
        int len = noun_group(w, n, i);
        if (len) {
            for (int k = i; k < i + len; k++)
                w[k].tag = pick(&w[k], 1);
            if (!*w[i + len - 1].boundary)
                w[i + len - 1].boundary = ")N";
            ng[nng][0] = i;
            ng[nng][1] = i + len - 1;
            nng++;
            i += len;
            continue;
        }
        len = verb_group(w, n, i);
        if (len) {
            for (int k = i; k < i + len; k++)
                w[k].tag = pick(&w[k], 0);
            i += len;
            continue;
        }
        w[i].tag = pick(&w[i], 0);
        i++;
    }

    for (int k = 0; k < n; k++) {
        w[k].level = LEVEL[w[k].tag];
        if (w[k].tag == POS_PRN) {
            size_t len = strcspn(w[k].text, "'");
            char low[16] = "-";
            if (len < sizeof low) {
                lower_copy(low, w[k].text, len + 1);
            }
            if (!in_list(low, PERSONAL, sizeof PERSONAL / sizeof *PERSONAL))
                w[k].level = 6;   /* a function word with secondary stress (8.2.2) */
        }
        int tg = w[k].tag;
        w[k].content = w[k].level >= 6 && tg != POS_DEM && tg != POS_MOD && tg != POS_QUANT &&
                       tg != POS_DETQ && tg != POS_PRN;
        /* clause boundary before a subordinating conjunction */
        if (k && tg == POS_SCONJ && !is_final_punct(w[k - 1].boundary))
            w[k - 1].boundary = ")C";
    }

    /* Sec. 8.2.4: erase a )N after a noun phrase with only one
     * primary-stressed content word (so NP + VP is one phrase). */
    for (int g = 0; g < nng; g++) {
        int first = ng[g][0], last = ng[g][1], sum = 0;
        if (strcmp(w[last].boundary, ")N"))
            continue;
        for (int k = first; k <= last; k++)
            sum += w[k].content;
        if (sum <= 1)
            w[last].boundary = "";
    }
    if (n && !*w[n - 1].boundary)
        w[n - 1].boundary = ".";
    /* Wh-questions take a falling tune. */
    if (n && !strcmp(w[n - 1].boundary, ")?")) {
        char low[16] = "-";
        if (strlen(w[0].text) < sizeof low)
            lower_copy(low, w[0].text, sizeof low);
        if (in_list(low, WH_WORDS, sizeof WH_WORDS / sizeof *WH_WORDS))
            w[n - 1].boundary = ".";
    }

    PWord *pw = xmalloc((n ? (size_t)n : 1) * sizeof *pw);
    for (int k = 0; k < n; k++) {
        pw[k].text = w[k].text;                    /* points into the Sentence */
        pw[k].pos = w[k].pos;
        pw[k].tag = w[k].tag;
        pw[k].content = w[k].content;
        pw[k].level = w[k].level;
        strcpy(pw[k].boundary, w[k].boundary);
    }
    free(ng);
    free(w);
    *out = pw;
    return n;
}
