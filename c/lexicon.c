/* Pronunciation lexicon: data/lexicon.tsv (port of mitalk/lexicon.py).
 *
 * Lines are "WORD<TAB>phones<TAB>source"; phones look like `'AA "EH TT`
 * (' = primary stress, " = secondary).  The file is read into one buffer,
 * split in place, and searched with bsearch on the upper-cased word.  The
 * phone string of an entry is parsed only when it is looked up.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mitalk.h"

typedef struct {
    const char *word;
    const char *phones;
    int line;                       /* file order, so a later duplicate wins */
} Entry;

static char *lex_buf;
static Entry *lex_ent;
static int lex_n;

static int cmp_entry(const void *a, const void *b)
{
    const Entry *x = a, *y = b;
    int c = strcmp(x->word, y->word);
    return c ? c : (x->line > y->line) - (x->line < y->line);
}

int lexicon_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    char *buf = xmalloc((size_t)size + 1);
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = 0;

    int nlines = 1;
    for (size_t i = 0; i < got; i++)
        nlines += buf[i] == '\n';
    Entry *ent = xmalloc((size_t)nlines * sizeof *ent);
    int n = 0;
    char *p = buf;
    while (*p) {
        char *eol = strchr(p, '\n');
        char *next = eol ? eol + 1 : p + strlen(p);
        if (eol) {
            if (eol > p && eol[-1] == '\r')
                eol[-1] = 0;
            *eol = 0;
        }
        char *t1 = strchr(p, '\t');
        char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        if (t2 && !strchr(t2 + 1, '\t')) {   /* exactly three fields */
            *t1 = *t2 = 0;
            ent[n].word = p;
            ent[n].phones = t1 + 1;
            ent[n].line = n;
            n++;
        }
        p = next;
    }

    int sorted = 1;
    for (int i = 1; i < n && sorted; i++)
        sorted = strcmp(ent[i - 1].word, ent[i].word) < 0;
    if (!sorted)
        qsort(ent, (size_t)n, sizeof *ent, cmp_entry);

    free(lex_buf);
    free(lex_ent);
    lex_buf = buf;
    lex_ent = ent;
    lex_n = n;
    return 0;
}

static int cmp_key(const void *key, const void *e)
{
    return strcmp(key, ((const Entry *)e)->word);
}

int lexicon_lookup(const char *word, int *syms, int *stress, int max)
{
    char small[128];
    size_t len = strlen(word);
    char *key = len < sizeof small ? small : xmalloc(len + 1);
    for (size_t i = 0; i <= len; i++)
        key[i] = word[i] >= 'a' && word[i] <= 'z' ? (char)(word[i] - 32) : word[i];
    const Entry *e = lex_n ? bsearch(key, lex_ent, (size_t)lex_n, sizeof *lex_ent, cmp_key) : NULL;
    if (e)                                   /* the last of equal words, as a dict */
        while (e + 1 < lex_ent + lex_n && !strcmp(e[1].word, key))
            e++;
    if (key != small)
        free(key);
    if (!e)
        return 0;

    int n = 0;
    const char *s = e->phones;
    while (*s && n < max) {
        while (*s == ' ' || *s == '\t')
            s++;
        if (!*s)
            break;
        const char *tok = s;
        while (*s && *s != ' ' && *s != '\t')
            s++;
        int st = *tok == '\'' ? 1 : *tok == '"' ? 2 : 0;
        while (tok < s && (*tok == '\'' || *tok == '"'))
            tok++;
        char name[8];
        size_t k = (size_t)(s - tok);
        if (k >= sizeof name)
            continue;
        memcpy(name, tok, k);
        name[k] = 0;
        int sym = seg_lookup(name);
        if (sym < 0)
            continue;
        syms[n] = sym;
        stress[n] = st;
        n++;
    }
    return n;
}
