/* MITalk recreation -- C port.  Shared types and module interfaces.
 *
 * The C code mirrors the Python package in ../mitalk module for module;
 * the Python version is the reference and the tools/compare_c.py script
 * checks that both produce the same transcriptions, durations, parameter
 * frames and audio.
 */
#ifndef MITALK_H
#define MITALK_H

#include <stddef.h>
#include "tables.h"

/* ---------------------------------------------------------------- klatt */
#define SR 10000            /* output sample rate */
#define NWS 50              /* samples per 5 ms frame */

/* Synthesizer frame layout (same order as mitalk/klatt.py PARAMS). */
enum kpar { K_AV, K_AVC, K_ASP, K_AF, K_A2, K_A3, K_A4, K_A5, K_A6, K_AB,
            K_AN, K_F0, K_F1, K_F2, K_F3, K_F4, K_FNZ, K_B1, K_B2, K_B3, NKPAR };

/* Render nframes rows of control values to nframes*NWS samples in out. */
void klatt_synthesize(const double (*frames)[NKPAR], int nframes, double *out);

/* Portable noise generator shared with the Python version (LCG). */
typedef struct { unsigned int state; } Rng;
void rng_seed(Rng *r, unsigned int seed);
double rng_uniform(Rng *r);        /* [0, 1) */

/* ------------------------------------------------- utterance structures */
#define MAXB 3                      /* boundary strings: "", ")N", ... */

typedef struct {
    int sym;                        /* enum seg */
    int lex_stress;                 /* 0, 1, 2 */
    int stress;                     /* segmental stress feature (8.4) */
    int emphasis;
    int word;                       /* index into Utterance.words, -1 for pauses */
    int syllable;                   /* index within word */
    int morph;
    int dur;                        /* msec (PROSOD) */
    int start;                      /* msec */
    int pause;                      /* inserted silence */
} Phone;

typedef struct {
    int content;
    int first, n;                   /* phones [first, first+n) in Utterance.phones */
    int nsyl;
    char boundary[MAXB + 1];        /* "", ")P", ")N", ")C", ",", ")?", "." */
} Word;

typedef struct {
    Word *words;  int nwords, capwords;
    Phone *phones; int nphones, capphones;   /* parsed phones, per word */
    Phone *flat;  int nflat, capflat;        /* PROSOD output incl. pauses */
} Utterance;

void utt_init(Utterance *u);
void utt_free(Utterance *u);

/* --------------------------------------------------------------- prosod */
/* Parse the book's phonemic notation (PHONO2 string).  Returns 0, or -1
 * and prints a message on an unknown symbol. */
int prosod_parse(const char *notation, Utterance *u);
/* Rules 1-11; fills u->flat (with leading/trailing silences). */
void prosod_durations(Utterance *u);

/* ------------------------------------------------------------------- f0 */
/* F0 per millisecond, total_ms+1 values (caller frees). */
double *f0_contour(Utterance *u, int total_ms);

/* --------------------------------------------------------------- phonet */
typedef struct {
    int n;                          /* total_ms + 1 */
    double *v[NPAR];                /* one track per PHONET parameter, 1 ms grid */
} Tracks;

/* Draw all tracks for u->flat; returns total ms. */
int phonet_run(Utterance *u, Tracks *tr);
void tracks_free(Tracks *tr);
/* Sample tracks every 5 ms into synthesizer frames (caller frees). */
double (*phonet_frames(const Tracks *tr, int total_ms, const double *f0,
                       int *nframes))[NKPAR];

/* Segment class test (Appendix C.3 classes, plus "stress", "SIL",
 * "glotstop", and segment names), shared by prosod/phono/phonet. */
int seg_has(int sym, int stress, const char *feature);
int seg_lookup(const char *name);   /* enum seg, or -1 */

/* ---------------------------------------------------- front end (text) */
typedef struct {
    char **tok;  int n, cap;        /* words and "," "." "?" tokens */
} Sentence;

/* FORMAT: split text into sentences (caller frees with sentences_free). */
Sentence *text_normalize(const char *text, int *nsent);
void sentences_free(Sentence *s, int n);

/* Lexicon: data/lexicon.tsv.  Returns 0 on success. */
int lexicon_load(const char *path);
/* Phones of a word; returns count (0 if unknown).  syms/stress up to max. */
int lexicon_lookup(const char *word, int *syms, int *stress, int max);

/* PARSER output word. */
typedef struct {
    const char *text;               /* points into the Sentence; valid while it lives */
    unsigned pos;                   /* bit set of POS_* */
    int tag;                        /* chosen POS_* index */
    int content;
    int level;
    char boundary[MAXB + 1];
} PWord;

int parser_parse(const Sentence *s, PWord **out);   /* returns word count */

/* PHONO1/2: returns a malloc'd PHONO2 string. */
char *phono_phono2(const PWord *w, int n);

/* ---------------------------------------------------------------- misc */
int wav_write(const char *path, const double *x, size_t n, int rate);
short *pcm16(const double *x, size_t n);              /* caller frees */
int audio_play(const double *x, size_t n, int rate);  /* blocks until done */
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);

#endif
