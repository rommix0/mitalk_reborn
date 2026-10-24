"""PHONET: phonetic segments -> synthesizer control parameters.

Implements the context-dependent rules of Appendix C.  For each segment
the state variables (Target, Oldval, Tcf, Tcb, Bper, Bvalf, Bvalb,
Trantype, Cumdur, Mintime, ...) are initialized, the rules are applied in
the printed order, and the parameter tracks are then drawn with the four
smoothing templates of Figure 11-6.

Tracks are drawn on a 1 ms grid and sampled every 5 ms for the
synthesizer.  F0 is drawn separately by the f0 module (ch. 10), so the
f0 rules of Appendix C are not applied here.

Where the book is ambiguous or silent, the choice made is marked
"RECONSTRUCTION" in a comment.
"""

import math
from dataclasses import dataclass, field

import numpy as np

from . import tables as T

# Parameter names as used in Appendix C (f0 handled by the f0 module).
PARS = ("av", "avc", "asp", "af", "a2p", "a3p", "a4p", "a5p", "a6p", "ab",
        "an", "f1", "f2", "f3", "f4", "fnz", "b1", "b2", "b3")
AMPS = ("av", "avc", "asp", "af", "a2p", "a3p", "a4p", "a5p", "a6p", "ab", "an")
FORMANTS = ("f1", "f2", "f3", "f4", "fnz", "b1", "b2", "b3")
TABLE_NAME = {"a2p": "a2", "a3p": "a3", "a4p": "a4", "a5p": "a5", "a6p": "a6"}

# Parameter order of Table C-3, which defines ranges such as "av..ab" in
# the rules (f0 is drawn by the f0 module and skipped here).
ORDER = ("av", "avc", "af", "asp", "an", "a2p", "a3p", "a4p", "a5p", "a6p", "ab",
         "f1", "f2", "f3", "f4", "fnz", "b1", "b2", "b3")


def prange(first, last):
    """Parameters first..last inclusive, in Table C-3 order."""
    return ORDER[ORDER.index(first):ORDER.index(last) + 1]


F1_B3 = prange("f1", "b3")
F2_B3 = prange("f2", "b3")
AV_AB = prange("av", "ab")
AF_B3 = prange("af", "b3")

SETSMO, SMODIS, DISSMO, DISCON = "SETSMO", "SMODIS", "DISSMO", "DISCON"
MANNER_RANK = {"vowel": 0, "sonorant": 1, "fricative": 2, "stop": 3}
# Manner numbers for "Mancur - Manlas" (C.9.1), in the order C.4 assigns them.
MANNER_NUM = {"vowel": 1, "stop": 2, "fricative": 3, "sonorant": 4}

ALIASES = {"sonor": "sonorant", "fric": "fricative", "liqgli": "liqglide"}
GLOTTAL_STOPS = {"QQ", "TQ"}  # RECONSTRUCTION: what "glotstop" covers


@dataclass
class Seg:
    sym: str
    dur: int                 # msec, from PROSOD
    stress: bool = False     # segmental stress feature (sec. 8.4)
    start: int = 0           # msec, filled in by phonet()


def has(seg, feature):
    """True if seg carries feature (a C.3 class, a segment name, or stress)."""
    if seg is None:
        seg = SILENCE
    feature = ALIASES.get(feature, feature)
    if feature == "stress":
        return seg.stress
    if feature in ("glotstop", "gltstop"):
        return seg.sym in GLOTTAL_STOPS
    if feature in T.CLASSES:
        return seg.sym in T.CLASSES[feature]
    return seg.sym == feature


def manner(seg):
    if has(seg, "vowel"):
        return "vowel"
    if has(seg, "stop"):
        return "stop"
    if has(seg, "fricative"):
        return "fricative"
    return "sonorant"


SILENCE = Seg("SIL", 0)


def table_targets(seg, nxt):
    """Initial Target values from Tables C-1/C-2 (C.4)."""
    row = T.TARGETS[seg.sym]
    tgt = {p: float(row[TABLE_NAME.get(p, p)]) for p in PARS
           if TABLE_NAME.get(p, p) in row}
    tgt["fnz"] = 250.0
    tgt["an"] = 0.0
    if seg.sym in ("HH", "HX") and nxt is not None and nxt.sym in T.TARGETS:
        nrow = T.TARGETS[nxt.sym]
        for p in ("f1", "f2", "f3", "f4", "b1", "b2", "b3"):
            tgt[p] = float(nrow[p])
    return tgt


@dataclass
class State:
    target: dict
    oldval: dict
    nextar: dict
    diptar: dict
    cumdur: dict
    segdur: dict
    mintime: dict
    trantype: dict
    tcf: dict
    tcb: dict = field(default_factory=dict)
    bper: dict = field(default_factory=dict)
    bvalf: dict = field(default_factory=dict)
    bvalb: dict = field(default_factory=dict)
    aspdur: float = 0.0
    aspdux: float = 0.0
    aspamp: float = 51.0
    aspaml: float = 51.0
    buramp: float = 57.0
    burdur: float = 0.0
    tcenter: dict = field(default_factory=dict)
    tcdips: dict = field(default_factory=dict)
    dip_bval: dict = field(default_factory=dict)
    burst: bool = False


class Tracks:
    """Parameter tracks on a 1 ms grid."""

    def __init__(self, total_ms):
        self.n = total_ms + 1
        self.v = {p: np.zeros(self.n) for p in PARS}

    def at(self, p, t):
        return self.v[p][int(min(max(t, 0), self.n - 1))]

    def line(self, p, t0, v0, t1, v1):
        t0, t1 = int(round(t0)), int(round(t1))
        t0, t1 = max(t0, 0), min(t1, self.n - 1)
        if t1 <= t0:
            return
        self.v[p][t0:t1 + 1] = np.linspace(v0, v1, t1 - t0 + 1)

    def hold(self, p, t0, t1, v):
        t0, t1 = int(round(t0)), int(round(t1))
        t0, t1 = max(t0, 0), min(t1, self.n - 1)
        if t1 >= t0:
            self.v[p][t0:t1 + 1] = v


def phonet(segs):
    """Return (tracks, total_ms) for a list of Seg with durations set."""
    t = 0
    for s in segs:
        s.start = t
        t += s.dur
    total = t
    tr = Tracks(total)

    # The first segment starts from silence.
    for p in PARS:
        tr.hold(p, 0, total, table_targets(SILENCE, None).get(p, 0.0))

    for i, cur in enumerate(segs):
        prev = segs[i - 1] if i > 0 else SILENCE
        prev2 = segs[i - 2] if i > 1 else SILENCE
        nxt = segs[i + 1] if i + 1 < len(segs) else SILENCE
        st = init_state(cur, prev, nxt, tr)
        general_rules(st, cur, prev, nxt)
        # AXP is in no C.3 class, yet the sonorant and vowel sections have
        # rules for [+AXP], so it takes both sections.
        if has(cur, "sonorant") or cur.sym == "AXP":
            sonorant_rules(st, cur, prev, prev2, nxt, tr)
        if has(cur, "vowel") or cur.sym == "AXP":
            vowel_rules(st, cur, prev, prev2, nxt)
        if not has(cur, "sonorant") or has(cur, "nasal"):
            obstruent_rules(st, cur, prev, nxt, tr)
        adjustments(st, cur, prev)
        modifications(st, cur, prev)
        draw(st, cur, tr)

    return tr, total


def init_state(cur, prev, nxt, tr):
    """C.4 Initialization."""
    target = table_targets(cur, nxt)
    oldval = {p: tr.at(p, cur.start - 1) if cur.start > 0 else
              table_targets(SILENCE, None).get(p, 0.0) for p in PARS}
    nextar = table_targets(nxt, None) if nxt.sym in T.TARGETS else dict(target)
    dip = T.DIPTAR.get(cur.sym)
    diptar = {"f1": target["f1"], "f2": target["f2"], "f3": target["f3"]}
    if dip:
        diptar = dict(zip(("f1", "f2", "f3"), map(float, dip)))
    prev_start = prev.start if prev is not SILENCE else 0
    # Mintime (how far back smoothing may reach) has no stated default.
    # Formants may reach back to the start of the previous segment
    # (t0 in Figure 11-6).  Amplitudes stop at its midpoint, and their
    # forward ramps end by mid-segment (draw()), so a short segment such
    # as the 55 ms Z of "was better" still reaches its target instead of
    # being eaten by two overlapping 40 ms ramps.  RECONSTRUCTION.
    prev_mid = prev_start + (prev.dur / 2 if prev is not SILENCE else 0)
    st = State(
        target=target, oldval=oldval, nextar=nextar, diptar=diptar,
        cumdur={p: float(cur.start) for p in PARS},
        segdur={p: float(cur.dur) for p in PARS},
        mintime={p: float(prev_mid if p in AMPS else prev_start) for p in PARS},
        trantype={p: SETSMO for p in PARS},
        tcf={p: float(T.TCF_DEFAULT[p]) for p in PARS},
    )
    bp = T.BPER_DEFAULT[(manner(prev), manner(cur))]
    st.bper = {p: float(bp) for p in PARS}
    st.burdur = float(T.BURDUR.get(cur.sym, 0))
    return st


def general_rules(st, cur, prev, nxt):
    """C.5 General rules (f0 rules omitted; see module docstring)."""
    if has(prev, "plosive"):
        st.mintime["af"] = st.cumdur["af"]
    if not has(prev, "voiced"):
        st.trantype["av"] = DISSMO
    if prev.sym != "SIL" and cur.sym == "SIL":
        # breathy offset into a pause
        st.aspdux = 30
        st.oldval["avc"] += 6
        st.aspaml = st.aspamp
    if has(prev, "sonorant") and not has(cur, "voiced"):
        st.aspdux = 10
    if has(prev, "sonorant") and not has(cur, "voiced") and has(cur, "fricative"):
        st.cumdur["af"] -= 20
        st.segdur["af"] += 20
    if has(prev, "sonorant") and not has(cur, "voiced") and has(cur, "stop"):
        st.aspaml -= 6
    if not has(cur, "voiced"):
        st.trantype["av"] = SMODIS
    if has(prev, "stop") and not has(cur, "stop"):
        for p in F1_B3:
            st.tcf[p] = float(T.TCOBST.get(prev.sym, st.tcf[p]))
    if has(prev, "stop") and has(prev, "nasal") and not has(cur, "stop") and has(cur, "voiced"):
        st.trantype["av"] = st.trantype["avc"] = DISCON
        for p in AF_B3:
            st.trantype[p] = DISSMO
    else:
        for p in AV_AB + ("avc",):
            if st.trantype[p] == SETSMO:
                st.trantype[p] = DISSMO
        for p in F1_B3:
            st.trantype[p] = SETSMO


def sonorant_rules(st, cur, prev, prev2, nxt, tr):
    """C.6 Sonorant rules, with C.6.1 aspiration and C.6.2 nonnasal sonorants."""
    tg = st.target
    if has(cur, "round"):
        tg["f4"] -= 100
    if has(cur, "retro") or has(cur, "lateral"):
        tg["f4"] += 300
    if has(prev, "YY") and has(cur, "round"):
        for p in F1_B3:
            st.tcf[p] = 110
    elif has(prev, "liqglide"):
        for p in prange("f1", "b2"):
            st.tcf[p] = 75
    if has(prev, "liqglide") and has(prev, "retro"):
        st.tcf["f3"] = 90
    # printed "-yglide"; read as +yglide  RECONSTRUCTION
    if has(prev, "wglide") or has(prev, "yglide"):
        for p in ("f1", "f2", "f3"):
            st.bper[p] = 35
    if has(prev, "palatal") and has(cur, "round"):
        st.tcf["f2"] += 50

    # C.6.1 aspiration of a preceding unvoiced obstruent intrudes on the sonorant
    if (not has(prev, "voiced") and not has(prev, "glotstop")
            and prev.sym not in ("SIL", "HH")):
        st.aspdur = 40
        st.aspaml = st.aspamp
        if not prev.stress:
            st.aspdur = 20
            st.aspaml -= 3
        if not has(prev, "labial") and not has(cur, "vowel") and not has(cur, "lateral"):
            st.aspdur = 55
        elif not has(cur, "vowel"):
            st.aspdur = 50
        if has(prev2, "voiced") and not cur.stress:
            st.aspdur -= 10
        if cur.sym == "AXP":
            st.aspdur = 70
        # Aspiration starts during the plosive burst.  Printed "Cumdur INC
        # Burdur-5, Aspdur INC Burdur-5"; read as the aspiration starting
        # Burdur-5 ms before the boundary (inside the burst) and lasting that
        # much longer, which is what the rule's comment describes.
        # RECONSTRUCTION.
        overlap = 0.0
        if has(prev, "plosive") and T.BURDUR.get(prev.sym, 0) > 5:
            overlap = T.BURDUR[prev.sym] - 5
        if has(prev, "fricative"):
            st.aspdur = 10 if has(cur, "vowel") else st.segdur["asp"] / 2
        st.aspdur = min(st.aspdur, st.segdur["asp"], st.segdur["av"])
        if has(prev, "alveolar") and has(cur, "retro"):
            st.aspaml += 9
        if st.aspdur > 0:
            t0 = st.cumdur["asp"] - overlap
            t1 = st.cumdur["asp"] + st.aspdur
            tr.hold("asp", t0, t1, st.aspaml)
            tr.hold("av", t0, t1, 0.0)
            tr.hold("avc", t0, t1, 0.0)
            tr.hold("b1", t0, t1, 150.0)
            if has(prev, "fricative"):
                tr.line("asp", t0 - 30, tr.at("asp", t0 - 30), t0, st.aspaml)
            for p in ("asp", "av", "avc", "b1"):
                st.cumdur[p] = t1
                st.segdur[p] = cur.start + cur.dur - t1
                st.oldval[p] = tr.at(p, t1 - 1)
                st.mintime[p] = t1   # keep the aspiration segment intact

    # C.6.2 nonnasal sonorant consonants
    if not has(cur, "vowel") and not has(cur, "nasal"):
        x = st.nextar
        if cur.sym == "LL":
            tg["av"] -= 3
        if cur.sym == "LL" and has(nxt, "sonorant") and not has(nxt, "nasal"):
            tg["f2"] = .9 * tg["f2"] + .1 * x["f2"]
        if cur.sym == "RR":
            st.tcf["f3"] = 75  # "Coart(75)"  RECONSTRUCTION: read as f3 coarticulation time
            tg["f3"] = tg["f2"] + 250
        if cur.sym == "RR" and has(nxt, "sonorant") and not has(nxt, "nasal"):
            tg["f2"] = .75 * tg["f2"] + .25 * x["f2"]
        if cur.sym == "LL" and (not has(nxt, "sonorant") or has(nxt, "nasal")):
            tg["f2"] = .9 * tg["f2"] + .1 * 1450
        if cur.sym == "RR" and (not has(nxt, "sonorant") or has(nxt, "nasal")):
            tg["f2"] = .75 * tg["f2"] + .25 * 1450  # schwa F2  RECONSTRUCTION
        if has(prev, "sonorant") and not has(prev, "nasal") and has(cur, "lateral"):
            for p in ("f1", "f2", "f3"):
                st.bper[p] = 90
        if not has(cur, "liqglide"):
            for p in F1_B3:
                st.tcf[p] = 90
        elif has(prev, "liqglide"):
            for p in F1_B3:
                st.tcf[p] = 50
        else:
            for p in F1_B3:
                st.tcf[p] = 70
        if has(cur, "retro"):
            st.tcf["f3"] = 90


def vowel_rules(st, cur, prev, prev2, nxt):
    """C.7 Vowels and C.7.1 diphthongs."""
    tg, dp = st.target, st.diptar
    # strong forward coarticulation of schwa: [+sonor,-nasal ; X] S [+AXP]_
    if cur.sym == "AXP" and has(prev2, "sonorant") and not has(prev2, "nasal"):
        tg["f2"] = (tg["f2"] + T.TARGETS[prev2.sym]["f2"]) / 2
    if has(cur, "front") and has(cur, "lax") and has(nxt, "velar"):
        dp["f2"] = tg["f2"]
    if (has(cur, "front") or has(cur, "yglide")) and has(nxt, "lateral"):
        dp["f2"] -= 300
    if ((has(cur, "front") or has(cur, "yglide")) and not has(cur, "diphthong")
            and has(nxt, "lateral")):
        dp["f2"] -= 300
    if cur.sym == "YU" and has(nxt, "alveolar") and has(nxt, "plosive"):
        dp["f2"] += 200
    if has(prev, "f2back") and has(cur, "round"):
        st.tcf["f2"] += 50
    if has(cur, "yglide") and has(nxt, "alveolar"):
        dp["f2"] -= 150
    # Centralize a short non-retroflex vowel toward 490, 1450, 2500 Hz by an
    # amount decaying with duration (time constant 60 ms).
    if not has(cur, "retro") and not has(cur, "rglide"):
        k = math.exp(-cur.dur / 60.0)
        for p, neutral in (("f1", 490), ("f2", 1450), ("f3", 2500)):
            tg[p] += (neutral - tg[p]) * k
            dp[p] += (neutral - dp[p]) * k
    if has(cur, "schwa"):
        tg["f3"] = (st.oldval["f3"] + tg["f3"] + st.nextar["f3"]) / 3
    if has(cur, "f2back"):
        st.tcf["f2"] -= 20

    if has(cur, "diphthong") and cur.sym in T.DIPHTHONG:
        tcdiph, tdmid = T.DIPHTHONG[cur.sym]
        seg = st.segdur["f1"]
        for p in ("f1", "f2", "f3"):
            st.tcenter[p] = tdmid / 100.0 * seg
            # Printed Tcdips = Tcdiph * .5 * (1 + Segdur/45).  RECONSTRUCTION:
            # Tcdiph read in units of 0.1 ms, giving 20-90 ms half-widths.
            st.tcdips[p] = tcdiph / 10.0 * .5 * (1 + seg / 45.0)
        if prev.sym == "HH" and has(cur, "lax"):
            for p in ("f1", "f2", "f3"):
                st.tcenter[p] *= .67
        bv = {p: (tg[p] + dp[p]) / 2 for p in ("f1", "f2", "f3")}
        if cur.sym == "OY":
            bv["f1"] += 70
        if cur.sym in ("AY", "OY"):
            bv["f2"] += 200
            bv["f3"] -= 200
        if cur.sym == "YU":
            st.tcenter["f3"] *= .3
        if cur.sym in ("EXR", "IXR"):
            st.tcenter["f3"] *= .6
        st.dip_bval = bv


def obstruent_rules(st, cur, prev, nxt, tr):
    """C.8 Obstruents (and nasals, C.8.4)."""
    tg = st.target
    if has(cur, "plosive") or has(cur, "fricative"):          # C.8.1
        if has(cur, "voiced") and not has(nxt, "vowel"):
            tg["avc"] -= 30
        if has(cur, "palatal"):
            tg["f4"] -= 150
        if has(cur, "alveolar") and has(cur, "plosive") and not has(nxt, "alveolar"):
            vals = (0, 67, 60, 65, 65) if has(nxt, "retro") else (0, 52, 60, 70, 73)
            for p, v in zip(("a2p", "a3p", "a4p", "a5p", "a6p"), vals):
                tg[p] = v
        if has(cur, "velar") and not has(nxt, "vowel"):
            st.buramp -= 5
        if ((not has(prev, "voiced") or not has(prev, "sonorant"))
                and has(cur, "voiced") and has(cur, "plosive")):
            tg["avc"] = 0
    if has(cur, "fricative"):                                  # C.8.2
        # Voiced fricatives: sec. 11.3.2 gives AF=50 (vs 60 voiceless) with
        # AV=47.  Table C-1 has AF=60 and AV=36-40, which buries the voicing
        # ~20 dB under the noise, so ZZ sounds like SS (and lowering AF
        # alone leaves the segment too quiet).  DEVIATION from Table C-1,
        # following the chapter 11 analysis.
        if has(cur, "voiced"):
            tg["af"] -= 10
            tg["av"] = max(tg["av"], 47)
        if nxt.sym == "SIL":
            tg["af"] -= 4
        if (not has(prev, "fricative") and not has(prev, "stop")
                and has(nxt, "vowel") and st.segdur["af"] < 80):
            # "(80 - Segdur[af]) * deltat / 7"  RECONSTRUCTION: deltat = 1 ms
            tg["af"] += (80 - st.segdur["af"]) / 7
        if has(prev, "vowel") and has(cur, "voiced") and has(nxt, "vowel"):
            tg["avc"] = 60
        if has(cur, "palatal") and has(nxt, "vowel") and has(nxt, "round"):
            tg["f2"] -= 50
            tg["f3"] -= 200
    if has(cur, "stop"):                                       # C.8.3
        if not has(prev, "stop"):
            for p in prange("av", "f1"):
                st.trantype[p] = SMODIS
            for p in F2_B3:
                st.trantype[p] = SETSMO
        cob = float(T.TCOBST.get(cur.sym, 80))
        if has(prev, "high"):
            for p in F2_B3:
                st.tcf[p] = 10
            st.tcf["f1"] = 15
        else:
            for p in F2_B3:
                st.tcf[p] = cob
            st.tcf["f1"] = (cob + 20) / 2
        # C.8.3.1 burst at the expense of closure duration
        if has(cur, "plosive") and not has(cur, "glotstop") and \
                not has(nxt, "stop") and nxt.sym != "SIL":
            if cur.sym == "TT" and has(nxt, "retro"):
                st.burdur += 10
            st.burdur = min(st.burdur, st.segdur["af"]) or 5
            if not has(cur, "voiced"):
                st.buramp += 6
            if has(prev, "nasal") and not cur.stress:
                st.buramp -= 6
            elif not cur.stress:
                st.buramp -= 3
            if nxt.sym == "AXP" or has(nxt, "fricative"):
                st.buramp -= 3
            st.burst = True
    if has(cur, "nasal"):                                      # C.8.4
        tg["fnz"] = 450
        st.bper["fnz"] = 100
        for p in AF_B3:
            st.trantype[p] = SMODIS
        st.trantype["av"] = DISCON
        if not has(prev, "voiced"):
            st.trantype["avc"] = st.trantype["av"] = DISSMO


def adjustments(st, cur, prev):
    """C.9 Adjustments: boundary values, with manner-class reversal."""
    # The rules are written for consonant -> following segment.  For the
    # reverse order (e.g. vowel -> stop) the two segments swap roles: the
    # consonant's side of the boundary plays "Oldval", the other segment's
    # target plays "Target".
    # c_val / v_tgt are the state dicts themselves (not copies), so the
    # C.9.2 rules below, written for previous -> current, also reach the
    # boundary value computed at the end.
    swap = MANNER_RANK[manner(cur)] > MANNER_RANK[manner(prev)]
    if swap:
        c, v = cur, prev
        c_val, v_tgt = st.target, st.oldval   # consonant side, other segment
    else:
        c, v = prev, cur
        c_val, v_tgt = st.oldval, st.target
    bp = st.bper

    if has(v, "retro"):
        bp["f2"] = bp["f3"] = 75
    if has(c, "palatal") and not has(c, "sonorant"):
        bp["f2"] = 20
        st.tcf["f2"] += 30

    if has(c, "stop") and not has(v, "stop"):                  # C.9.1
        bp["f2"] = 50  # printed, then overridden on the next line
        bp["f2"] = bp["f3"] = 0
        if has(c, "nasal"):
            mancur, manlas = MANNER_NUM[manner(cur)], MANNER_NUM[manner(prev)]
            bp["fnz"] = 100 if mancur - manlas > 1 else 0
            st.tcf["fnz"] = 150
            bp["f1"] = 0
        if has(c, "labial") and not has(v, "glottal"):
            if has(v, "front"):
                bp["f2"], bp["f3"] = 65, 20
            else:
                bp["f2"], bp["f3"] = 20, 70
            if has(v, "retro"):
                c_val["f3"], bp["f3"] = 1750, 20
        if has(c, "alveolar"):
            if has(v, "lateral"):
                c_val["f2"] = 1050
            elif c.sym == "NN":
                c_val["f2"] = 1400
            else:
                c_val["f2"] = 1600
            c_val["f3"] = 2300 if has(v, "retro") else 2620
        if has(c, "velar"):
            c_val["f2"] = v_tgt["f2"] + (v_tgt["f1"] - 300) * 2
            c_val["f3"] = c_val["f2"] + (800 if has(v, "labial") else 400)
            if has(v, "front") or v.sym == "AA":
                # printed KP-target[f2,f4]; read as f2,f3  RECONSTRUCTION
                c_val["f2"] = T.TARGETS["KP"]["f2"]
                c_val["f3"] = T.TARGETS["KP"]["f3"]
            if c.sym == "NG":
                c_val["f2"] = (c_val["f2"] + v_tgt["f2"]) / 2
                c_val["f3"] = (c_val["f3"] + v_tgt["f3"]) / 2
            if v.sym in ("IY", "IH"):
                c_val["f2"] += 250
                c_val["f3"] += 50
        if has(c, "palvel"):
            c_val["f2"] = T.TARGETS[c.sym]["f2"]
            c_val["f3"] = T.TARGETS[c.sym]["f3"]

    # C.9.2 all segments (written for previous -> current, unswapped)
    if has(prev, "alveolar") and has(prev, "stop") and cur.sym == "WW":
        st.oldval.update(f2=1200, f3=2050, f4=2500)
        st.bper["f4"] = 0
        st.tcf["f4"] = 30
    if (has(prev, "rglide") or has(prev, "retro")) and has(cur, "alveolar"):
        st.target["f2"], st.target["f3"] = 1850, 2200
    if (has(prev, "rglide") or has(prev, "retro")) and has(cur, "velar"):
        st.target["f2"], st.target["f3"] = 1700, 1900
    if has(prev, "velar") and has(cur, "retro"):
        st.oldval["f2"], st.oldval["f3"] = 1100, 1500
    if has(prev, "alveolar") and has(cur, "retro"):
        st.oldval["f3"] = 2100
    if has(prev, "labial") and has(cur, "retro"):
        st.oldval["f3"] = 1700
    if has(prev, "wglide") and has(cur, "velar"):
        # printed "... ELSE Target[f2] <- 1900" has no usable condition; omitted
        st.target["f2"], st.target["f3"] = 900, 2000
    if has(prev, "alveolar") and has(prev, "stop") and has(cur, "f2back"):
        st.oldval["f2"] = 1850

    # Boundary value from the (possibly swapped) consonant-side value and
    # the other segment's target; used on both sides of the boundary.
    st.bvalf = {p: (bp[p] * v_tgt[p] + (100 - bp[p]) * c_val[p]) / 100
                for p in F1_B3}


def modifications(st, cur, prev):
    """C.10 Modifications: final targets, boundary values and Tcb."""
    tg = st.target
    if cur.sym == "DX":
        for p in ("f1", "f2", "f3"):
            tg[p] = (tg[p] + st.oldval[p] + st.nextar[p]) / 3
    if has(prev, "f2back"):
        st.tcf["f2"] -= 20
    # formants at least 200 Hz apart
    for lo, hi in (("f1", "f2"), ("f2", "f3"), ("f3", "f4")):
        if tg[hi] < tg[lo] + 200:
            tg[hi] = tg[lo] + 200
    st.tcb = dict(st.tcf)
    for p in AMPS:
        st.bvalf[p] = (st.bper[p] * tg[p] + (100 - st.bper[p]) * st.oldval[p]) / 100
    if has(prev, "lateral"):
        for p in ("f1", "f2"):
            st.bvalb[p] = st.bvalf[p] - 50
            st.bvalf[p] += 50
    for p in F1_B3:
        st.bvalb.setdefault(p, st.bvalf[p])
    if has(prev, "nasal") and has(cur, "diphthong"):
        for p in ("f1", "f2", "f3"):
            st.bvalb[p] = st.oldval[p]
    for p in AV_AB + ("avc",):
        st.bvalf[p] = (tg[p] + st.oldval[p]) / 2
        st.bvalb[p] = st.bvalf[p]
    if has(prev, "vowel"):
        st.tcb["av"] = 30
    if has(prev, "voiced") and not has(cur, "voiced") and not has(cur, "sonorant"):
        st.bvalb["av"] -= 6
        st.bvalf["av"] = st.bvalb["av"]
    if ((has(prev, "voiced") and (has(prev, "stop") or has(prev, "fricative")))
            and has(cur, "sonorant") and has(cur, "nasal")):
        st.bvalb["av"] -= 4
        st.bvalf["av"] = st.bvalb["av"]
    if has(prev, "glottal"):
        for p in prange("a2p", "b3"):
            st.bvalb[p] = st.bvalf[p] = st.oldval[p] = tg[p]
    if has(prev, "plosive"):
        st.bvalf["af"] = tg["af"]


def draw(st, cur, tr):
    """Draw every parameter track for the current segment (Figure 11-6)."""
    end = cur.start + cur.dur
    for p in PARS:
        t1 = st.cumdur[p]
        kind = st.trantype[p]
        if kind in (SETSMO, SMODIS) and st.tcb.get(p, 0) > 0:
            tb = max(t1 - st.tcb[p], st.mintime[p])
            tr.line(p, tb, tr.at(p, tb), t1, st.bvalb.get(p, st.bvalf[p]))
        tgt = st.target[p]
        if kind in (SETSMO, DISSMO):
            tcf = st.tcf[p]
            if p in AMPS:  # amplitudes reach target by mid-segment (see init_state)
                tcf = min(tcf, (end - t1) / 2)
            tf = t1 + tcf
            v0 = st.bvalf[p]
            if tf > end:  # target not reached inside the segment
                v_end = v0 + (tgt - v0) * (end - t1) / max(tf - t1, 1)
                tr.line(p, t1, v0, end, v_end)
            else:
                tr.line(p, t1, v0, tf, tgt)
                tr.hold(p, tf, end, tgt)
        else:
            tr.hold(p, t1, end, tgt)
        # hold the reached value until a later segment overwrites it
        tr.hold(p, end, tr.n - 1, tr.at(p, end))

    # Diphthong offglide (C.7.1, Figure C-2).
    for p, tc in st.tcenter.items():
        dip = st.diptar[p]
        if abs(dip - st.target[p]) < 1:
            continue
        tcen = cur.start + tc
        half = min(st.tcdips[p], tc, end - tcen)
        if half <= 0:
            continue
        tr.line(p, tcen - half, tr.at(p, tcen - half), tcen, st.dip_bval[p])
        tr.line(p, tcen, st.dip_bval[p], tcen + half, dip)
        tr.hold(p, tcen + half, end, dip)
        tr.hold(p, end, tr.n - 1, dip)

    # Pre-aspiration into a voiceless segment (Figure C-1).
    if st.aspdux > 0:
        ta = cur.start - st.aspdux
        tr.line("asp", ta - 30, tr.at("asp", ta - 30), ta, st.aspaml)
        tr.hold("asp", ta, cur.start, st.aspaml)

    # Plosive burst at the end of the closure (C.8.3.1): af drops 20 dB at
    # the closure onset (smoothed back over 30 ms), is zero through the
    # closure, then the burst fills the last Burdur ms.
    if st.burst:
        tb = end - st.burdur
        t0 = st.cumdur["af"]
        tr.line("af", t0 - 30, tr.at("af", t0 - 30), t0, max(st.oldval["af"] - 20, 0))
        tr.hold("af", t0 + 1, tb, 0.0)
        tr.hold("af", tb, end, st.buramp)
        tr.hold("af", end, tr.n - 1, st.buramp)


def frames(tracks, total_ms, f0_track, step=5):
    """Sample tracks every `step` ms into synthesizer frames (klatt.PARAMS order)."""
    from .klatt import PARAMS
    out_names = {v: k for k, v in TABLE_NAME.items()}
    times = np.arange(0, total_ms, step)
    rows = np.zeros((len(times), len(PARAMS)))
    for j, name in enumerate(PARAMS):
        if name == "f0":
            rows[:, j] = f0_track[times]
        else:
            rows[:, j] = tracks.v[out_names.get(name, name)][times]
    return rows
