"""FORMAT-style text preprocessing (chapter 2): split sentences, expand
numbers, abbreviations and symbols into words.

RECONSTRUCTION: chapter 2 was not transcribed; this covers the common
cases (cardinals, ordinals, years and decades, money, decimals,
negatives, percent, initials and a short abbreviation list).

Written as a plain character scanner (no regular expressions) so the C
port in c/text.c can follow it line for line.
"""

ONES = ("zero one two three four five six seven eight nine ten eleven twelve "
        "thirteen fourteen fifteen sixteen seventeen eighteen nineteen").split()
TENS = "_ _ twenty thirty forty fifty sixty seventy eighty ninety".split()
SCALES = ((10 ** 12, "trillion"), (10 ** 9, "billion"), (10 ** 6, "million"),
          (1000, "thousand"), (100, "hundred"))
ORDINAL_FIX = {"one": "first", "two": "second", "three": "third", "five": "fifth",
               "eight": "eighth", "nine": "ninth", "twelve": "twelfth"}
SCALE_WORDS = ("million", "billion", "trillion")

# Abbreviations that end in a period without ending the sentence.
TITLES = {"mr": "mister", "mrs": "missus", "ms": "miz", "dr": "doctor",
          "prof": "professor", "mt": "mount", "jr": "junior", "sr": "senior"}
# Abbreviations that may also end a sentence (when a capital follows).
TRAILING = {"etc": "et cetera", "vs": "versus"}
DOTTED = {"e.g.": "for example", "i.e.": "that is", "a.m.": "A M", "p.m.": "P M"}
SYMBOLS = {"&": "and", "%": "percent", "+": "plus", "=": "equals", "@": "at",
           "#": "number"}

# Typographic and accented characters folded to ASCII.
FOLD = {"‘": "'", "’": "'", "“": '"', "”": '"',
        "–": "-", "—": "-"}
for _chars, _base in (("àáâãäå", "a"), ("ç", "c"), ("èéêë", "e"), ("ìíîï", "i"),
                      ("ñ", "n"), ("òóôõö", "o"), ("ùúûü", "u"), ("ýÿ", "y")):
    for _c in _chars:
        FOLD[_c] = _base
        FOLD[_c.upper()] = _base.upper()

MAX_DIGITS = 15  # longer digit strings are read digit by digit


def number_words(n):
    if n < 20:
        return ONES[n]
    if n < 100:
        return TENS[n // 10] + ("" if n % 10 == 0 else " " + ONES[n % 10])
    for value, name in SCALES:
        if n >= value:
            head, rest = divmod(n, value)
            words = number_words(head) + " " + name
            return words + ("" if rest == 0 else " " + number_words(rest))
    return str(n)


def digit_words(digits):
    return " ".join(ONES[int(d)] for d in digits)


def cardinal(digits):
    """Words for a digit string (no commas)."""
    if len(digits) > MAX_DIGITS:
        return digit_words(digits)
    return number_words(int(digits))


def year_words(n):
    hi, lo = divmod(n, 100)
    if lo == 0:
        return number_words(hi) + " hundred"
    return number_words(hi) + " " + ("oh " + ONES[lo] if lo < 10 else number_words(lo))


def is_year(digits):
    if len(digits) != 4:
        return False
    n = int(digits)
    return 1100 <= n <= 1999 or 2010 <= n <= 2099


def ordinal_words(words):
    parts = words.split()
    last = parts[-1]
    if last in ORDINAL_FIX:
        parts[-1] = ORDINAL_FIX[last]
    elif last.endswith("y"):
        parts[-1] = last[:-1] + "ieth"
    else:
        parts[-1] = last + "th"
    return " ".join(parts)


def plural_words(words):
    """'nineteen ninety' -> 'nineteen nineties' (decades)."""
    parts = words.split()
    last = parts[-1]
    if last.endswith("y"):
        parts[-1] = last[:-1] + "ies"
    elif last.endswith(("s", "x")):
        parts[-1] = last + "es"
    else:
        parts[-1] = last + "s"
    return " ".join(parts)


def _scan_number(text, i):
    """Scan digits with optional 3-digit comma groups and a decimal part.
    Returns (int_digits, frac_digits or None, end_index)."""
    j = i
    while j < len(text) and text[j].isdigit():
        j += 1
    whole = text[i:j]
    # comma groups: only when the leading group is 1-3 digits and every
    # group after a comma is exactly three digits
    if len(whole) <= 3:
        while (j + 3 < len(text) + 1 and text[j:j + 1] == "," and
               text[j + 1:j + 4].isdigit() and len(text[j + 1:j + 4]) == 3 and
               not text[j + 4:j + 5].isdigit()):
            whole += text[j + 1:j + 4]
            j += 4
    frac = None
    if text[j:j + 1] == "." and text[j + 1:j + 2].isdigit():
        k = j + 1
        while k < len(text) and text[k].isdigit():
            k += 1
        frac = text[j + 1:k]
        j = k
    return whole, frac, j


def _number_token(text, i):
    """Expand the number starting at text[i] (a digit, '$', '.', or '-').
    Returns (words, end_index) or None if this is not a number."""
    n = len(text)
    money = negative = False
    j = i
    if text[j] == "-":
        negative, j = True, j + 1
    if j < n and text[j] == "$":
        money, j = True, j + 1
    if j < n and text[j] == "." and j + 1 < n and text[j + 1].isdigit():
        whole, frac, j = "", None, j
        k = j + 1
        while k < n and text[k].isdigit():
            k += 1
        frac, j = text[j + 1:k], k
    elif j < n and text[j].isdigit():
        whole, frac, j = _scan_number(text, j)
    else:
        return None
    out = ["minus"] if negative else []

    # suffixes: ordinal (1st, 2ND), decade (1990s, 1990's)
    suffix = text[j:j + 2].lower()
    if not money and frac is None and whole and suffix in ("st", "nd", "rd", "th") \
            and not text[j + 2:j + 3].isalpha():
        out.append(ordinal_words(cardinal(whole)))
        return " ".join(out), j + 2
    dec = 2 if text[j:j + 2] == "'s" else 1 if text[j:j + 1] == "s" else 0
    if not money and frac is None and dec and not text[j + dec:j + dec + 1].isalpha() \
            and (is_year(whole) or (len(whole) == 2 and whole.endswith("0"))):
        words = year_words(int(whole)) if len(whole) == 4 else number_words(int(whole))
        out.append(plural_words(words))
        return " ".join(out), j + dec

    if money:
        # "$1.5 million" -> "one point five million dollars"
        k = j
        while k < n and text[k] == " ":
            k += 1
        scale = next((s for s in SCALE_WORDS if text[k:k + len(s)].lower() == s
                      and not text[k + len(s):k + len(s) + 1].isalpha()), None)
        if scale:
            num = cardinal(whole or "0") + (" point " + digit_words(frac) if frac else "")
            out.append(f"{num} {scale} dollars")
            return " ".join(out), k + len(scale)
        dollars = int(whole) if whole else 0
        cents = int((frac or "")[:2].ljust(2, "0")) if frac else 0
        parts = []
        if dollars or not cents:
            parts.append(cardinal(whole or "0") + (" dollar" if dollars == 1 else " dollars"))
        if cents:
            parts.append(number_words(cents) + (" cent" if cents == 1 else " cents"))
        out.append(" and ".join(parts))
        return " ".join(out), j

    if frac is not None:
        out.append((cardinal(whole) if whole else "") + " point " + digit_words(frac))
        return " ".join(w for w in " ".join(out).split()), j
    out.append(year_words(int(whole)) if is_year(whole) else cardinal(whole))
    return " ".join(out), j


def tokenize(text):
    """Words (with expansions already split into words) and . , ? ! tokens."""
    text = "".join(FOLD.get(c, c) for c in text)
    toks = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        prev = text[i - 1] if i else " "
        if c.isdigit() or (c in "$" and i + 1 < n and (text[i + 1].isdigit() or
                                                       text[i + 1] == "." and i + 2 < n and text[i + 2].isdigit())) \
                or (c == "." and i + 1 < n and text[i + 1].isdigit() and not prev.isdigit()) \
                or (c == "-" and not prev.isalnum() and i + 1 < n and
                    (text[i + 1].isdigit() or text[i + 1] == "$")):
            res = _number_token(text, i)
            if res:
                words, i = res
                toks += words.split()
                continue
        if c == "'" and text[i + 1:i + 3].isdigit() and text[i + 3:i + 4] == "s":
            toks += plural_words(number_words(int(text[i + 1:i + 3]))).split()  # '80s
            i += 4
            continue
        if c.isascii() and c.isalpha():
            j = i
            while j < n and text[j].isascii() and text[j].isalpha():
                j += 1
            # dotted abbreviations: e.g. i.e. a.m. U.S.
            if j - i == 1 and text[j:j + 1] == "." and text[j + 1:j + 2].isalpha() \
                    and text[j + 2:j + 3] == ".":
                k = j
                letters = [text[i]]
                while text[k:k + 1] == "." and text[k + 1:k + 2].isalpha() \
                        and not text[k + 2:k + 3].isalpha():
                    letters.append(text[k + 1])
                    k += 2
                if text[k:k + 1] == ".":
                    k += 1
                key = ".".join(letters).lower() + "."
                toks += (DOTTED.get(key) or " ".join(l.upper() for l in letters)).split()
                i = k
                continue
            while text[j:j + 1] == "'" and text[j + 1:j + 2].isascii() and text[j + 1:j + 2].isalpha():
                j += 1
                while j < n and text[j].isascii() and text[j].isalpha():
                    j += 1
            toks.append(text[i:j])
            i = j
            continue
        if c in SYMBOLS:
            toks.append(SYMBOLS[c])
        elif c in ".!?":
            toks.append(c)
        elif c in ",;:":
            toks.append(",")
        i += 1
    return toks


def _capitalized(tok):
    return tok is not None and tok[:1].isupper()


def normalize(text):
    """Return a list of sentences, each a list of word/punctuation tokens."""
    toks = tokenize(text)
    sentences, cur = [], []
    for i, tok in enumerate(toks):
        nxt = toks[i + 1] if i + 1 < len(toks) else None
        if tok in (".", "!", "?"):
            last = cur[-1] if cur else None
            low = last.lower() if last else ""
            if tok == ".":
                if low in TITLES or low == "st":
                    cur[-1] = ("saint" if _capitalized(nxt) else "street") if low == "st" \
                        else TITLES[low]
                    if nxt is not None:
                        continue
                elif low == "no" and nxt and nxt in ONES + TENS + ["minus"]:
                    cur[-1] = "number"
                    continue
                elif low in TRAILING:
                    cur[-1] = TRAILING[low]
                    if nxt is not None and not _capitalized(nxt):
                        continue
                elif last and len(last) == 1 and last.isupper() and last != "I" \
                        and _capitalized(nxt):
                    continue  # an initial, as in "J. Allen"
            if not cur or cur[-1] in (".", "?", ","):
                if cur and tok == "?":
                    cur[-1] = "?"
                continue  # "...", "?!" and the like
            cur.append("?" if tok == "?" else ".")
            sentences.append(cur)
            cur = []
        elif tok == ",":
            if cur and cur[-1] != ",":
                cur.append(",")
        else:
            cur.append(tok)
    if cur:
        if cur[-1] == ",":
            cur.pop()
        if cur:
            cur.append(".")
            sentences.append(cur)
    return [[w for tok in s for w in tok.split()] for s in sentences]
