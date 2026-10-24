"""Klatt (1980) cascade/parallel formant synthesizer, as described in
chapter 12 of the MITalk book (CWTRAN + COEWAV).

Input is one row of control parameters per 5 ms frame; output is a
10 kHz waveform.  Structure follows Figure 12-6:

    impulse gen -> RGP -+-> AV  -> RGZ -+-> (cascade) RNP RNZ R1..R5 -+
                        +-> AVS -> RGS -+                             |
    noise -> MOD -+-> AH  -----------------^                          +-> out
                  +-> AF  -> parallel R2..R6, bypass AB --------------+

Two simplifications the book itself allows (p.136): the radiation
characteristic (a first difference) is moved into the voicing source,
and the noise low-pass filter it would cancel is removed.

Each frame is filtered with scipy's lfilter, carrying the resonator
state across frame boundaries exactly as the sample-by-sample
difference equations would (coefficients change once per frame).
"""

import numpy as np
from scipy.signal import lfilter

SR = 10000
NWS = 50  # samples per 5 ms frame

# Variable parameters, in the column order used throughout the package.
PARAMS = ("av", "avc", "asp", "af", "a2", "a3", "a4", "a5", "a6", "ab",
          "an", "f0", "f1", "f2", "f3", "f4", "fnz", "b1", "b2", "b3")
IX = {name: i for i, name in enumerate(PARAMS)}

# Constant parameters (Table 12-1 typical values).  FNP = 250 so it cancels
# Appendix C's default FNZ of 250 Hz on non-nasal sounds (ch. 11 mentions a
# 270 Hz pole for its own CV program).
CONST = dict(FGP=0, BGP=100, FGZ=1500, BGZ=6000, BGS=200,
             F5=3850, B4=250, B5=200, F6=4900, B6=1000,
             FNP=250, BNP=100, BNZ=100, G0=48)

# Source and branch calibration.  The book gives the parallel-branch gain
# corrections only in prose (12.2.7), so the parallel branch follows
# Klatt's parwav.c: unity-DC resonators scaled by his per-formant factors.
# parwav's noise is tilted by a one-pole low-pass (n + 0.75*n[-1]) while
# the book's is flat, so each factor also carries that filter's gain at
# the formant (rms over the band for the bypass path).  K_VOICE, K_ASP and
# K_FRIC were then set so steady AA / HH / SS match the levels Klatt's
# klsyn produces for the same parameters (HH -16 dB, SS +2 dB re AA).
K_PAR = {"a2": 0.15, "a3": 0.06, "a4": 0.04, "a5": 0.022, "a6": 0.03}
K_AB = 0.05 / np.sqrt(1 - 0.75 ** 2)
K_VOICE = 2.0e4
K_ASP = 1.45
K_FRIC = 4.626e-3


def parwav_tilt(f):
    """Gain of parwav's noise filter 1/(1 - 0.75 z^-1) at frequency f."""
    return 1.0 / abs(1 - 0.75 * np.exp(-2j * np.pi * f / SR))

BURST_SEED = 4242  # book: generator reset at each burst; seed not given


class Lcg:
    """32-bit linear congruential generator, shared with the C port so the
    two produce identical noise (the book only asks for a pseudo-random
    number generator)."""

    def __init__(self, seed):
        self.state = seed

    def uniforms(self, n):
        out = np.empty(n)
        s = self.state
        for i in range(n):
            s = (1664525 * s + 1013904223) & 0xFFFFFFFF
            out[i] = s / 4294967296.0
        self.state = s
        return out

def db2lin(db):
    """Klatt's dB-to-linear conversion: 87 dB is full scale, <= 0 dB is off."""
    db = np.asarray(db, dtype=float)
    return np.where(db <= 0, 0.0, 10.0 ** ((db - 87.0) / 20.0) * 32767.0)


def resonator_coefs(f, bw):
    """Eq. 2: impulse-invariant second-order resonator, unity gain at DC."""
    r = np.exp(-np.pi * bw / SR)
    c = -r * r
    b = 2.0 * r * np.cos(2.0 * np.pi * f / SR)
    a = 1.0 - b - c
    return a, b, c


class Resonator:
    """y[n] = A x[n] + B y[n-1] + C y[n-2], state carried between frames."""

    def __init__(self):
        self.y1 = self.y2 = 0.0

    def run(self, x, f, bw, gain=1.0):
        a, b, c = resonator_coefs(f, bw)
        zi = [b * self.y1 + c * self.y2, c * self.y1]
        y, _ = lfilter([a * gain], [1.0, -b, -c], x, zi=zi)
        if len(y) >= 2:
            self.y1, self.y2 = y[-1], y[-2]
        return y


class AntiResonator:
    """y[n] = A' x[n] + B' x[n-1] + C' x[n-2], with A'=1/A, B'=-B/A, C'=-C/A."""

    def __init__(self):
        self.x1 = self.x2 = 0.0

    def run(self, x, f, bw):
        a, b, c = resonator_coefs(f, bw)
        a0, b0, c0 = 1.0 / a, -b / a, -c / a
        zi = [b0 * self.x1 + c0 * self.x2, c0 * self.x1]
        y, _ = lfilter([a0, b0, c0], [1.0], x, zi=zi)
        if len(x) >= 2:
            self.x1, self.x2 = x[-1], x[-2]
        return y


def synthesize(frames, const=None):
    """Render an (n_frames, len(PARAMS)) array of control values to audio."""
    k = dict(CONST, **(const or {}))
    frames = np.asarray(frames, dtype=float)
    rng = Lcg(BURST_SEED)

    rgp, rgps = Resonator(), Resonator()   # RGP for the AV and AVS branches
    rgs, rgz = Resonator(), AntiResonator()
    rnp, rnz = Resonator(), AntiResonator()
    casc = [Resonator() for _ in range(5)]
    par = {name: Resonator() for name in ("a2", "a3", "a4", "a5", "a6")}

    out = np.zeros(len(frames) * NWS)
    t0 = 0            # current glottal period in samples (0 = no voicing)
    nper = 0          # position within the current period
    av_lin = avs_lin = 0.0
    last_voice = 0.0
    prev_af = prev_asp = 0.0

    for fi, p in enumerate(frames):
        P = {name: p[i] for name, i in IX.items()}

        # Voicing source: unit impulses; AV and AVS latch at each impulse
        # (12.1.16) and the next period comes from the F0 then in effect.
        imp = np.zeros(NWS)
        g_av = np.empty(NWS)
        g_avs = np.empty(NWS)
        modulate = np.ones(NWS)
        for n in range(NWS):
            if nper >= t0:
                if P["f0"] > 0 and P["av"] + P["avc"] > 0:
                    t0 = max(2, int(np.floor(SR / P["f0"] + 0.5)))
                    imp[n] = 1.0
                    av_lin = db2lin(P["av"]) * K_VOICE
                    avs_lin = db2lin(P["avc"]) * K_VOICE
                else:
                    t0 = 1
                    av_lin = avs_lin = 0.0
                nper = 0
            g_av[n], g_avs[n] = av_lin, avs_lin
            # 50% square-wave noise modulation while voiced (12.1.14).
            if av_lin > 0 and nper >= t0 // 2:
                modulate[n] = 0.5
            nper += 1

        # AV and AVS scale the impulse entering RGP, not RGP's output: the
        # filtered pulse is still decaying when the next one arrives, and
        # rescaling that tail at an impulse puts a step in the flow (a
        # spike after the radiation difference) at every AV change.  RGP
        # is linear, so this is the same AV/AVS-gated source without the
        # discontinuity; each branch gets its own copy of RGP.
        voice = (rgz.run(rgp.run(imp * g_av, k["FGP"], k["BGP"]), k["FGZ"], k["BGZ"])
                 + rgs.run(rgps.run(imp * g_avs, k["FGP"], k["BGP"]), 0, k["BGS"]))
        # Radiation characteristic moved into the source (first difference).
        voice_d = np.diff(voice, prepend=last_voice)
        last_voice = voice[-1]

        # Noise: sum of 16 uniform numbers, modulated; AF/AH ramp linearly
        # over the frame, except AF jumps (and the generator resets) on a
        # rise of more than 50 dB, for plosive bursts (12.1.17).
        if P["af"] - prev_af > 50:
            rng = Lcg(BURST_SEED)
            af_ramp = np.full(NWS, db2lin(P["af"]))
        else:
            af_ramp = np.linspace(db2lin(prev_af), db2lin(P["af"]), NWS,
                                  endpoint=False)
        asp_ramp = np.linspace(db2lin(prev_asp), db2lin(P["asp"]), NWS,
                               endpoint=False)
        prev_af, prev_asp = P["af"], P["asp"]
        noise = (rng.uniforms(NWS * 16).reshape(NWS, 16).sum(axis=1) - 8.0) * modulate

        # Cascade branch: voicing + aspiration through RNP, RNZ, R1..R5.
        x = voice_d + noise * asp_ramp * K_ASP
        x = rnp.run(x, k["FNP"], k["BNP"])
        x = rnz.run(x, P["fnz"], k["BNZ"])
        formants = ((P["f1"], P["b1"]), (P["f2"], P["b2"]),
                    (P["f3"], P["b3"]), (P["f4"], k["B4"]),
                    (k["F5"], k["B5"]))
        for res, (f, bw) in zip(casc, formants):
            x = res.run(x, f, bw)
        cascade = x

        # Parallel branch (SW=0): frication only.  Outputs summed with
        # alternating signs as in Klatt's code; Fig. 12-6 marks no signs.
        fric = noise * af_ramp * K_FRIC
        pf = {"a2": (P["f2"], P["b2"]), "a3": (P["f3"], P["b3"]),
              "a4": (P["f4"], k["B4"]), "a5": (k["F5"], k["B5"]),
              "a6": (k["F6"], k["B6"])}
        parallel = db2lin(P["ab"]) * K_AB * fric
        sign = -1.0
        for name in ("a2", "a3", "a4", "a5", "a6"):
            f, bw = pf[name]
            g = db2lin(P[name]) * K_PAR[name] * parwav_tilt(f)
            parallel = parallel + sign * par[name].run(fric, f, bw, g)
            sign = -sign

        out[fi * NWS:(fi + 1) * NWS] = cascade + parallel

    return out * db2lin(k["G0"]) / 32767.0
