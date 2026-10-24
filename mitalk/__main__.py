"""Command line: speak text (or the book's phonemic notation) on the audio
device, or write it to a WAV file with -o.

    python -m mitalk "The old man sat in a rocker."
    python -m mitalk "The old man sat in a rocker." -o out.wav -v
    python -m mitalk -p "F: DH IY C: 'OW LX DD ..."
"""

import argparse

import numpy as np

from . import f0, parser, phono, phonet, prosod, text
from .klatt import SR, synthesize
from .wav import play, write_wav


def speak_phonemes(notation):
    words = prosod.parse(notation)
    flat = prosod.durations(words)
    segs = [phonet.Seg(p.sym, p.dur, p.stress) for p in flat]
    tracks, total = phonet.phonet(segs)
    f0_track = f0.contour(flat, words, total)
    return synthesize(phonet.frames(tracks, total, f0_track)), flat


def text_to_phono2(txt):
    """One PHONO2 string per sentence."""
    return [phono.phono2(parser.parse(sentence)) for sentence in text.normalize(txt)]


def speak_text(txt):
    lines = text_to_phono2(txt)
    audio = [speak_phonemes(line)[0] for line in lines]
    return (np.concatenate(audio) if audio else np.zeros(0)), lines


def main():
    ap = argparse.ArgumentParser(prog="mitalk", description=__doc__.splitlines()[0])
    ap.add_argument("input", help="text to speak (or phonemic notation with -p)")
    ap.add_argument("-p", "--phonemes", action="store_true",
                    help="input is in the book's phonemic notation (Table 8-1)")
    ap.add_argument("-o", "--out", help="write a WAV file instead of playing")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="print the PHONO2 transcription and segment durations")
    args = ap.parse_args()
    if args.phonemes:
        lines = [args.input]
        audio, flat = speak_phonemes(args.input)
        flats = [flat]
    else:
        lines = text_to_phono2(args.input)
        parts = [speak_phonemes(line) for line in lines]
        audio = np.concatenate([a for a, _ in parts]) if parts else np.zeros(0)
        flats = [f for _, f in parts]
    if args.verbose:
        for line, flat in zip(lines, flats):
            print("PHONO2:", line)
            print("PROSOD:", " ".join(f"{p.sym}/{p.dur}" for p in flat))
    if args.out:
        write_wav(args.out, audio, SR)
        print(f"wrote {args.out} ({len(audio) / SR:.2f} s)")
    elif len(audio):
        play(audio, SR)


if __name__ == "__main__":
    main()
