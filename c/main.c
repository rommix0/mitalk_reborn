/* Command line: speak text (or the book's phonemic notation) on the audio
 * device, or write it to a WAV file with -o.
 *
 *   mitalk "The old man sat in a rocker."
 *   mitalk "The old man sat in a rocker." -o out.wav -v
 *   mitalk -p "F: DH IY C: 'OW LX DD ..."
 *
 * Debug dumps used by tools/compare_c.py:
 *   --dump-phono2   print the PHONO2 line of each sentence and stop
 *   --dump-prosod   print segment/duration pairs
 *   --dump-frames   print synthesizer frames (one row per 5 ms)
 *   --dump-audio    print raw samples (one per line, %.9g)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mitalk.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
int _dowildcard = 0;   /* MinGW: do not expand * and ? in arguments */

/* The C runtime hands main() arguments in the ANSI code page; fetch the
 * real command line and convert it to UTF-8, which text.c expects. */
static char **utf8_argv(int *argc)
{
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), argc);
    char **argv = xmalloc((*argc + 1) * sizeof *argv);
    for (int i = 0; i < *argc; i++) {
        int n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = xmalloc(n);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], n, NULL, NULL);
    }
    LocalFree(wargv);
    return argv;
}
#endif

/* "-5 degrees", "-$3": text that starts with a dash, not an option. */
static int looks_like_text(const char *a)
{
    return strchr(a, ' ') || isdigit((unsigned char)a[1]) || a[1] == '$' || a[1] == '.';
}

static const char *USAGE =
    "usage: mitalk [-p] [-o out.wav] [-v] [-d lexicon.tsv] \"text\"\n"
    "  speaks the text on the default audio device\n"
    "  -p   input is the book's phonemic notation (Table 8-1)\n"
    "  -o   write a WAV file instead of playing\n"
    "  -v   print the PHONO2 transcription and PROSOD durations\n"
    "  -d   path to lexicon.tsv (default: next to the program, then data/)\n"
    "  --   treat the next argument as text even if it starts with '-'\n";

/* Load the lexicon: -d path, else lexicon.tsv next to the program (the
 * dist layout), else data/lexicon.tsv beside it or under the cwd. */
static int load_lexicon(const char *explicit_path, const char *argv0)
{
    if (explicit_path)
        return lexicon_load(explicit_path);
    char dir[1024] = "";
#ifdef _WIN32
    (void)argv0;
    GetModuleFileNameA(NULL, dir, sizeof dir);
#else
    snprintf(dir, sizeof dir, "%s", argv0);
#endif
    char *slash = strrchr(dir, '/'), *bslash = strrchr(dir, '\\');
    if (bslash > slash)
        slash = bslash;
    if (slash)
        slash[1] = 0;
    else
        dir[0] = 0;
    const char *rel[] = {"lexicon.tsv", "data/lexicon.tsv", "../data/lexicon.tsv"};
    char path[1200];
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof path, "%s%s", dir, rel[i]);
        if (lexicon_load(path) == 0)
            return 0;
    }
    return lexicon_load("data/lexicon.tsv") && lexicon_load("../data/lexicon.tsv");
}

enum { DUMP_NONE, DUMP_PHONO2, DUMP_PROSOD, DUMP_FRAMES, DUMP_AUDIO };

/* Synthesize one PHONO2 line; appends samples to *audio. */
static int speak_phonemes(const char *line, double **audio, size_t *n, size_t *cap,
                          int verbose, int dump)
{
    Utterance u;
    utt_init(&u);
    if (prosod_parse(line, &u) != 0) {
        utt_free(&u);
        return -1;
    }
    prosod_durations(&u);
    if (verbose || dump == DUMP_PROSOD) {
        printf(verbose ? "PROSOD:" : "");
        for (int i = 0; i < u.nflat; i++)
            printf("%s%s/%d", i || verbose ? " " : "", SEG_NAME[u.flat[i].sym], u.flat[i].dur);
        printf("\n");
        if (dump == DUMP_PROSOD) {
            utt_free(&u);
            return 0;
        }
    }
    Tracks tr;
    int total = phonet_run(&u, &tr);
    double *f0 = f0_contour(&u, total);
    int nframes;
    double (*frames)[NKPAR] = phonet_frames(&tr, total, f0, &nframes);
    if (dump == DUMP_FRAMES) {
        for (int i = 0; i < nframes; i++) {
            for (int k = 0; k < NKPAR; k++)
                printf("%s%.6f", k ? " " : "", frames[i][k]);
            printf("\n");
        }
    } else {
        if (*n + (size_t)nframes * NWS > *cap) {
            *cap = (*n + (size_t)nframes * NWS) * 2;
            *audio = xrealloc(*audio, *cap * sizeof **audio);
        }
        klatt_synthesize((const double (*)[NKPAR])frames, nframes, *audio + *n);
        *n += (size_t)nframes * NWS;
    }
    free(frames);
    free(f0);
    tracks_free(&tr);
    utt_free(&u);
    return 0;
}

int main(int argc, char **argv)
{
    const char *out = NULL, *input = NULL, *lex = NULL;
    int phonemes = 0, verbose = 0, dump = DUMP_NONE;

#ifdef _WIN32
    argv = utf8_argv(&argc);
#endif
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--")) {
            if (i + 1 < argc) input = argv[i + 1];
            break;
        } else if (argv[i][0] == '-' && argv[i][1] && looks_like_text(argv[i])) input = argv[i];
        else if (!strcmp(argv[i], "-p")) phonemes = 1;
        else if (!strcmp(argv[i], "-v")) verbose = 1;
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) lex = argv[++i];
        else if (!strcmp(argv[i], "--dump-phono2")) dump = DUMP_PHONO2;
        else if (!strcmp(argv[i], "--dump-prosod")) dump = DUMP_PROSOD;
        else if (!strcmp(argv[i], "--dump-frames")) dump = DUMP_FRAMES;
        else if (!strcmp(argv[i], "--dump-audio")) dump = DUMP_AUDIO;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            fputs(USAGE, stdout);
            return 0;
        } else if (argv[i][0] == '-' && argv[i][1]) {
            fprintf(stderr, "mitalk: unknown option %s\n%s", argv[i], USAGE);
            return 2;
        } else input = argv[i];
    }
    if (!input) {
        fputs(USAGE, stderr);
        return 2;
    }

    /* Collect the PHONO2 lines to speak. */
    char **lines = NULL;
    int nlines = 0;
    if (phonemes) {
        lines = xmalloc(sizeof *lines);
        lines[0] = strcpy(xmalloc(strlen(input) + 1), input);
        nlines = 1;
    } else {
        if (load_lexicon(lex, argv[0])) {
            fprintf(stderr, "mitalk: cannot read the lexicon (use -d path/to/lexicon.tsv)\n");
            return 1;
        }
        int nsent;
        Sentence *s = text_normalize(input, &nsent);
        lines = xmalloc((nsent ? nsent : 1) * sizeof *lines);
        for (int i = 0; i < nsent; i++) {
            PWord *w;
            int nw = parser_parse(&s[i], &w);
            lines[nlines++] = phono_phono2(w, nw);
            free(w);
        }
        sentences_free(s, nsent);
    }

    if (dump == DUMP_PHONO2) {
        for (int i = 0; i < nlines; i++)
            printf("%s\n", lines[i]);
        return 0;
    }

    double *audio = NULL;
    size_t n = 0, cap = 0;
    for (int i = 0; i < nlines; i++) {
        if (verbose)
            printf("PHONO2: %s\n", lines[i]);
        if (speak_phonemes(lines[i], &audio, &n, &cap, verbose, dump) != 0)
            return 1;
        free(lines[i]);
    }
    free(lines);
    if (dump == DUMP_AUDIO) {
        for (size_t i = 0; i < n; i++)
            printf("%.9g\n", audio[i]);
        return 0;
    }
    if (dump != DUMP_NONE)
        return 0;
    if (!out) {
        if (n && audio_play(audio, n, SR) != 0) {
            fprintf(stderr, "mitalk: cannot play audio (use -o file.wav to write a file)\n");
            return 1;
        }
    } else if (wav_write(out, audio, n, SR) != 0) {
        fprintf(stderr, "mitalk: cannot write %s\n", out);
        return 1;
    } else {
        printf("wrote %s (%.2f s)\n", out, n / (double)SR);
    }
    free(audio);
    return 0;
}
