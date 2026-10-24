"""PARSER: parts of speech and phrase structure (chapter 4, Appendix A).

The book's ATN grammars run over a 12,000-morph lexicon with parts of
speech that is not printed.  Here closed-class words come from a word
list, and every other word gets the part-of-speech processor's default
set (NOUN, VERB, ADJ) refined by its suffix dispatch (sec. 4.5).  Phrases
are then found with the book's global strategy (sec. 4.6.1): at each word
take the longest noun group, else the longest verb group, else skip.

The output is what the prosodic modules need: content vs. function
words, Table 10-1 peak levels, and )N / )C boundary markers.
"""

from dataclasses import dataclass, field

CLOSED = {
    "ART": "a an the",
    "DEM": "this that these those",
    "PRN": ("i me my mine myself you your yours yourself yourselves he him his himself "
            "she her hers herself it its itself we us our ours ourselves they them "
            "their theirs themselves who whom whose what which anyone anybody anything "
            "everyone everybody everything someone somebody something nobody nothing none"),
    "DETQ": "every any each no some either neither",
    "QUANT": "many much few several more most less least all both half",
    "PREP": ("of in on at by for with from to into onto upon about above across after "
             "against along among around before behind below beneath beside between "
             "beyond during except inside near off out outside over past since through "
             "throughout toward towards under underneath until unto up within without via per"),
    "CONJ": "and or but nor yet so",
    "SCONJ": ("because although though whereas whether while unless if when whenever "
              "where wherever"),
    # comparatives / "once": function words that do not start a clause
    "PREPLIKE": "than as once",
    "MOD": "can could may might must shall should will would ought",
    "BE": "be am is are was were been being",
    "HAVE": "have has had having",
    "DO": "do does did",
    "TO": "",
    "NOT": "not never",
}
POS = {}
for pos, words in CLOSED.items():
    for w in words.split():
        POS.setdefault(w, set()).add(pos)
POS["to"].add("TO")
for w in CLOSED["PREPLIKE"].split():
    POS[w] = {"PREP"}

# Frequent verbs, so a noun group does not swallow the main verb ("the old
# man sat").  The book's lexicon knows these; with no POS lexicon we list
# the common ones.  Words also common as nouns keep both.  RECONSTRUCTION.
VERBS = ("sat sit sits said say says went go goes got get gets made make makes took take "
         "takes came come comes saw see sees knew know knows thought think thinks told "
         "tell tells found find finds gave give gives left leave leaves felt feel feels "
         "kept keep keeps began begin begins ran brought bring brings bought buy buys "
         "stood stand stands held hold holds wrote write writes spoke speak speaks heard "
         "hear hears meant mean means met meet meets paid pay pays sent send sends built "
         "build builds lost lose loses fell grew grow grows drew draw draws flew wore "
         "wear wears broke break breaks chose choose chooses drove drive drives ate eat "
         "eats became become becomes seemed seem seems looked wanted want wants used "
         "worked called tried try tries asked ask asks needed liked like likes lived "
         "believed believe believes happened happen happens produced produce produces "
         "require requires required let lets")
BOTH = set("work works use uses call calls need needs look looks run runs".split())
for w in VERBS.split():
    POS[w] = {"VERB"}
for w in BOTH:
    POS[w] = {"VERB", "NOUN"}

CONTRACTIONS = {"n't": "NOT", "'s": "BE", "'re": "BE", "'m": "BE", "'ll": "MOD",
                "'d": "MOD", "'ve": "HAVE"}
# contractions whose base is not a word on its own
CONTRACTION_BASE = {"can't": {"MOD", "NOT"}, "won't": {"MOD", "NOT"},
                    "shan't": {"MOD", "NOT"}, "ain't": {"BE", "NOT"},
                    "let's": {"VERB"}}
WH_WORDS = set("who whom whose what which when where why how".split())

# Table 10-1 peak levels.  Content words are level 6 and above.
LEVEL = {"ART": 0, "CONJ": 1, "SCONJ": 1, "PREP": 2, "BE": 2, "HAVE": 2, "DO": 2,
         "TO": 2, "PRN": 3, "VERB": 6, "DEM": 6, "NOUN": 7, "ADJ": 7, "ADV": 7,
         "MOD": 9, "QUANT": 10, "DETQ": 2, "NOT": 12}
PERSONAL = set("i me my you your he him his she her it its we us our they them their".split())


@dataclass
class PWord:
    text: str
    pos: set
    tag: str = ""            # the single POS chosen in a phrase
    content: bool = False
    level: int = 0
    boundary: str = ""       # )N, )C, ",", ".", ")?"


def word_pos(word):
    """Part-of-speech set for one word (closed list, else sec. 4.5 rules)."""
    w = word.lower()
    if w in POS:
        return set(POS[w])
    if w in CONTRACTION_BASE:
        return set(CONTRACTION_BASE[w])
    for ending, pos in CONTRACTIONS.items():
        if w.endswith(ending) and len(w) > len(ending):
            base = w[:-len(ending)]
            if base in POS:
                # "didn't" -> DO+NOT, "it's" -> PRN+BE, "should've" -> MOD+HAVE
                return set(POS[base]) | {pos}
            # 's on a noun: possessive (or contraction) noun, sec. 4.5
            return {"NOUN"} if ending == "'s" else {"NOUN", pos}
    if w.endswith("ly") and len(w) > 4:
        return {"ADV", "ADJ"}
    if w.endswith("ing") and len(w) > 4:
        return {"VERBING", "NOUN", "ADJ"}
    if w.endswith("ed") and len(w) > 3:
        return {"VERBEN", "VERB", "ADJ"}
    if w.endswith(("ness", "ment", "tion", "sion", "ity", "ism", "ist", "ance", "ence")):
        return {"NOUN"}
    if w.endswith(("ous", "ful", "ive", "able", "ible", "al", "ic", "less")):
        return {"ADJ", "NOUN"}
    if w.endswith("s") and not w.endswith("ss") and len(w) > 3:
        return {"NOUN", "VERB"}
    return {"NOUN", "VERB", "ADJ"}  # the default routine


NG_HEAD = {"NOUN"}


def _noun_group(words, i):
    """Length of the longest noun group at i (0 if none)."""
    j = i
    if j < len(words) and "PREP" in words[j].pos:
        j += 1
    k = j
    if k < len(words) and words[k].pos & {"PRN"} and not words[k].pos & {"ART", "DEM"}:
        return k + 1 - i
    if k < len(words) and "QUANT" in words[k].pos:
        k += 1
    if k < len(words) and words[k].pos & {"ART", "DEM", "DETQ", "PRN"}:
        k += 1
    best = 0
    while k < len(words) and words[k].pos & {"ADV", "ADJ", "NOUN", "VERBING", "VERBEN"} \
            and not words[k].pos & {"PREP", "CONJ", "SCONJ", "BE", "HAVE", "MOD", "DO", "NOT"}:
        # Subject-verb agreement: after a plural noun, an ambiguous word
        # without -s is taken as the verb ("messages work").  RECONSTRUCTION.
        if best and "VERB" in words[k].pos and words[best - 1].text.lower().endswith("s") \
                and not words[k].text.lower().endswith("s"):
            words[k].pos = {"VERB"}   # so the verb group takes it next
            break
        k += 1
        if words[k - 1].pos & NG_HEAD:
            best = k
    if best:
        return best - i
    # a bare determiner + pronoun-like word ("this", "those")
    if k > j and words[j].pos & {"DEM", "QUANT"}:
        return k - i
    return 0


def _verb_group(words, i):
    """Length of the longest verb group (or verbal) at i."""
    k = i
    aux = {"MOD", "BE", "HAVE", "DO", "NOT", "TO"}
    while k < len(words) and (words[k].pos & aux or (words[k].pos == {"ADV", "ADJ"} and k > i)):
        k += 1
    if k < len(words) and words[k].pos & {"VERB", "VERBING", "VERBEN"} \
            and not words[k].pos & {"ART", "PREP", "PRN"}:
        k += 1
    return k - i if k > i else 0


def parse(tokens):
    """Tag a tokenized sentence and mark phrase boundaries."""
    words = []
    for tok in tokens:
        if tok in (",", ".", "?"):
            if words:
                words[-1].boundary = ")?" if tok == "?" else tok
            continue
        words.append(PWord(tok, word_pos(tok)))

    ngroups = []          # (first, last) word index of each noun group
    i = 0
    while i < len(words):
        n = _noun_group(words, i)
        if n:
            for w in words[i:i + n]:
                w.tag = _pick(w, noun_group=True)
            if not words[i + n - 1].boundary:
                words[i + n - 1].boundary = ")N"
            ngroups.append((i, i + n - 1))
            i += n
            continue
        n = _verb_group(words, i)
        if n:
            for w in words[i:i + n]:
                w.tag = _pick(w, noun_group=False)
            i += n
            continue
        words[i].tag = _pick(words[i], noun_group=False)
        i += 1

    for k, w in enumerate(words):
        w.level = LEVEL.get(w.tag, 7)
        if w.tag == "PRN" and w.text.lower().split("'")[0] not in PERSONAL:
            w.level = 6   # a function word with secondary stress (8.2.2)
        w.content = w.level >= 6 and w.tag not in ("DEM", "MOD", "QUANT", "DETQ", "PRN")
        # clause boundary before a subordinating conjunction
        if k and w.tag == "SCONJ" and not words[k - 1].boundary in (",", ".", ")?"):
            words[k - 1].boundary = ")C"

    # Sec. 8.2.4: erase a )N after a noun phrase with only one
    # primary-stressed content word (so NP + VP is one phrase).
    for first, last in ngroups:
        if words[last].boundary == ")N" and \
                sum(x.content for x in words[first:last + 1]) <= 1:
            words[last].boundary = ""
    if words and not words[-1].boundary:
        words[-1].boundary = "."
    # Wh-questions take a falling tune (10.5: steeper fall than statements),
    # not the yes/no rise.  RECONSTRUCTION: approximated by the statement tune.
    if words and words[-1].boundary == ")?" and words[0].text.lower() in WH_WORDS:
        words[-1].boundary = "."
    return words


def _pick(w, noun_group):
    order = (("PRN", "ART", "DEM", "DETQ", "QUANT", "PREP", "NOUN", "ADJ", "ADV",
              "VERBING", "VERBEN", "VERB") if noun_group else
             ("MOD", "BE", "HAVE", "DO", "NOT", "TO", "PREP", "CONJ", "SCONJ", "ART",
              "DEM", "PRN", "VERB", "VERBING", "VERBEN", "ADV", "ADJ", "NOUN"))
    for p in order:
        if p in w.pos:
            return {"VERBING": "VERB", "VERBEN": "VERB"}.get(p, p)
    return "NOUN"
