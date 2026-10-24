"""F0: fundamental frequency contour (chapter 10, O'Shaughnessy algorithm
as adjusted for MITalk in sec. 10.5).

The book gives the structure and most constants but leaves several
amounts unstated (phrase-initial/final adjustments, the "more
exponential" fall, the wh-question tune); those are marked
RECONSTRUCTION.  Output is an F0 value per millisecond.
"""

import numpy as np

from .phonet import has

BASE_A, BASE_B = 110.0, 125.0      # lower declination line start (Tune A / B)
PEAK_MAX = 190.0
FINAL_A = 75.0 - 10.0              # final statement value, lowered 10 Hz (10.5)
UNACCENTED_BUMP = 5.0
CONT_RISE_BIG, CONT_RISE_SMALL = 16.0, 8.0


def contour(flat, words, total_ms):
    """Return an F0 track (Hz per ms) for the PROSOD output `flat`."""
    question = bool(words) and words[-1].boundary == ")?"
    base0 = BASE_B if question else BASE_A

    t = 0
    for p in flat:
        p.start, t = t, t + p.dur
    voiced = [p for p in flat if not p.pause]
    if not voiced:
        return np.full(total_ms + 1, base0)

    # One accent per content word, on its primary-stressed nucleus.
    accents = []
    for wi, w in enumerate(words):
        if not w.content:
            continue
        nuclei = [p for p in w.phones if has(p, "syllabic")]
        if not nuclei:
            continue
        best = next((p for p in nuclei if p.lex_stress == 1),
                    next((p for p in nuclei if p.lex_stress == 2), nuclei[0]))
        accents.append((wi, best))

    nsyl = sum(w.nsyl for w in words)
    peak0 = min(PEAK_MAX, 150.0 + 1.5 * nsyl)    # RECONSTRUCTION: longer -> higher
    t_end = voiced[-1].start + voiced[-1].dur
    t_beg = voiced[0].start
    # Declination: steeper for Tune A than Tune B.  RECONSTRUCTION: slopes.
    top_drop = 0.30 if not question else 0.15
    base_drop = 0.20 if not question else 0.05

    def top(tm):
        return peak0 * (1 - top_drop * (tm - t_beg) / max(t_end - t_beg, 1))

    def base(tm):
        return base0 * (1 - base_drop * (tm - t_beg) / max(t_end - t_beg, 1))

    order = [syl_key(p) for p in voiced if has(p, "syllabic")]

    def phrase_end(wi):
        """Index of the word that closes the phrase containing word wi."""
        for j in range(wi, len(words)):
            if words[j].boundary:
                return j
        return len(words) - 1

    # Per accent: valley (time, value), peak (time, value), fall (time, value).
    marks = []
    for k, (wi, nuc) in enumerate(accents):
        w = words[wi]
        idx = order.index(syl_key(nuc))
        prev_idx = order.index(syl_key(accents[k - 1][1])) if k else -1
        next_idx = order.index(syl_key(accents[k + 1][1])) if k + 1 < len(accents) else len(order)
        gap_before, gap_after = idx - prev_idx - 1, next_idx - idx - 1

        peak = top(nuc.start)
        span = peak - base(nuc.start)
        if gap_before in (2, 3):
            peak -= span * (0.15 if gap_before == 2 else 0.25)
        if gap_after in (2, 3):
            peak += span * (0.10 if gap_after == 2 else 0.15)
        peak = min(peak, PEAK_MAX)

        # Phrase position is decided by the whole phrase, which may end in
        # function words after this accent.
        pe = phrase_end(wi)
        phrase_initial = k == 0 or phrase_end(accents[k - 1][0]) != pe
        phrase_final = k + 1 == len(accents) or phrase_end(accents[k + 1][0]) != pe
        nuclear = phrase_final and (words[pe].boundary in (",", ".", ")?", ")C")
                                    or pe == len(words) - 1)

        rise = 0.40 * (peak - base0)
        fall = 0.20 * (peak - base(nuc.start))
        if not phrase_initial and not phrase_final:
            rise *= 0.70
            fall *= 0.70
        if phrase_final:
            fall *= 2.0                              # RECONSTRUCTION: "increased"
        if gap_before == 0 and k:
            rise *= 0.60
        rise *= {2: 1.15, 3: 1.20}.get(gap_before, 1.30 if gap_before >= 4 else 1.0)
        # Not implemented, as the book gives no amounts or needs parse nodes
        # this parser does not build: the phrase-initial/final rise
        # adjustments, rise = peak - 95 Hz for words outside any node, and the
        # swapped fall/rise of the middle one of three successive accents.

        # Nuclear stress peaks at the vowel's left boundary; other peaks sit
        # at its right boundary.
        onset = syllable_onset(w, nuc)
        unvoiced_onset = onset is not None and not has(onset, "voiced")
        v0, v1 = nuc.start, nuc.start + nuc.dur
        valley_t = v0 if unvoiced_onset else (onset.start if onset else v0)
        if nuclear:
            peak_t = v0 if valley_t < v0 else v0 + min(10, nuc.dur // 2)
            valley_v = peak - rise
            fall_t = v1
        else:
            peak_t = v1
            valley_v = peak - rise
            if unvoiced_onset:          # extra 20% rise at the vowel's left edge (10.5)
                valley_v += 0.20 * rise
            nxt = _next_segment(flat, nuc)
            fall_t = nxt.start + nxt.dur / 2 if nxt else v1 + 20
        marks.append([valley_t, valley_v, peak_t, peak, fall_t, peak - fall, v0])

    for k in range(len(marks) - 1):
        cur, nxt = marks[k], marks[k + 1]
        # Adjacent accents: the next valley may not sit on the previous peak;
        # put it half way between that peak and the next stressed vowel.
        if nxt[0] <= cur[2] + 10:
            nxt[0] = (cur[2] + nxt[6]) / 2
        # The fall must come before the next accent's valley; for adjacent
        # accents the next valley takes its place.
        if cur[4] >= nxt[0]:
            cur[4] = None
        # Rising endpoint difference: keep F0 flat across the unaccented
        # syllables by moving both ends half way.
        elif cur[5] < nxt[1]:
            cur[5] = nxt[1] = (cur[5] + nxt[1]) / 2

    pts = [(t_beg, (peak0 + base0) / 2)]
    for valley_t, valley_v, peak_t, peak_v, fall_t, fall_v, _ in marks:
        pts += [(valley_t, valley_v), (peak_t, peak_v)]
        if fall_t is not None:
            pts.append((fall_t, fall_v))

    # Terminal: statements fall to the final value; yes/no questions rise
    # to 20% above the highest peak.
    final = 1.2 * max(v for _, v in pts) if question else FINAL_A
    pts.append((t_end, final))

    # Keep time order as built (it is chronological); nudge equal times.
    times, vals = [], []
    for t, v in pts:
        t = max(t, times[-1] + 1) if times else t
        times.append(t)
        vals.append(v)
    f0 = np.interp(np.arange(total_ms + 1), np.array(times, float), np.array(vals, float))

    # Continuation rises over the last syllable of a nonfinal phrase: 16 Hz
    # before a comma or clause break, 8 Hz at other phrase ends when more
    # than five words have passed since the last rise.
    last_cont = -10
    for j, w in enumerate(words[:-1]):
        if not w.phones:
            continue
        big = w.boundary in (",", ")C")
        if big or (w.boundary in (")N", ")P") and j - last_cont > 5):
            amount = CONT_RISE_BIG if big else CONT_RISE_SMALL
            last_syl = [p for p in w.phones if p.syllable == w.nsyl - 1]
            lo, hi = last_syl[0].start, w.phones[-1].start + w.phones[-1].dur
            f0[lo:hi] += np.linspace(0, amount, max(hi - lo, 0))
            tail = f0[hi:hi + 60]          # relax back over the next 60 ms
            tail += np.linspace(amount, 0, len(tail))
            last_cont = j

    # 5 Hz perturbation about the midpoint of flat or falling unaccented
    # syllables (10.5).
    accented = {syl_key(n) for _, n in accents}
    for p in voiced:
        lo, hi = p.start, p.start + p.dur
        if has(p, "syllabic") and syl_key(p) not in accented and hi > lo                 and f0[min(hi, total_ms)] <= f0[lo]:
            mid = p.start + p.dur // 2
            bump = UNACCENTED_BUMP * (1 - np.abs(np.arange(lo, hi) - mid) / max(p.dur / 2, 1))
            f0[lo:hi] += bump
    # Dip at points of glottalization.
    for p in voiced:
        if p.sym in ("QQ", "TQ"):
            f0[p.start:p.start + p.dur] *= 0.75
    return f0


def syl_key(p):
    return (p.word, p.syllable)


def syllable_onset(word, nucleus):
    """First consonant of the nucleus's syllable, or None."""
    for p in word.phones:
        if p.syllable == nucleus.syllable:
            return None if p is nucleus else p
    return None


def _next_segment(flat, p):
    i = flat.index(p)
    return flat[i + 1] if i + 1 < len(flat) else None
