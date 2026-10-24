# AGENTS.md

Notes for coding agents working on this repository. See `README.md` for usage.

## What this is

A reconstruction of MITalk from Allen, Hunnicutt & Klatt, *From Text to Speech:
The MITalk System* (1987). The pipeline follows the book: text normalization ->
PARSER -> PHONO1/2 -> PROSOD -> F0 -> PHONET -> Klatt 1980 synthesizer, 10 kHz.
`mitalk/` is the Python reference; `c/` is a module-for-module C port.

## Ground rules

- **Fidelity first.** Follow the book, including the authors' corrections in
  §8.7. Rules and tables are transcribed in `docs/book/`. Anything the book
  does not specify is marked `RECONSTRUCTION` in the code; keep doing that.
- **Python is the reference; C must match it.** Any behaviour change goes into
  both. Transcriptions and durations must be identical, frames within 1e-4,
  and audio within 1e-6. The noise generator is a shared 32-bit LCG so the two
  match sample for sample.
- **Generated files:** `mitalk/tables.py` comes from `docs/book/` via
  `tools/gen_tables.py`, and `c/tables.h` comes from the Python tables via
  `tools/gen_c_tables.py` (`make -C c tables`). Don't edit them by hand.
- **Lexicon:** `data/lexicon.tsv` is built by `tools/build_lexicon.py`, with
  phonemes from `mitalk.100k.dict` and stress from CMUdict (`data/cmudict.dict`).
  Phoneme recoding: AH0->AX, IH0->IH, ER0->ER, stressed ER->'ER, WH->W, and
  velars fronted (KP/GP) before front vowels.
- **Klatt's `klsyn`** (parwav.c) is a reference for synthesizer behaviour and
  levels only. Don't copy it into this project.
- **Sound quality is judged by ear by the maintainer.** Measure what you can
  (levels against klsyn, spectrograms with `tools/plot_utterance.py`), but
  don't claim a sound is fixed until it has been listened to.

## Build and test

```
python -m pytest tests          # Python tests
make -C c                       # build c/mitalk(.exe); gcc, C99, libm only
make -C c test                  # compare every C stage with Python (tools/compare_c.py)
make -C c dist                  # dist/mitalk-c/ and a zip
```

Run both test suites after any change to `mitalk/` or `c/`.

On Windows the MinGW gcc is 32-bit with x87 floating point. The floating-point
back-end files (`klatt.c`, `phonet.c`, `prosod.c`, `f0.c`) carry
`#pragma GCC target("sse2,fpmath=sse")` so their results match Python; any new
C file doing floating-point math needs it too.

## Not in git

The scanned book PDF (`*.pdf`) and `dist/` are ignored on purpose. Never commit
them.
