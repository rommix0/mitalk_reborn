"""Regression tests against numbers printed in the book.

Run:  python -m pytest tests
"""

import numpy as np
from scipy.linalg import solve_toeplitz

from mitalk import klatt, prosod
from mitalk.__main__ import speak_phonemes

FIG_8_1 = "F: DH IY C: 'OW LX DD C: MM 'AE NN )N C: SS 'AE DX F: IH NN F: AX C: RR 'AA KK * - ER ."

# Figure 9-1 PROSOD output (msec), including leading and final silence.
FIG_9_1 = [30, 50, 105, 170, 75, 50, 70, 210, 55, 100, 175, 20, 55, 50, 60, 80, 160, 65, 170, 400]


def test_durations_track_figure_9_1():
    flat = prosod.durations(prosod.parse(FIG_8_1))
    got = [p.dur for p in flat]
    assert len(got) == len(FIG_9_1)
    errors = np.abs(np.array(got) - FIG_9_1)
    # The book's own text and figure disagree by 5 ms in places; most
    # segments should land within 5 ms and none further than 25 ms.
    assert np.mean(errors <= 5) >= 0.6
    assert errors.max() <= 25


def _lpc_formants(x, order=12):
    w = x * np.hamming(len(x))
    r = np.correlate(w, w, "full")[len(w) - 1:len(w) + order + 1]
    a = solve_toeplitz(r[:order], -r[1:order + 1])
    roots = [z for z in np.roots(np.r_[1, a]) if z.imag > 0 and abs(z) > 0.85]
    return sorted(np.angle(z) * klatt.SR / (2 * np.pi) for z in roots)


def test_steady_vowel_formants():
    row = np.zeros(len(klatt.PARAMS))
    for k, v in dict(av=57, f0=120, f1=700, f2=1220, f3=2600, f4=3300,
                     fnz=250, b1=130, b2=70, b3=160).items():
        row[klatt.IX[k]] = v
    audio = klatt.synthesize(np.tile(row, (100, 1)))
    f = _lpc_formants(audio[1000:4000])
    for want in (700, 1220, 2600):
        assert min(abs(np.array(f) - want)) < 40


def _steady_level(**params):
    row = np.zeros(len(klatt.PARAMS))
    for k, v in dict(f0=120, f4=3300, fnz=250, **params).items():
        row[klatt.IX[k]] = v
    x = klatt.synthesize(np.tile(row, (100, 1)))[1000:4000]
    return 20 * np.log10(np.sqrt(np.mean(x ** 2)))


def test_consonant_levels_match_klatts_klsyn():
    """Relative levels (dB re AA) measured from Klatt's klsyn.exe with the
    same parameters: SS +2.0, SH -5.2, FF -24.4, HH -16.3."""
    aa = _steady_level(av=60, f1=700, f2=1220, f3=2600, b1=130, b2=70, b3=160)
    got = {
        "SS": _steady_level(af=60, a5=50, a6=82, f1=400, f2=1400, f3=2700, b1=200, b2=95, b3=220),
        "SH": _steady_level(af=60, a3=60, a4=75, a5=70, a6=70, f1=400, f2=1650, f3=2400,
                            b1=200, b2=110, b3=280),
        "FF": _steady_level(af=60, ab=64, f1=400, f2=1130, f3=2100, b1=225, b2=120, b3=175),
        "HH": _steady_level(asp=60, f1=450, f2=1450, f3=2450, b1=300, b2=160, b3=300),
    }
    for name, ref in {"SS": 2.0, "SH": -5.2, "FF": -24.4, "HH": -16.3}.items():
        assert abs(got[name] - aa - ref) < 1.5, (name, got[name] - aa)


def test_voicing_amplitude_change_does_not_spike():
    """AV rising 40 -> 57 dB must not make any glottal pulse larger than the
    steady 57 dB pulses (a gain step on the decaying flow used to produce
    5-7x spikes at every voicing onset)."""
    row = np.zeros(len(klatt.PARAMS))
    for k, v in dict(f0=137, f1=550, f2=1500, f3=2500, f4=3300, fnz=250,
                     b1=100, b2=90, b3=150).items():
        row[klatt.IX[k]] = v
    rows = np.tile(row, (80, 1))
    rows[:, klatt.IX["av"]] = np.r_[np.linspace(40, 57, 10), np.full(70, 57)]
    audio = klatt.synthesize(rows)
    t0 = round(klatt.SR / 137)
    peaks = [np.max(np.abs(audio[i:i + t0])) for i in range(0, len(audio) - t0, t0)]
    steady = np.median(peaks[-20:])
    assert max(peaks) < 1.3 * steady


def test_voiced_fricatives_get_weaker_frication():
    """Sec. 11.3.2: voiced fricatives AF=50 vs voiceless AF=60."""
    from mitalk import phonet
    def af_peak(sym):
        segs = [phonet.Seg("SIL", 50), phonet.Seg("AA", 150, True), phonet.Seg(sym, 120),
                phonet.Seg("AA", 150), phonet.Seg("SIL", 50)]
        tracks, _ = phonet.phonet(segs)
        return tracks.v["af"][segs[2].start:segs[2].start + segs[2].dur].max()
    assert af_peak("SS") - af_peak("ZZ") >= 9


def test_voiced_fricatives_keep_voicing():
    """Sec. 11.3.2: voiced fricatives have AV=47 (Table C-1 gives 36-40)."""
    from mitalk import phonet
    for sym in ("ZZ", "VV", "DH", "ZH"):
        segs = [phonet.Seg("SIL", 50), phonet.Seg("AA", 150, True), phonet.Seg(sym, 120),
                phonet.Seg("AA", 150), phonet.Seg("SIL", 50)]
        tracks, _ = phonet.phonet(segs)
        mid = segs[2].start + segs[2].dur // 2
        assert tracks.v["av"][mid] >= 46, sym


def test_sentence_renders():
    audio, flat = speak_phonemes(FIG_8_1)
    assert np.isfinite(audio).all()
    assert len(audio) == sum(p.dur for p in flat) // 5 * klatt.NWS


def test_c92_rules_reach_boundary_values():
    """Review fix: C.9.2 Oldval rules (e.g. TT -> WW) must shape the boundary."""
    from mitalk import phonet
    seen = {}
    orig = phonet.modifications

    def capture(st, cur, prev):
        orig(st, cur, prev)
        seen[cur.sym] = (round(st.bvalf["f2"]), round(st.bvalf["f3"]), round(st.bvalf["f4"]))
    phonet.modifications = capture
    try:
        phonet.phonet([phonet.Seg("SIL", 50), phonet.Seg("TT", 80, True),
                       phonet.Seg("WW", 100, True), phonet.Seg("SIL", 50)])
    finally:
        phonet.modifications = orig
    assert seen["WW"] == (1200, 2050, 2500)


def test_final_released_plosive_is_aspirated():
    """Review fix: AXP after a voiceless plosive is aspiration, not voicing."""
    from mitalk import phonet
    words = prosod.parse("C: KP 'AE TT AXP .")
    flat = prosod.durations(words)
    segs = [phonet.Seg(p.sym, p.dur, p.stress) for p in flat]
    tracks, _ = phonet.phonet(segs)
    axp = [s for s in segs if s.sym == "AXP"][0]
    mid = axp.start + axp.dur // 2
    assert tracks.v["av"][mid] == 0 and tracks.v["asp"][mid] > 40


def test_f0_has_no_jumps():
    """Review fix: adjacent accents and continuation rises made F0 step."""
    from mitalk import f0, phonet
    for line in ("C: SS 'IY C: BB 'AA BB .", FIG_8_1, "C: MM 'AE NN F: IH TT ."):
        words = prosod.parse(line)
        flat = prosod.durations(words)
        segs = [phonet.Seg(p.sym, p.dur, p.stress) for p in flat]
        _, total = phonet.phonet(segs)
        track = f0.contour(flat, words, total)
        assert np.max(np.abs(np.diff(track))) < 3, line
