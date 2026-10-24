/* Allocation helpers, utterance bookkeeping and WAV output. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mitalk.h"

void *xmalloc(size_t n)
{
    void *p = calloc(1, n ? n : 1);
    if (!p) {
        fprintf(stderr, "mitalk: out of memory\n");
        exit(1);
    }
    return p;
}

void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p) {
        fprintf(stderr, "mitalk: out of memory\n");
        exit(1);
    }
    return p;
}

void utt_init(Utterance *u) { memset(u, 0, sizeof *u); }

void utt_free(Utterance *u)
{
    free(u->words);
    free(u->phones);
    free(u->flat);
    memset(u, 0, sizeof *u);
}

static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }
static void put32(FILE *f, unsigned long v) { put16(f, v & 0xffff); put16(f, v >> 16); }

/* 16-bit samples normalized so the peak is 0.9 of full scale (as wav.py). */
short *pcm16(const double *x, size_t n)
{
    double peak = 0;
    for (size_t i = 0; i < n; i++)
        if (fabs(x[i]) > peak)
            peak = fabs(x[i]);
    double scale = peak > 0 ? 0.9 / peak : 0;
    short *pcm = xmalloc(n * sizeof *pcm);
    for (size_t i = 0; i < n; i++)
        pcm[i] = (short)(long)(x[i] * scale * 32767);   /* truncate, like numpy astype */
    return pcm;
}

/* 16-bit mono WAV. */
int wav_write(const char *path, const double *x, size_t n, int rate)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    short *pcm = pcm16(x, n);
    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + 2 * n);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);                 /* PCM */
    put16(f, 1);                 /* mono */
    put32(f, rate);
    put32(f, 2UL * rate);
    put16(f, 2);
    put16(f, 16);
    fwrite("data", 1, 4, f);
    put32(f, 2 * n);
    for (size_t i = 0; i < n; i++)
        put16(f, (unsigned)pcm[i] & 0xffff);
    free(pcm);
    return fclose(f);
}
