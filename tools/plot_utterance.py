"""Plot spectrogram + PHONET tracks for a phonemic input (debugging aid).

    python tools/plot_utterance.py "C: SS 'AE DX ." out.png
"""

import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from mitalk import f0, phonet, prosod  # noqa: E402
from mitalk.klatt import SR, synthesize  # noqa: E402


def main(text, out):
    words = prosod.parse(text)
    flat = prosod.durations(words)
    segs = [phonet.Seg(p.sym, p.dur, p.stress) for p in flat]
    tracks, total = phonet.phonet(segs)
    f0_track = f0.contour(flat, words, total)
    audio = synthesize(phonet.frames(tracks, total, f0_track))

    fig, ax = plt.subplots(3, 1, figsize=(14, 9), sharex=True)
    norm = audio / (np.max(np.abs(audio)) or 1) + 1e-6 * np.random.randn(len(audio))
    ax[0].specgram(norm, NFFT=128, Fs=SR, noverlap=112, cmap="gray_r", vmin=-110, vmax=-40)
    for p in ("f1", "f2", "f3"):
        ax[0].plot(np.arange(total + 1) / 1000, tracks.v[p], lw=0.8)
    ax[0].set_ylim(0, 5000)
    for p in ("av", "avc", "asp", "af"):
        ax[1].plot(np.arange(total + 1) / 1000, tracks.v[p], label=p)
    ax[1].legend(loc="upper right")
    ax[2].plot(np.arange(total + 1) / 1000, f0_track, label="f0")
    ax[2].legend(loc="upper right")
    for a in ax:
        for s in segs:
            a.axvline(s.start / 1000, color="k", lw=0.3)
    for s in segs:
        ax[1].text((s.start + s.dur / 2) / 1000, 75, s.sym, ha="center", fontsize=8)
    fig.tight_layout()
    fig.savefig(out, dpi=80)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
