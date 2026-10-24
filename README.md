# MITalk Reborn

A working reconstruction of the MIT text-to-speech system described in
Allen, Hunnicutt & Klatt, *From Text to Speech: The MITalk System* (1987).

This project has been made with the assistance of Claude Opus 5.5.

## Status

- **Back end (done, first pass):** phonemic input -> PROSOD durations (ch. 9)
  -> F0 contour (ch. 10) -> PHONET parameter rules (Appendix C) -> Klatt 1980
  cascade/parallel formant synthesizer (ch. 12) -> 10 kHz WAV.
- **Front end (done, first pass):** text normalization -> part-of-speech and
  phrase parser (ch. 4, simplified: closed-class word list + the book's default
  and suffix rules, longest noun group / verb group) -> lexicon lookup
  (`mitalk.100k.dict` phonemes with CMUdict stress) -> phonemic recoding
  (8.2.1, the authors' 8.7 corrections) -> segmental rules (8.5), velar fronting.

## Usage

```
python -m mitalk "The old man sat in a rocker."
python -m mitalk "The old man sat in a rocker." -o old_man.wav -v
```

Speech plays on the default audio device; `-o` writes a 10 kHz WAV file
instead.  `-v` prints the PHONO2 transcription (as in Figure 8-1) and PROSOD
durations.
With `-p` the input is the book's phonemic notation (Table 8-1):

```
python -m mitalk -p "F: DH IY C: 'OW LX DD C: MM 'AE NN )N C: SS 'AE DX F: IH NN F: AX C: RR 'AA KK * - ER ." -o old_man.wav -v
```

`C:`/`F:` start a content/function word, `'` and `"` mark primary and
secondary stress, `* -` are morph/syllable boundaries, and `, . )? )N )C`
are syntactic boundaries.

## C port

`c/` is a standalone command-line version, module for module the same as the
Python package and checked against it: identical transcriptions and durations,
and byte-identical WAV files. It is about 50x faster.

```
make -C c                 # gcc (MinGW on Windows) or clang; C99, needs only libm
c/mitalk "The old man sat in a rocker."
c/mitalk "The old man sat in a rocker." -o old_man.wav -v
make -C c test            # compare every stage with the Python version
make -C c dist            # dist/mitalk-c/ and dist/mitalk-c-<target>.zip
```

Playback uses the Windows waveOut API, or `aplay` / `paplay` / `afplay`
elsewhere.  The dist zip holds the program, `lexicon.tsv`, a README and the
CMU dictionary license; the program finds `lexicon.tsv` next to itself.

It reads `data/lexicon.tsv` (or `../data/lexicon.tsv`; use `-d path` elsewhere).
`c/tables.h` is generated from the Python tables by `tools/gen_c_tables.py`.

## Layout

- `mitalk/klatt.py` - formant synthesizer
- `mitalk/phonet.py` - Appendix C rules and track drawing
- `mitalk/prosod.py` - input parsing and duration rules
- `mitalk/f0.py` - pitch contour
- `mitalk/text.py`, `mitalk/parser.py`, `mitalk/phono.py`, `mitalk/lexicon.py` - front end
- `data/lexicon.tsv` - built by `tools/build_lexicon.py` from `mitalk.100k.dict`
  and `data/cmudict.dict` (CMUdict, BSD license in `data/cmudict.LICENSE`)
- `mitalk/tables.py` - generated from `docs/book/` by `tools/gen_tables.py`
- `docs/book/` - transcriptions of the relevant chapters (tables and rules
  exact; prose paraphrased)
- `tools/plot_utterance.py` - spectrogram + parameter-track plot for debugging

Tests: `python -m pytest tests` (or run the `test_*` functions directly).

Anything not specified by the book is marked `RECONSTRUCTION` in the code.
