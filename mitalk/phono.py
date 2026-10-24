"""PHONO1/PHONO2: words -> broad phonetic transcription (chapter 8).

Word level: dictionary lookup, function-word stress reduction (8.2.3),
and the phonemic recoding of 8.2.1 (r-colored vowels, syllabic
consonants, YU, affricates) plus the reduced-vowel choices from the
authors' corrections in 8.7.

Sentence level: the six segmental rules of 8.5, velar fronting, and the
AXP release vowel before pauses.

The result is a string in the book's PHONO2 notation, which prosod.parse
reads.
"""

from . import lexicon
from .phonet import has
from .prosod import Phone  # noqa: F401  (documented output consumer)

R_COLOR = {"IH": "IXR", "IY": "IXR", "EH": "EXR", "EY": "EXR", "AE": "EXR",
           "AA": "AXR", "AH": "AXR", "AO": "OXR", "OW": "OXR", "UH": "UXR",
           "UW": "UXR"}
FRONT = {"AE", "EH", "EXR", "EY", "IH", "IX", "IXR", "IY", "YU"}  # C.3 "front"
DETERMINERS = {"a", "an", "the", "this", "that", "these", "those", "every",
               "any", "each", "no", "some", "my", "your", "his", "her", "its",
               "our", "their"}


# Function-word forms printed in the book's 8.7 output where the dictionary
# gives a different (citation) form.
FUNCTION_FORMS = {
    "was": [("WW", 0), ("AH", 0), ("ZZ", 0)],
    "and": [("AE", 0), ("NN", 0)],
}


def word_phones(word, content, keep_secondary=False):
    """[(sym, stress)] for one word, recoded at the word level."""
    if not content and word.lower() in FUNCTION_FORMS:
        return list(FUNCTION_FORMS[word.lower()])
    phones = lexicon.lookup(word)
    if phones is None and word.lower().endswith("'s"):
        base = lexicon.lookup(word[:-2])
        if base:
            last = base[-1][0]
            ending = ([("IX", 0), ("ZZ", 0)] if last in ("SS", "ZZ", "SH", "ZH", "CH", "JJ")
                      else [("SS", 0)] if last in ("PP", "TT", "KK", "FF", "TH")
                      else [("ZZ", 0)])
            phones = base + ending
    if phones is None:
        if word.lower().endswith("'s") and len(word) > 2:
            phones = _spell(word[:-2]) + [("ZZ", 0)]
        else:
            phones = _spell(word)
    phones = list(phones)
    spelling = word.lower()

    if not content:
        # 8.2.3: function words lose primary stress; polysyllabic ones keep
        # a secondary stress on their strongest syllable.
        nuclei = [i for i, (s, _) in enumerate(phones) if has_sym(s, "syllabic")]
        for i, (s, st) in enumerate(phones):
            if st:
                phones[i] = (s, 2 if (len(nuclei) > 1 or keep_secondary) and st == 1 else 0)
        phones = [("AX", 0) if (s == "AH" and not st) else (s, st) for s, st in phones]

    phones = _reduced_vowels(phones, spelling)
    phones = _syllabics(phones)
    phones = _r_colored(phones, spelling)
    return phones


def has_sym(sym, feature):
    return has(_S(sym), feature)


class _S:
    def __init__(self, sym):
        self.sym, self.stress = sym, False


def _reduced_vowels(phones, spelling):
    """Authors' 8.7 corrections: IX only in -es, -ed and -ish endings;
    other unstressed IX is IH in a closed final syllable, else AX."""
    nuclei = [i for i, (s, _) in enumerate(phones) if has_sym(s, "syllabic")]
    out = list(phones)
    for i, (s, st) in enumerate(phones):
        if st or s not in ("IX", "IH"):
            continue
        final = nuclei and i == nuclei[-1]
        rest = [p for p, _ in phones[i + 1:]]
        ending = (final and ((rest == ["ZZ"] and spelling.endswith(("es", "'s"))) or
                             (rest == ["DD"] and spelling.endswith("ed")) or
                             (rest == ["SH"] and spelling.endswith("ish"))))
        if ending:
            out[i] = ("IX", 0)
        elif s == "IX":
            out[i] = ("IH", 0) if final and rest else ("AX", 0)
    return out


def _syllabics(phones):
    """8.2.1 #5: word-final unstressed AX+LL -> EL; AX+NN after TT/DD -> EN
    (the corrections in 8.7 keep EN only after T/D).  A plural or past
    ending may follow ("kittens", "bottles")."""
    out = list(phones)
    end = len(out)
    if end >= 4 and out[-1][0] in ("ZZ", "SS", "DD") and out[-2][0] in ("LL", "NN"):
        end -= 1
    if end >= 3 and out[end - 2] == ("AX", 0):
        prev = out[end - 3][0]
        if out[end - 1][0] == "LL" and not has_sym(prev, "syllabic"):
            out[end - 2:end] = [("EL", 0)]
        elif out[end - 1][0] == "NN" and prev in ("TT", "DD"):
            out[end - 2:end] = [("EN", 0)]
    return out


def _r_colored(phones, spelling):
    """8.2.1 #3: vowel + RR (not followed by a vowel) -> IXR EXR AXR OXR UXR;
    after AY/AW the R is the postvocalic RX ("fire").  "-ire"/"-our"
    words with a two-syllable dictionary entry (AY ER) also get RX, while
    "buyer"/"player" stay EY+ER, AY+ER (8.2.1 #4)."""
    out = []
    i = 0
    while i < len(phones):
        s, st = phones[i]
        nxt = phones[i + 1][0] if i + 1 < len(phones) else None
        after = phones[i + 2][0] if i + 2 < len(phones) else None
        if nxt == "RR" and (after is None or not has_sym(after, "syllabic")):
            if s in R_COLOR:
                out.append((R_COLOR[s], st))
                i += 2
                continue
            if s in ("AY", "AW"):
                out += [(s, st), ("RX", 0)]
                i += 2
                continue
        if (s in ("AY", "AW") and nxt == "ER" and phones[i + 1][1] == 0
                and ("ir" in spelling or "our" in spelling or "yr" in spelling)):
            out += [(s, st), ("RX", 0)]
            i += 2
            continue
        out.append((s, st))
        i += 1
    return out


LETTERS = {
    "a": "EY", "b": "BB IY", "c": "SS IY", "d": "DD IY", "e": "IY", "f": "EH FF",
    "g": "JJ IY", "h": "EY CH", "i": "AY", "j": "JJ EY", "k": "KK EY", "l": "EH LL",
    "m": "EH MM", "n": "EH NN", "o": "OW", "p": "PP IY", "q": "KK YU", "r": "AA RR",
    "s": "EH SS", "t": "TT IY", "u": "YU", "v": "VV IY", "w": "DD AH BB AX LL YU",
    "x": "EH KK SS", "y": "WW AY", "z": "ZZ IY",
}


def _spell(word):
    """Unknown word: spell it letter by letter, secondary stress on each
    letter and primary on the last.  RECONSTRUCTION: MITalk used
    letter-to-sound rules (ch. 6) that are only sampled in the book."""
    out = []
    for ch in word.lower():
        if ch in LETTERS:
            syms = LETTERS[ch].split()
            first_vowel = next(i for i, x in enumerate(syms) if has_sym(x, "syllabic"))
            out += [(x, 2 if i == first_vowel else 0) for i, x in enumerate(syms)]
    if not out:
        return [("AX", 0)]
    last = max(i for i, (_, st) in enumerate(out) if st)
    out[last] = (out[last][0], 1)
    return out


def phono2(pwords):
    """Sentence-level rules; returns the PHONO2 string for prosod.parse."""
    # Flatten with word indices.
    segs = []  # dicts: sym, stress(0/1/2), word, boundary_after (on last seg)
    for wi, w in enumerate(pwords):
        # 8.2.2: secondary stress on non-personal pronouns and demonstratives
        ph = word_phones(w.text, w.content, keep_secondary=w.level == 6)
        if w.text.lower() == "the":
            ph = [("DH", 0), ("AX", 0)]
        for k, (s, st) in enumerate(ph):
            segs.append({"sym": s, "st": st, "word": wi, "first": k == 0,
                         "last": k == len(ph) - 1})

    def nxt(i):
        return segs[i + 1] if i + 1 < len(segs) else None

    def prv(i):
        return segs[i - 1] if i > 0 else None

    def boundary_after(i):
        s = segs[i]
        return pwords[s["word"]].boundary if s["last"] else ""

    def phrase_break(i):
        return boundary_after(i) in (",", ".", ")?", ")C", ")N")

    def stressed_onset(i):
        """Segment i starts a word whose first vowel is stressed, with only
        consonants before it (8.4: the onset shares the vowel's stress)."""
        j = i
        while j < len(segs) and segs[j]["word"] == segs[i]["word"]:
            if has_sym(segs[j]["sym"], "syllabic"):
                return segs[j]["st"] > 0
            j += 1
        return False

    for i, s in enumerate(segs):
        n, p = nxt(i), prv(i)
        # Rule 6: "the" -> DH IY before a vowel.
        if pwords[s["word"]].text.lower() == "the" and s["sym"] == "AX" and n \
                and has_sym(n["sym"], "syllabic") and n["sym"] != "YU" and not phrase_break(i):
            s["sym"] = "IY"
        # Rule 1: postvocalic LL -> LX unless a stressed vowel follows in the word.
        if s["sym"] == "LL" and p and has_sym(p["sym"], "syllabic") and p["word"] == s["word"]:
            if not (n and n["word"] == s["word"] and has_sym(n["sym"], "syllabic") and n["st"]):
                s["sym"] = "LX"
        # Rule 2: flapping (not across phrase/clause boundaries).  A
        # word-initial T/D never flaps (book output: "AX C: DD "EH MM AX NN").
        if s["sym"] in ("TT", "DD") and p and n and not phrase_break(i) and not s["first"] \
                and has_sym(n["sym"], "syllabic") and n["st"] != 1 \
                and has_sym(p["sym"], "sonorant") and not has_sym(p["sym"], "nasal") \
                and n["sym"] != "EN":
            s["sym"] = "DX"
        # 8.7 correction: T before syllabic EN is glottalized ("certain").
        if s["sym"] == "TT" and n and n["sym"] == "EN":
            s["sym"] = "TQ"
        # Rule 3: word-final TT after a sonorant before a stressed sonorant -> TQ.
        if s["sym"] == "TT" and s["last"] and p and has_sym(p["sym"], "sonorant") \
                and n and n["first"] and has_sym(n["sym"], "sonorant") \
                and stressed_onset(i + 1) \
                and boundary_after(i) not in (",", ".", ")?", ")C"):
            s["sym"] = "TQ"

    # Rule 5: glottal stop before a word-initial stressed vowel after a
    # syllabic segment (not a determiner), or after a voiced nonplosive
    # across a phrase boundary.
    out = []
    for i, s in enumerate(segs):
        p = prv(i)
        if s["first"] and s["st"] and has_sym(s["sym"], "syllabic") and s["sym"] != "YU" and p:
            pw = pwords[p["word"]].text.lower()
            if (has_sym(p["sym"], "syllabic") and pw not in DETERMINERS) or \
                    (has_sym(p["sym"], "voiced") and not has_sym(p["sym"], "plosive")
                     and boundary_after(i - 1)):
                out.append({"sym": "QQ", "st": 0, "word": s["word"], "first": True,
                            "last": False})
                s = dict(s, first=False)
        out.append(s)
    segs = out

    # Velar fronting before front vowels in the same word (KP/GP of
    # Table 8-1; the book's examples are all word-internal).
    for i, s in enumerate(segs):
        n = nxt(i)
        if s["sym"] in ("KK", "GG") and n and n["sym"] in FRONT and n["word"] == s["word"]:
            s["sym"] = {"KK": "KP", "GG": "GP"}[s["sym"]]

    # Build the PHONO2 string.
    parts = []
    for i, s in enumerate(segs):
        w = pwords[s["word"]]
        if s["first"]:
            parts.append("C:" if w.content else "F:")
        mark = {1: "'", 2: '"'}.get(s["st"], "") if has_sym(s["sym"], "syllabic") else ""
        sym = s["sym"]
        # Affricates are realized as closure + frication (8.7 output: CH SH, JJ ZH).
        sym = {"CH": "CH SH", "JJ": "JJ ZH"}.get(sym, sym)
        parts.append(mark + sym)
        if s["last"] and w.boundary:
            b = w.boundary
            # 8.2.1 #7: release a plosive into a pause with AXP.
            if b in (",", ".", ")?", ")C") and has_sym(s["sym"], "plosive") \
                    and s["sym"] not in ("CH", "JJ"):
                parts.append("AXP")
            parts.append(b)
    return " ".join(parts)
