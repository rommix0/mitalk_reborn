"""Check the C port against the Python reference, stage by stage.

    python tools/compare_c.py              (build c/ first: make -C c)
    python tools/compare_c.py --frontend   text -> PHONO2 only
    python tools/compare_c.py --backend    PHONO2 -> durations/frames/audio only
                                           (PHONO2 lines come from Python)

For each test sentence: PHONO2 strings must be identical, PROSOD durations
identical, synthesizer frames equal to 1e-6, and audio equal to 1e-6 of
full scale.
"""

import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from mitalk import f0, klatt, phonet, prosod  # noqa: E402
from mitalk.__main__ import text_to_phono2  # noqa: E402

EXE = ROOT / "c" / ("mitalk.exe" if sys.platform == "win32" else "mitalk")
SENTENCES = [
    "The old man sat in a rocker.",
    "This recording is a demonstration of speech synthesis by rule and automatic text-to-speech conversion.",
    "Prerecorded messages work well for single word response applications.",
    "Is this a question?",
    "Give the keen cat a gift. The butter in the bottle was better.",
    "She sells sea shells, and the fish feeds the cat.",
    "The zoo is busy. Visit these vivid zones with the others.",
    "Dr. Klatt bought 3 kittens for $12.50 in 1979.",
    "O'Brien's dog isn't here; it's gone.",
    "Wait... what?!",
    "Xyzzqk blorft!",
    "The birch canoe slid on the smooth planks. Glue the sheet to the dark blue background.",
]


def run_c(*args):
    r = subprocess.run([str(EXE), *args], capture_output=True, text=True, cwd=ROOT)
    if r.returncode:
        raise RuntimeError(r.stderr)
    return r.stdout


def python_stages(line):
    words = prosod.parse(line)
    flat = prosod.durations(words)
    segs = [phonet.Seg(p.sym, p.dur, p.stress) for p in flat]
    tracks, total = phonet.phonet(segs)
    frames = phonet.frames(tracks, total, f0.contour(flat, words, total))
    return flat, frames, klatt.synthesize(frames)


def main(argv):
    frontend = "--backend" not in argv
    backend = "--frontend" not in argv
    failures = 0
    for text in SENTENCES:
        py_lines = text_to_phono2(text)
        if frontend:
            c_lines = run_c("--dump-phono2", text).splitlines()
            if py_lines != c_lines:
                failures += 1
                print(f"PHONO2 differs for {text!r}\n  py: {py_lines}\n  c:  {c_lines}")
                continue
        for line in py_lines if backend else []:
            flat, frames, audio = python_stages(line)
            c_prosod = run_c("-p", "--dump-prosod", line).split()
            py_prosod = [f"{p.sym}/{p.dur}" for p in flat]
            if c_prosod != py_prosod:
                failures += 1
                print(f"PROSOD differs for {line!r}\n  py: {py_prosod}\n  c:  {c_prosod}")
                continue
            c_frames = np.loadtxt(run_c("-p", "--dump-frames", line).splitlines(), ndmin=2)
            if c_frames.shape != frames.shape or np.max(np.abs(c_frames - frames)) > 1e-4:
                failures += 1
                bad = np.argwhere(np.abs(c_frames - frames) > 1e-4) if c_frames.shape == frames.shape else []
                print(f"FRAMES differ for {line!r}: shape {c_frames.shape} vs {frames.shape}; "
                      f"first bad (frame, param) = {bad[:5].tolist() if len(bad) else 'shape'}")
                continue
            c_audio = np.array(run_c("-p", "--dump-audio", line).split(), dtype=float)
            err = np.max(np.abs(c_audio - audio)) / (np.max(np.abs(audio)) or 1)
            if len(c_audio) != len(audio) or err > 1e-6:
                failures += 1
                print(f"AUDIO differs for {line!r}: rel. max error {err:.2e}")
                continue
        print(f"ok  {text}")
    print("all stages match" if not failures else f"{failures} mismatches")
    return failures


if __name__ == "__main__":
    sys.exit(1 if main(sys.argv[1:]) else 0)
