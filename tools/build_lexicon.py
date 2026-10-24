"""Build data/lexicon.tsv: MITalk-lineage phonemes with CMUdict stress.

    python tools/build_lexicon.py

For each word, the mitalk.100k.dict pronunciation (no stress, Sphinx-style
symbols, some allophones baked in) is aligned against the CMUdict one
(ARPAbet with stress digits).  When they align, the 100k segments are kept
and CMUdict's stress is copied onto the aligned vowels; otherwise the
CMUdict pronunciation is used.  Output is phonemic Klatt notation, before
the allophonic rules of sec. 8.5 (those need sentence context).
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

VOWELS = {"AA", "AE", "AH", "AO", "AW", "AX", "AXR", "AY", "EH", "ER", "EY",
          "IH", "IX", "IY", "OW", "OY", "UH", "UW"}

# 100k-only symbols -> their CMUdict-style base, for alignment.
BASE_100K = {"AX": "AH", "IX": "IH", "AXR": "ER", "TD": "T", "DD": "D",
             "KD": "K", "PD": "P", "BD": "B", "GD": "G"}
FLAP = "DX"  # matches T or D

KLATT = {"B": "BB", "CH": "CH", "D": "DD", "DH": "DH", "F": "FF", "G": "GG",
         "HH": "HH", "JH": "JJ", "K": "KK", "L": "LL", "M": "MM", "N": "NN",
         "NG": "NG", "P": "PP", "R": "RR", "S": "SS", "SH": "SH", "T": "TT",
         "TH": "TH", "V": "VV", "W": "WW", "Y": "YY", "Z": "ZZ", "ZH": "ZH"}


def load_100k(path):
    out = {}
    for line in path.read_text(encoding="ascii", errors="replace").splitlines():
        parts = line.split("\t")
        if len(parts) != 2 or not re.fullmatch(r"[A-Z][A-Z'.\-]*", parts[0]):
            continue  # drops the WARNING / path / three-field junk lines
        phones = parts[1].split()
        phones = [p for p in phones if p not in ("1", "2")]  # stray digits
        expanded = []
        for p in phones:
            expanded += ["T", "S"] if p == "TS" else [p]
        out[parts[0]] = expanded
    return out


def load_cmu(path):
    out = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        word, *phones = line.split("#")[0].split()
        if "(" in word:
            continue  # keep the first variant only
        out[word.upper()] = phones
    return out


def _sub_cost(a, b):
    """a: 100k symbol, b: CMU symbol without stress."""
    base = BASE_100K.get(a, a)
    if base == b or (a == FLAP and b in ("T", "D")):
        return 0.0
    if a == "AXR" and b == "R":
        return 0.3
    if a in VOWELS and b in VOWELS:
        return 0.6
    if (a in VOWELS) != (b in VOWELS):
        return 3.0
    return 1.5


def align(a, b):
    """Needleman-Wunsch; returns (cost, pairs of indices or None)."""
    n, m = len(a), len(b)
    INF = float("inf")
    d = [[INF] * (m + 1) for _ in range(n + 1)]
    bt = [[None] * (m + 1) for _ in range(n + 1)]
    d[0][0] = 0.0
    for i in range(n + 1):
        for j in range(m + 1):
            if i and j:
                c = d[i - 1][j - 1] + _sub_cost(a[i - 1], b[j - 1])
                if c < d[i][j]:
                    d[i][j], bt[i][j] = c, (i - 1, j - 1)
            if i and d[i - 1][j] + 1.0 < d[i][j]:
                d[i][j], bt[i][j] = d[i - 1][j] + 1.0, (i - 1, j)
            if j and d[i][j - 1] + 1.0 < d[i][j]:
                d[i][j], bt[i][j] = d[i][j - 1] + 1.0, (i, j - 1)
    pairs, i, j = [], n, m
    while (i, j) != (0, 0):
        pi, pj = bt[i][j]
        pairs.append((i - 1 if pi == i - 1 else None, j - 1 if pj == j - 1 else None))
        i, j = pi, pj
    return d[n][m], pairs[::-1]


def merge(p100, cmu):
    """Phonemic Klatt (sym, stress) list, or None if the two don't agree."""
    bare = [re.sub(r"\d", "", p) for p in cmu]
    cost, pairs = align(p100, bare)
    if cost > 1.5:
        return None
    # The 100k entry only contributes vowel qualities: any disagreement on a
    # consonant (a substitution, or a consonant only one side has) means the
    # two entries are not the same pronunciation, so use CMUdict instead.
    for i, j in pairs:
        if i is None or j is None:
            if (j is not None and bare[j] not in VOWELS) or \
                    (i is not None and p100[i] not in VOWELS):
                return None
        elif p100[i] not in VOWELS and _sub_cost(p100[i], bare[j]) > 0.3:
            return None
    out = []
    for i, j in pairs:
        if i is None:
            if bare[j] in VOWELS:
                return None  # a CMU syllable the 100k entry lacks
            continue
        a = p100[i]
        stress = int(cmu[j][-1]) if j is not None and cmu[j][-1].isdigit() else 0
        if a in VOWELS and j is None:
            return None
        cmu_sym = bare[j] if j is not None else a
        if a == FLAP:
            a = cmu_sym                        # undo the baked-in flap
        elif a in ("TD", "DD", "KD", "PD", "BD", "GD"):
            a = BASE_100K[a]
        if a == "AXR":
            # CMUdict says whether this is its own "er" syllable (butter,
            # player) or a postvocalic R (bar, beer); r-colored vowels are
            # formed later (sec. 8.2.1).
            a = "ER" if cmu_sym == "ER" else "R"
        if a in VOWELS and stress:
            a = cmu_sym if cmu_sym in VOWELS else a
            if a in ("AX", "IX"):
                a = {"AX": "AH", "IX": "IH"}[a]
        elif a == "AH":
            a = "AX"
        out.append((to_klatt(a), stress if a in VOWELS or a == "ER" else 0))
    return out


VOWELS_K = VOWELS | {"AH"}


def from_cmu(cmu):
    out = []
    for p in cmu:
        sym, stress = re.sub(r"\d", "", p), int(p[-1]) if p[-1].isdigit() else 0
        if stress == 0:
            sym = {"AH": "AX", "IH": "IH", "ER": "ER"}.get(sym, sym)
        out.append((to_klatt(sym), stress))
    return out


def to_klatt(sym):
    return KLATT.get(sym, sym)


def finalize(phones):
    """Join Y+UW into the single phoneme YU (sec. 8.2.1) and format."""
    out = []
    for sym, st in phones:
        if sym == "UW" and out and out[-1][0] == "YY":
            out[-1] = ("YU", st)
        else:
            out.append((sym, st))
    return " ".join(("'" if st == 1 else '"' if st == 2 else "") + s for s, st in out)


def main():
    p100 = load_100k(ROOT / "mitalk.100k.dict")
    cmu = load_cmu(ROOT / "data/cmudict.dict")
    rows, stats = [], {"merged": 0, "cmu_only": 0, "100k_unaligned": 0}
    for word in sorted(set(p100) | set(cmu)):
        merged = merge(p100[word], cmu[word]) if word in p100 and word in cmu else None
        if merged:
            stats["merged"] += 1
            rows.append(f"{word}\t{finalize(merged)}\t100k")
        elif word in cmu:
            stats["cmu_only" if word not in p100 else "100k_unaligned"] += 1
            rows.append(f"{word}\t{finalize(from_cmu(cmu[word]))}\tcmu")
    (ROOT / "data/lexicon.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")
    print(stats, "words:", len(rows))


if __name__ == "__main__":
    main()
