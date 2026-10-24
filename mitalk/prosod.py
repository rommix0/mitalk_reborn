"""PROSOD: segmental durations (chapter 9), plus parsing of the phonemic
input notation used by the book (Figure 8-1 / Table 8-1):

    C: RR 'AA KK * - ER .

C: / F: begin a content / function word; ' and " (or 1 and 2) mark
primary and secondary stress on the following vowel; ! marks emphasis;
* is a morpheme boundary and - a syllable boundary (both carry no
duration); , . )? )N )C )P are syntactic boundaries.
"""

import math
from dataclasses import dataclass, field

from . import tables as T
from .phonet import has

PUNCT = {",", ".", ")?", ")N", ")C", ")P"}
PAUSING = {",": 200, ")C": 200, ".": 400, ")?": 400}  # Rule 1 / Figure 9-1
INITIAL_SILENCE = 30                                   # Figure 9-1

# Legal word-initial consonant clusters, used for consonant stress (sec. 8.4).
# RECONSTRUCTION: the book cites the notion without listing the clusters.
_ONSET2 = {
    "PP RR", "PP LL", "BB RR", "BB LL", "TT RR", "TT WW", "DD RR", "DD WW",
    "KK RR", "KK LL", "KK WW", "GG RR", "GG LL", "GG WW", "FF RR", "FF LL",
    "TH RR", "TH WW", "SH RR", "SS PP", "SS TT", "SS KK", "SS MM", "SS NN",
    "SS LL", "SS WW", "SS FF", "VV RR", "PP YY", "BB YY", "KK YY", "GG YY",
    "MM YY", "FF YY", "VV YY", "HH YY",
}
_ONSET3 = {"SS PP RR", "SS PP LL", "SS TT RR", "SS KK RR", "SS KK WW",
           "SS KK LL", "SS PP YY", "SS KK YY"}


@dataclass
class Phone:
    sym: str
    lex_stress: int = 0      # 0, 1 or 2 (vowels); consonants inherit, sec. 8.4
    stress: bool = False     # segmental stress feature
    emphasis: bool = False
    word: int = 0
    syllable: int = 0        # index within word
    morph: int = 0
    dur: int = 0
    pause: bool = False      # an inserted silence


@dataclass
class Word:
    content: bool
    phones: list = field(default_factory=list)
    boundary: str = ""       # strongest syntactic marker after the word
    nsyl: int = 0


def parse(text):
    """Parse the phonemic notation into a list of Words."""
    words, cur = [], None
    stress, emph = 0, False
    morph = 0
    for tok in text.replace("'", " ' ").replace('"', ' " ').replace("!", " ! ").split():
        if tok in ("C:", "F:"):
            cur = Word(content=(tok == "C:"))
            words.append(cur)
            morph = 0
        elif tok in PUNCT:
            if words:
                words[-1].boundary = _stronger(words[-1].boundary, tok)
        elif tok in ("'", "1"):
            stress = 1
        elif tok in ('"', "2"):
            stress = 2
        elif tok == "!":
            emph = True
        elif tok == "*":
            morph += 1
        elif tok == "-":
            pass
        else:
            sym = tok.upper()
            if sym not in T.TARGETS:
                raise ValueError(f"unknown segment {tok!r}")
            if cur is None:
                cur = Word(content=True)
                words.append(cur)
            cur.phones.append(Phone(sym, stress, emphasis=emph, morph=morph))
            stress, emph = 0, False
    for w in words:
        _syllabify(w)
        _segmental_stress(w)
    return words


def _stronger(a, b):
    order = ["", ")P", ")N", ")C", ",", ")?", "."]
    return a if order.index(a) >= order.index(b) else b


def _syllabify(w):
    """Assign syllable numbers: consonants go with the following nucleus
    (the last syllable takes any final consonants)."""
    nuclei = [i for i, p in enumerate(w.phones) if has(p, "syllabic")]
    w.nsyl = max(len(nuclei), 1)
    syl = 0
    for i, p in enumerate(w.phones):
        later = [n for n in nuclei if n >= i]
        syl = nuclei.index(later[0]) if later else w.nsyl - 1
        p.syllable = syl


def _segmental_stress(w):
    """Sec. 8.4: stressed vowels, plus consonants before them in the same
    morpheme that form a legal word-initial cluster."""
    ph = w.phones
    for i, p in enumerate(ph):
        if has(p, "syllabic"):
            p.stress = p.lex_stress > 0
            if p.stress:
                j = i - 1
                cluster = []
                while j >= 0 and not has(ph[j], "syllabic") and ph[j].morph == p.morph:
                    cand = [ph[j].sym] + cluster
                    if len(cand) == 1 or " ".join(cand) in _ONSET2 | _ONSET3:
                        cluster = cand
                        ph[j].stress = True
                        ph[j].lex_stress = p.lex_stress
                        j -= 1
                    else:
                        break


def durations(words):
    """Apply Rules 1-11 and return the flat list of Phones (with pauses)."""
    flat = []
    flat.append(Phone("SIL", dur=INITIAL_SILENCE, pause=True))
    for wi, w in enumerate(words):
        for p in w.phones:
            p.word = wi
            flat.append(p)
        pause = PAUSING.get(w.boundary)
        if pause:
            flat.append(Phone("SIL", dur=pause, pause=True, word=wi))
    if not flat[-1].pause:
        flat.append(Phone("SIL", dur=PAUSING["."], pause=True))

    phrase_end = _phrase_final_syllables(words)

    for k, p in enumerate(flat):
        if p.pause:
            continue
        w = words[p.word]
        mindur, inhdur = T.DURATIONS.get(p.sym, (40, 60))
        prcnt = 100.0
        syll = has(p, "syllabic")
        prev = flat[k - 1]
        nxt = flat[k + 1]
        before_pause = _before_pause(flat, k)
        phrase_final = (p.word, p.syllable) in phrase_end
        word_final_syl = p.syllable == w.nsyl - 1

        # Rule 2: clause-final lengthening (nucleus and following consonants).
        if before_pause and (syll or _after_last_nucleus(w, p)):
            prcnt *= 1.40
        # Rule 3: non-phrase-final shortening.
        if syll and not phrase_final:
            prcnt *= 0.60
        if (phrase_final and not syll and _after_last_nucleus(w, p)
                and (has(p, "liqglide") or has(p, "nasal"))):
            prcnt *= 1.40
        # Rule 4: non-word-final shortening.
        if syll and not word_final_syl:
            prcnt *= 0.85
        # Rule 5: polysyllabic shortening.
        if syll and w.nsyl > 1:
            prcnt *= 0.80
        # Rule 6: non-initial-consonant shortening.
        if not syll and w.phones.index(p) > 0:
            prcnt *= 0.85
        # Rule 7: unstressed shortening.
        if p.lex_stress == 0 and not p.stress:
            mindur /= 2
        if (p.lex_stress != 1) or not p.stress:
            if syll:
                medial = 0 < p.syllable < w.nsyl - 1
                prcnt *= 0.50 if medial else 0.70
            elif has(p, "liqglide") and has(nxt, "vowel"):
                prcnt *= 0.10
            else:
                prcnt *= 0.70
        # Rule 8: emphasis.
        if p.emphasis and has(p, "vowel"):
            prcnt *= 1.40
        # Rule 9: postvocalic context of vowels; in a postvocalic
        # sonorant-obstruent cluster the obstruent's effect also applies to
        # the sonorant consonant.
        idx = w.phones.index(p)
        cluster_son = (not syll and has(p, "sonorant") and 0 < idx < len(w.phones) - 1
                       and has(w.phones[idx - 1], "vowel")
                       and not has(w.phones[idx + 1], "sonorant"))
        if has(p, "vowel") or cluster_son:
            f = _postvocalic(w, w.phones[idx - 1] if cluster_son else p)
            if not phrase_final:
                f = 70 + 0.3 * f
            prcnt *= f / 100.0
        # Rule 10: shortening in clusters (not across phrase boundaries).
        prev_c = _cluster_neighbour(flat, k, -1)
        next_c = _cluster_neighbour(flat, k, +1)
        if has(p, "vowel"):
            if next_c is not None and has(next_c, "vowel"):
                prcnt *= 1.20
            if prev_c is not None and has(prev_c, "vowel"):
                prcnt *= 0.70
        elif not syll:
            pc = prev_c is not None and not has(prev_c, "syllabic")
            nc = next_c is not None and not has(next_c, "syllabic")
            if pc and nc:
                prcnt *= 0.50
            elif pc or nc:
                prcnt *= 0.70

        dur = (inhdur - mindur) * prcnt / 100.0 + mindur          # Eq. 1
        # Rule 11: lengthening due to plosive aspiration.
        if (p.stress and (has(p, "vowel") or has(p, "sonorant"))
                and has(prev, "plosive") and not has(prev, "voiced")):
            dur += 25
        p.dur = int(math.ceil(dur / 5.0) * 5)                     # round up to 5 ms
    return flat


def _after_last_nucleus(w, p):
    i = w.phones.index(p)
    return all(not has(q, "syllabic") for q in w.phones[i:]) and \
        any(has(q, "syllabic") for q in w.phones[:i])


def _before_pause(flat, k):
    """True if only non-syllabic segments of the same word lie between k and a pause."""
    j = k + 1
    while j < len(flat) and not flat[j].pause:
        if has(flat[j], "syllabic") or flat[j].word != flat[k].word:
            return False
        j += 1
    return j < len(flat)


def _phrase_final_syllables(words):
    """(word, syllable) of the last syllable before each phrase boundary."""
    out = set()
    for wi, w in enumerate(words):
        if w.boundary or wi == len(words) - 1:
            out.add((wi, w.nsyl - 1))
    return out


def _postvocalic(w, p):
    """Rule 9a table: effect of the consonant(s) after a vowel in its word."""
    i = w.phones.index(p)
    rest = w.phones[i + 1:]
    if not rest:
        return 120
    c = rest[0]
    # sonorant-obstruent cluster: the obstruent decides
    if has(c, "sonorant") and len(rest) > 1 and not has(rest[1], "sonorant"):
        c = rest[1]
    if has(c, "syllabic"):
        return 100
    if has(c, "fricative") and has(c, "voiced"):
        return 160
    if has(c, "plosive") and has(c, "voiced"):
        return 120
    if has(c, "nasal"):
        return 85
    if has(c, "plosive"):
        return 70
    return 100


def _cluster_neighbour(flat, k, step):
    """Adjacent segment for Rule 10, or None across a pause or word boundary.

    The text says word boundaries are disregarded, but the book's own
    output (Figure 9-1: 'OW after "the", DD before "man") only matches
    when clusters stop at word boundaries, so that is what we do.
    """
    j = k + step
    if j < 0 or j >= len(flat) or flat[j].pause or flat[j].word != flat[k].word:
        return None
    return flat[j]
