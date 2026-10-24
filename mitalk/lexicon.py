"""Pronunciation lexicon: data/lexicon.tsv (built by tools/build_lexicon.py
from mitalk.100k.dict and CMUdict)."""

from functools import lru_cache
from pathlib import Path

PATH = Path(__file__).resolve().parent.parent / "data" / "lexicon.tsv"


@lru_cache(maxsize=1)
def _table():
    out = {}
    for line in PATH.read_text(encoding="utf-8").splitlines():
        word, phones, _source = line.split("\t")
        out[word] = phones
    return out


def lookup(word):
    """[(sym, stress)] for a word, or None if unknown."""
    entry = _table().get(word.upper())
    if entry is None:
        return None
    out = []
    for tok in entry.split():
        stress = {"'": 1, '"': 2}.get(tok[0], 0)
        out.append((tok.lstrip("'\""), stress))
    return out
