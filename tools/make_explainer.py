"""Render a short dialogless explainer video of the MITalk pipeline.

    python tools/make_explainer.py [out.mp4]            # needs ffmpeg on PATH
    python tools/make_explainer.py --stills 7,17,32     # PNG frames for review

One sentence goes through every stage; each stage adds a layer to the screen,
and the synthesized audio plays at the end with a playhead across all layers.
"""

import subprocess
import sys
import wave
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import patheffects as pe
from matplotlib.colors import LinearSegmentedColormap
from matplotlib.patches import Rectangle

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from mitalk import f0, parser, phonet, phono, prosod, text  # noqa: E402
from mitalk.klatt import SR, synthesize  # noqa: E402

SENT = "The old man sat in a rocker."
W, H, FPS = 1280, 720, 30
GREEN, DIM, FAINT = "#4dff88", "#2fcf68", "#1f7a42"
FONT = "Consolas"

T_TEXT, T_PARSE, T_LEX, T_PROS, T_F0, T_PHON, T_KLATT, T_FIN = 2.6, 5.4, 9.0, 12.8, 16.4, 19.8, 23.8, 27.8
T_END, T_TOTAL = 33.3, 36.3
PLAYS = (T_KLATT + 0.6, T_FIN + 0.5, T_FIN + 2.9)  # start times of the spoken sentence

STAGES = [
    (T_TEXT, "1/7  TEXT", "Plain English in. Numbers and abbreviations get spelled out."),
    (T_PARSE, "2/7  PARSER", "Tag each word's part of speech and find the phrase boundaries."),
    (T_LEX, "3/7  LEXICON", "A 100,000-word dictionary gives each word's phonemes.  ' marks stress."),
    (T_PROS, "4/7  PROSOD", "Decide how long every sound lasts, in milliseconds."),
    (T_F0, "5/7  F0", "Draw the melody: pitch peaks on stressed syllables and falls at the end."),
    (T_PHON, "6/7  PHONET", "Turn sounds into 19 synthesizer controls every 5 ms. Shown: formants F1-F3."),
    (T_KLATT, "7/7  KLATT SYNTHESIZER", "A 1980 formant synthesizer renders the controls as sound, 10,000 samples a second."),
    (T_FIN, "TEXT IN, SPEECH OUT", "The old man sat in a rocker."),
]

# Screen layout (pixels, y down).
X0, X1 = 110, 1230
Y_BRACKET, Y_WORD, Y_TAG, Y_PHON = 150, 182, 212, 242
BAR0, BAR1 = 270, 302
F0_0, F0_1 = 330, 430
SP_0, SP_1 = 450, 670


def cw(fs):
    """Consolas advance width in pixels at font size fs (pt, 100 dpi)."""
    return 0.55 * fs * 100 / 72


def prog(t, a, b):
    return float(np.clip((t - a) / (b - a), 0, 1))


def ease(x):
    return x * x * (3 - 2 * x)


def lerp(a, b, u):
    return a + (b - a) * u


# ---------------------------------------------------------------- pipeline

def run_pipeline():
    pwords = parser.parse(text.normalize(SENT)[0])
    words = prosod.parse(phono.phono2(pwords))
    flat = prosod.durations(words)
    segs = [phonet.Seg(p.sym, p.dur, p.stress) for p in flat]
    tracks, total = phonet.phonet(segs)
    f0_track = f0.contour(flat, words, total)
    audio = synthesize(phonet.frames(tracks, total, f0_track))
    return pwords, words, flat, tracks, total, f0_track, audio


class Data:
    def __init__(self):
        pwords, words, flat, tracks, total, f0_track, audio = run_pipeline()
        assert len(pwords) == len(words)
        assert len(flat) == 2 + sum(len(w.phones) for w in words)
        self.total = total
        self.words = [w.text for w in pwords]
        self.tags = [w.tag for w in pwords]
        self.bounds = [w.boundary for w in pwords]
        self.tokens = [[("'" if p.lex_stress == 1 else '"' if p.lex_stress == 2 else "") + p.sym
                        for p in w.phones] for w in words]
        # segment start/end ms; flat[0] and flat[-1] are the silences
        starts = np.cumsum([0] + [p.dur for p in flat])
        self.seg = [(starts[i], starts[i + 1], p.stress) for i, p in enumerate(flat)]
        self.word_seg = []  # per word: list of flat indices
        k = 1
        for w in words:
            self.word_seg.append(list(range(k, k + len(w.phones))))
            k += len(w.phones)
        self.f0 = f0_track
        self.v = tracks.v
        self.audio = audio / np.max(np.abs(audio))
        # the screen timeline stops shortly after speech, not after the final silence
        self.span = int(self.seg[-1][0]) + 80
        self.spec = spectrogram(self.audio[:self.span * SR // 1000])
        self.layouts()

    def xt(self, ms):
        return X0 + (X1 - X0) * np.asarray(ms) / self.span

    def layouts(self):
        n = len(self.words)
        line = "> " + " ".join(self.words) + "."
        self.line = line
        # A0: typed sentence; A1: spread-out slots; A2: aligned to time
        off, self.word_off = 2, []
        for w in self.words:
            self.word_off.append(off)
            off += len(w) + 1
        self.a0 = [X0 + (o + len(w) / 2) * cw(22) for o, w in zip(self.word_off, self.words)]
        widths = [max(len(w) * cw(22), len(" ".join(t)) * cw(13), len(g) * cw(11)) + 30
                  for w, t, g in zip(self.words, self.tokens, self.tags)]
        left = (X0 + X1) / 2 - sum(widths) / 2
        self.a1 = []
        for wd in widths:
            self.a1.append(left + wd / 2)
            left += wd
        self.a2 = [float(self.xt((self.seg[ix[0]][0] + self.seg[ix[-1]][1]) / 2)) for ix in self.word_seg]
        # phoneme token positions in the slot layout
        self.tok_x0 = []
        for c, toks in zip(self.a1, self.tokens):
            s = " ".join(toks)
            x, xs = c - len(s) * cw(13) / 2, []
            for tk in toks:
                xs.append(x + len(tk) * cw(13) / 2)
                x += (len(tk) + 1) * cw(13)
            self.tok_x0.append(xs)
        # phrase groups split at syntactic boundaries
        self.groups, g = [], []
        for i in range(n):
            g.append(i)
            if self.bounds[i]:
                self.groups.append(g)
                g = []
        if g:
            self.groups.append(g)


def spectrogram(audio, win=128, hop=8, nfft=256):
    w = np.hanning(win)
    pad = np.concatenate([np.zeros(win // 2), audio, np.zeros(win // 2)])
    cols = [np.abs(np.fft.rfft(pad[i:i + win] * w, nfft)) for i in range(0, len(audio), hop)]
    s = 20 * np.log10(np.array(cols).T + 1e-9)
    s = np.clip((s - (s.max() - 65)) / 65, 0, 1)
    return s  # rows: 0..5000 Hz, cols: time


# ---------------------------------------------------------------- drawing

CMAP = LinearSegmentedColormap.from_list("crt", ["#000000", "#0b3a1c", "#2fcf68", "#c8ffd8"])


class Painter:
    def __init__(self, d):
        self.d = d
        self.fig = plt.figure(figsize=(W / 100, H / 100), dpi=100)
        bg = self.fig.add_axes([0, 0, 1, 1])
        bg.axis("off")
        yy, xx = np.mgrid[0:H, 0:W]
        r = np.hypot((xx - W / 2) / (W * 0.72), (yy - H / 2) / (H * 0.72))
        c0, c1 = np.array([0x0e, 0x27, 0x16]) / 255, np.array([0x02, 0x06, 0x03]) / 255
        bg.imshow(c0 + (c1 - c0) * np.clip(r, 0, 1)[..., None], aspect="auto")
        self.ax = self.fig.add_axes([0, 0, 1, 1])
        ov = self.fig.add_axes([0, 0, 1, 1])
        ov.axis("off")
        over = np.zeros((H, W, 4))
        over[::3, :, 3] = 0.22  # scanlines
        over[..., 3] = np.maximum(over[..., 3], np.clip(r - 0.55, 0, 1) * 1.2)  # vignette
        ov.imshow(over, aspect="auto")
        self.ga = 1.0

    def txt(self, x, y, s, fs=14, color=GREEN, alpha=1.0, ha="left", glow=True, **kw):
        a = alpha * self.ga
        if a <= 0.01 or not s:
            return
        eff = [pe.withStroke(linewidth=4, foreground=color, alpha=0.18 * a)] if glow else None
        # baseline anchoring: a centered line would jump as its glyph extents change
        self.ax.text(x, y + 0.36 * fs * 100 / 72, s, fontsize=fs, color=color, alpha=a, ha=ha, va="baseline",
                     family=FONT, path_effects=eff, **kw)

    def line(self, x, y, color=GREEN, lw=1.6, alpha=1.0, glow=True):
        a = alpha * self.ga
        if a <= 0.01:
            return
        if glow:
            self.ax.plot(x, y, color=color, lw=lw * 4, alpha=0.12 * a, solid_capstyle="round")
        self.ax.plot(x, y, color=color, lw=lw, alpha=a)

    def rect(self, x0, y0, x1, y1, fill=None, falpha=0.0, edge=FAINT, alpha=1.0):
        a = alpha * self.ga
        if a <= 0.01:
            return
        if fill:
            self.ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, color=fill, alpha=falpha * a, lw=0))
        if edge:
            self.ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fill=False, ec=edge, alpha=a, lw=1))

    def frame(self, t):
        ax = self.ax
        ax.cla()
        ax.set_xlim(0, W)
        ax.set_ylim(H, 0)
        ax.axis("off")
        if t < T_TEXT:
            self.title(t)
        elif t < T_END + 0.6:
            self.ga = prog(t, T_TEXT, T_TEXT + 0.3) * (1 - prog(t, T_END, T_END + 0.6))
            self.main(t)
        else:
            self.ga = prog(t, T_END + 0.6, T_END + 1.1) * (1 - prog(t, T_TOTAL - 0.6, T_TOTAL))
            self.endcard()
        self.ga = 1.0
        self.fig.canvas.draw()
        return self.fig.canvas.buffer_rgba()

    def title(self, t):
        self.ga = 1 - prog(t, T_TEXT - 0.45, T_TEXT - 0.05)
        s = "MITALK REBORN"
        n = int(len(s) * prog(t, 0.2, 1.2))
        self.txt(W / 2 - len(s) * cw(48) / 2, 320, s[:n] + ("█" if n < len(s) or t % 0.9 < 0.45 else ""),
                 fs=48, ha="left")
        self.txt(W / 2, 385, "FROM TEXT TO SPEECH  ·  MIT, 1979", fs=16, color=DIM,
                 alpha=prog(t, 1.2, 1.6), ha="center")

    def endcard(self):
        self.txt(W / 2, 300, "MITALK REBORN", fs=44, ha="center")
        self.txt(W / 2, 362, "after Allen, Hunnicutt & Klatt", fs=15, color=DIM, ha="center")
        self.txt(W / 2, 390, "From Text to Speech: The MITalk System (1987)", fs=15, color=DIM, ha="center")
        self.txt(W / 2, 440, "rebuilt in Python and C", fs=13, color=FAINT, ha="center")

    # -------------------------------------------------------------- main

    def playhead_ms(self, t):
        for p in PLAYS:
            ms = (t - p) * 1000
            if -1 <= ms <= self.d.span:
                return ms
        return None

    def main(self, t):
        d = self.d
        ms = self.playhead_ms(t)
        self.txt(40, 32, "MITALK REBORN", fs=10, color=FAINT, glow=False)
        # stage header and pips
        si = max(i for i, s in enumerate(STAGES) if t >= s[0])
        t0, name, cap = STAGES[si]
        u = prog(t, t0, t0 + 0.35)
        self.txt(40, 74, name, fs=26, alpha=u)
        self.txt(42, 108, cap, fs=14, color=DIM, alpha=prog(t, t0 + 0.15, t0 + 0.6))
        for i in range(7):
            on = i <= si
            self.rect(1100 + i * 18, 26, 1112 + i * 18, 38, fill=GREEN, falpha=0.8 if on else 0,
                      edge=GREEN if on else FAINT)

        # words: typed, then spread into slots, then aligned with time
        u1 = ease(prog(t, T_PARSE, T_PARSE + 0.7))
        u2 = ease(prog(t, T_PROS + 0.3, T_PROS + 1.3))
        typed = len(d.line) * prog(t, T_TEXT + 0.3, T_TEXT + 1.6)
        centers = [lerp(lerp(a0, a1, u1), a2, u2) for a0, a1, a2 in zip(d.a0, d.a1, d.a2)]
        cur = None
        if ms is not None:
            for i, ix in enumerate(d.word_seg):
                if d.seg[ix[0]][0] <= ms < d.seg[ix[-1]][1]:
                    cur = i
        for i, (w, c) in enumerate(zip(d.words, centers)):
            vis = int(np.clip(typed - d.word_off[i], 0, len(w)))
            a = 1.0 if (ms is None or i == cur) else 0.45
            self.txt(c - len(w) * cw(22) / 2, Y_WORD, w[:vis], fs=22, alpha=a)
        pa = 1 - u1
        self.txt(X0, Y_WORD, ">", fs=22, color=DIM, alpha=pa)
        if typed >= len(d.line):
            self.txt(d.a0[-1] + len(d.words[-1]) * cw(22) / 2, Y_WORD, ".", fs=22, alpha=pa, ha="left")
        if t < T_PARSE:
            n = int(typed)
            cx = X0 + n * cw(22)
            if n < len(d.line) or t % 0.9 < 0.45:
                self.txt(cx, Y_WORD, "█", fs=22, alpha=0.8)

        # parser: tags and phrase brackets
        for i, (g, c) in enumerate(zip(d.tags, centers)):
            self.txt(c, Y_TAG, g, fs=11, color=DIM, ha="center",
                     alpha=prog(t, T_PARSE + 0.7 + i * 0.18, T_PARSE + 1.0 + i * 0.18) * (0.6 if t > T_PROS else 1))
        ba = prog(t, T_PARSE + 2.2, T_PARSE + 2.6)
        for g in d.groups:
            xa = centers[g[0]] - len(d.words[g[0]]) * cw(22) / 2 - 6
            xb = centers[g[-1]] + len(d.words[g[-1]]) * cw(22) / 2 + 6
            self.line([xa, xa, xb, xb], [Y_BRACKET + 8, Y_BRACKET, Y_BRACKET, Y_BRACKET + 8],
                      color=DIM, lw=1.2, alpha=ba, glow=False)
        mid = [centers[g[-1]] + len(d.words[g[-1]]) * cw(22) / 2 + 14 for g in d.groups]
        for g, x in zip(d.groups, mid):
            self.txt(x, Y_TAG, d.bounds[g[-1]], fs=11, color=GREEN, alpha=ba * (1 - u2))

        # lexicon tokens, which drop into PROSOD's duration bars
        j = 0
        for i, toks in enumerate(d.tokens):
            for k, tk in enumerate(toks):
                fi = d.word_seg[i][k]
                s0, s1, stressed = d.seg[fi]
                ap = prog(t, T_LEX + 0.4 + i * 0.3 + k * 0.08, T_LEX + 0.7 + i * 0.3 + k * 0.08)
                um = ease(prog(t, T_PROS + 0.2 + j * 0.06, T_PROS + 1.0 + j * 0.06))
                x = lerp(d.tok_x0[i][k], float(d.xt((s0 + s1) / 2)), um)
                y = lerp(Y_PHON, (BAR0 + BAR1) / 2, um)
                hot = ms is not None and s0 <= ms < s1
                self.rect(float(d.xt(s0)), BAR0, float(d.xt(s1)), BAR1, fill=GREEN,
                          falpha=0.5 if hot else (0.2 if stressed else 0.07), alpha=um)
                label = tk if um < 0.5 else tk.lstrip("'\"")
                self.txt(x, y, label, fs=lerp(13, 10, um), ha="center",
                         color=GREEN if (stressed or um < 0.5) else DIM, alpha=ap)
                j += 1

        # time axis
        ta = prog(t, T_PHON, T_PHON + 0.4)
        for sec in np.arange(0, d.span / 1000, 0.5):
            x = float(d.xt(sec * 1000))
            self.line([x, x], [SP_1, SP_1 + 5], color=FAINT, lw=1, alpha=ta, glow=False)
            self.txt(x, SP_1 + 16, f"{sec:.1f} s", fs=9, color=FAINT, ha="center", alpha=ta, glow=False)

        voiced = np.asarray(d.v["av"]) > 0
        audible = voiced | (np.asarray(d.v["af"]) > 0) | (np.asarray(d.v["asp"]) > 0)
        msx = np.arange(d.span + 1)
        voiced, audible = voiced[:d.span + 1], audible[:d.span + 1]

        # F0 contour
        fa = prog(t, T_F0, T_F0 + 0.4)
        self.rect(X0, F0_0, X1, F0_1, alpha=fa)
        self.txt(X0 - 10, (F0_0 + F0_1) / 2, "PITCH", fs=9, color=DIM, ha="right", alpha=fa, glow=False)
        reveal = prog(t, T_F0 + 0.4, T_F0 + 2.4)
        if reveal > 0:
            y = F0_1 - (np.asarray(d.f0[:d.span + 1]) - 60) / (170 - 60) * (F0_1 - F0_0)
            yv = np.where(voiced & (msx <= reveal * d.span), y, np.nan)
            self.line(d.xt(msx), yv, lw=2.0)

        # PHONET formants, then the Klatt spectrogram behind them
        pa_ = prog(t, T_PHON, T_PHON + 0.4)
        self.rect(X0, SP_0, X1, SP_1, alpha=pa_)
        self.txt(X0 - 10, (SP_0 + SP_1) / 2, "SOUND", fs=9, color=DIM, ha="right", alpha=pa_, glow=False)
        if t >= PLAYS[0]:
            frac = 1.0 if ms is None or t > PLAYS[0] + d.span / 1000 else ms / d.span
            ncol = int(d.spec.shape[1] * frac)
            if ncol > 1:
                self.ax.imshow(d.spec[:, :ncol], cmap=CMAP, aspect="auto", origin="lower",
                               extent=(X0, float(d.xt(d.span * ncol / d.spec.shape[1])), SP_1, SP_0),
                               alpha=self.ga, interpolation="bilinear")
        reveal = prog(t, T_PHON + 0.5, T_PHON + 2.7)
        dimf = 1 - 0.6 * prog(t, PLAYS[0], PLAYS[0] + 0.5)
        if reveal > 0:
            for p in ("f1", "f2", "f3"):
                f = np.asarray(d.v[p][:d.span + 1], dtype=float)
                y = SP_1 - f / 5000 * (SP_1 - SP_0)
                self.line(d.xt(msx), np.where(audible & (msx <= reveal * d.span), y, np.nan),
                          lw=1.6, alpha=dimf)
            if reveal >= 1:
                last = int(np.nonzero(audible)[0][-1])
                for p in ("f1", "f2", "f3"):
                    f = np.asarray(d.v[p], dtype=float)[:last + 1][audible[:last + 1]]
                    self.txt(float(d.xt(last)) + 10, SP_1 - f[-1] / 5000 * (SP_1 - SP_0), p.upper(), fs=9,
                             color=DIM, alpha=prog(t, T_PHON + 2.7, T_PHON + 3.0) * dimf, glow=False)

        if ms is not None:
            x = float(d.xt(ms))
            self.line([x, x], [Y_BRACKET - 6, SP_1], color="#c8ffd8", lw=1.2, alpha=0.9)


# ---------------------------------------------------------------- audio

def soundtrack(d):
    out = np.zeros(int(T_TOTAL * SR))
    rng = np.random.default_rng(1)

    def add(at, x):
        i = int(at * SR)
        out[i:i + len(x)] += x[:len(out) - i]

    click = rng.uniform(-1, 1, int(0.004 * SR)) * np.exp(-np.arange(int(0.004 * SR)) / 8) * 0.12
    for k in range(len("MITALK REBORN")):
        add(0.2 + 1.0 * k / 13, click)
    for k in range(len(d.line)):
        add(T_TEXT + 0.3 + 1.3 * k / len(d.line), click)
    nb = int(0.07 * SR)
    beep = np.sin(2 * np.pi * 880 * np.arange(nb) / SR) * np.hanning(nb) * 0.05
    for t0, *_ in STAGES[:-1]:
        add(t0, beep)
    for p in PLAYS:
        add(p, d.audio * 0.8)
    return np.clip(out, -1, 1)


def write_wav(path, x):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((x * 32767).astype("<i2").tobytes())


def main():
    d = Data()
    pt = Painter(d)
    if len(sys.argv) > 2 and sys.argv[1] == "--stills":
        for s in sys.argv[2].split(","):
            img = np.asarray(pt.frame(float(s)))
            plt.imsave(ROOT / "dist" / f"still_{float(s):05.1f}.png", img)
        return
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "dist" / "mitalk_explainer.mp4"
    out.parent.mkdir(parents=True, exist_ok=True)
    wav = out.with_suffix(".wav")
    write_wav(wav, soundtrack(d))
    ff = subprocess.Popen(
        ["ffmpeg", "-y", "-loglevel", "error",
         "-f", "rawvideo", "-pix_fmt", "rgba", "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-",
         "-i", str(wav), "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20",
         "-c:a", "aac", "-b:a", "128k", "-ar", "44100", "-shortest", str(out)],
        stdin=subprocess.PIPE)
    n = int(T_TOTAL * FPS)
    for i in range(n):
        ff.stdin.write(bytes(pt.frame(i / FPS)))
        if i % FPS == 0:
            print(f"\r{i / FPS:4.0f} / {T_TOTAL:.0f} s", end="", flush=True)
    ff.stdin.close()
    ff.wait()
    wav.unlink()
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
