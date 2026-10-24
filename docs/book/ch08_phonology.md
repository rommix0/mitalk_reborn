# Chapter 8 — The phonological component (implementation notes)

Source: Allen, Hunnicutt & Klatt, *From Text to Speech: The MITalk System* (1987), ch. 8.

**Format note.** This is a structured implementation spec written in our own words from the scans.
It is **not** a verbatim transcription of the book's prose, and the long sample output listing of
section 8.7 is catalogued rather than copied out. Functional data (symbol inventory, the six
segmental rules, pause durations, the short Figure 8-1 example) is recorded exactly.

**Page numbering.** Markers give PRINTED page numbers. `book_pNNN.png` shows printed page NNN+1;
printed p.81 comes from `book_fix/printed_p081.png`. Printed p.93 (`book_p092.png`) is the start of
ch. 9 and is skipped.

---

<!-- book p.81 (book_fix/printed_p081.png) -->

## 8.1 Overview

- **PHONOL** takes the text-analysis output (chs. 2–6) and feeds **PROSOD** (ch. 9). It has two
  modules:
  - **PHONO1**: uses PARSER information to place the syntactic markers that affect the spoken output.
  - **PHONO2**: segmental recoding rules that choose an allophone for each phoneme and simplify some
    unstressed phonetic sequences.
- Pause rules live in both PHONO1 and PHONO2. Pauses of different lengths go at sentence
  boundaries, clause boundaries, and certain punctuation (e.g. commas). Extra pauses are added in
  longer phrases and at slow speaking rate so the talker does not seem to have unlimited breath.

### 8.1.1 Synthesis-by-rule

To use MITalk as synthesis-by-rule without the analysis modules, start at PHONO2 or PROSOD. Input is
then an abstract representation: phonemes, lexical stress symbols, and syntactic structure symbols
per sentence. Suitable when the abstract syntactic/phonemic form is known or computable. Quality is
better than full TTS (no analysis errors) but users need linguistic skill. Storage is about 100 bits
per sentence.

## 8.2 Input representation for a sentence

PHONO1 input: per word, a phonemic pronunciation (as spoken in isolation), its lexical stress
pattern, and syntactic information (part of speech, phrase structure). PHONO1 output: a single
symbol string per sentence. The PHONO1/PHONO2 symbol inventory is Table 8-1, which

<!-- book p.82 (book_p081.png) -->

has **55 phonetic segments, 3 stress markers, 4 boundary
types, 5 syntactic structure indicators**. The authors expect the syntactic/semantic inventory to
grow; this set is what they judged sufficient for a first-order acceptable prosody.

### Figure 8-1: Example of PHONO1 and PHONO2 processing

Input sentence: *The old man sat in a rocker.* Pipeline output, exactly as printed (a short
canonical test case):

```
The old man sat in a rocker.
 SOUND1:  DH 'AH
 SOUND1:  'OW LL DD
 SOUND1:  MM 'AE NN
 SOUND1:  SS 'AE TT
 SOUND1:  'IH NN
 SOUND1:  AX
 SOUND1:  RR 'AA KK * - ER
 SOUND1:  .
 SOUND1: <EOF>
  PHONO1: Function word: DH AH
  PHONO1: Content word:  'OW LL DD
  PHONO1: Content word:  MM 'AE NN [End NOUN phrase]
  PHONO1: Content word:  SS 'AE TT
  PHONO1: Function word: IH NN
  PHONO1: Function word: AX
  PHONO1: Content word:  RR 'AA KK * - ER
  PHONO1: Punctuation:  .
  PHONO1: <EOF>
   PHONO2: Function word: DH IY
   PHONO2: Content word:  'OW LX DD
   PHONO2: Content word:  MM 'AE NN [End NOUN phrase]
   PHONO2: Content word:  SS 'AE DX
   PHONO2: Function word: IH NN
   PHONO2: Function word: AX
   PHONO2: Content word:  RR 'AA KK * - ER
   PHONO2: Punctuation:  .
   PHONO2: <EOF>
```

What changes between stages (useful as assertions):
- SOUND1 → PHONO1: words classified Function/Content; stress removed from function words
  (`DH 'AH` → `DH AH`, `'IH NN` → `IH NN`); `[End NOUN phrase]` attached after *man*.
- PHONO1 → PHONO2: `DH AH` → `DH IY` (rule 6), `LL` → `LX` in *old* (rule 1), `TT` → `DX` in *sat*
  (rule 2, flapping across the word boundary before unstressed *in*).

### 8.2.1 Phonemic inventory — departures from a traditional analysis

1. Diphthongs `AY`, `AW`, `OW`, `YU` are single phonemes (not `AA+YY`, `AA+IY`, `AA+IH` etc.),
   because two-phoneme versions do not give simpler duration or formant-trajectory rules.
2. Affricates `CH`, `JJ` are single phonemes (not `TT+SH`), same reason.
3. Vowel+`RR` nuclei are internal special vowels: `IXR` ("beer"), `EXR` ("bear"), `AXR` ("bar"),
   `OXR` ("boar"), `UXR` ("pure").

<!-- book p.83 (book_p082.png) -->

4. "player", "buyer" are two syllables: `EY+ER`, `AY+ER`.
5. Syllabic consonants: "butter" `BB 'AH TT ER` (phonetically `BB AH DX ER`), "button"
   `BB 'AH TT EN`, "bottle" `BB 'AA TT EL`, "pop'em" `PP 'AA PP EM`.
6. Flap `DX`, glottalized t `TQ`, velarized l `LX` are allophones, not phonemes; rules insert them.
7. Pseudo-vowel `AXP` is inserted between a plosive and a following pause so the plosive gets
   released.

### 8.2.2 Lexical stress

In PHONO1 input every stressed vowel is preceded by `'` (primary) or `"` (secondary).
- `'` primary: only vowels of open-class content words; at most one primary per word.
- `"` secondary: used in some content words (first syllable of "demonstration"), compounds (second
  syllable of "baseball"), the strongest syllable of polysyllabic function words ("until"), and
  pronouns other than personal pronouns (not e.g. "his").

### 8.2.3 Stress reduction in function words

Content words (nouns, adjectives, adverbs, main verbs) should arrive at PHONO1 with one primary
stress. PHONO1 reduces stress in many, not all, closed-class function words so they get no
primary-stress pitch gesture — e.g. determiners, conjunctions, auxiliaries, personal pronouns.

Every word must be immediately preceded by a word-boundary symbol: `C:` for content words (open
class: nouns, verbs, adjectives, adverbs), `F:` for all others. Later modules use this to pick
plausible pause sites in long phrases (between a content word and a function word).

### 8.2.4 Syntactic structure

Syntactic symbols strongly affect sentence stress, rhythm, and intonation. They are placed
immediately **before** the word-boundary symbol. At most one syntactic marker per position; when
several apply, use the strongest.

An utterance must end with `.` (final fall) or `)?` (yes–no question intonation).

<!-- book p.84 (book_p083.png) -->

### Table 8-1: Klatt symbols used in the synthesis modules

**Vowels**

| Sym | Example | Sym | Example | Sym | Example | Sym | Example | Sym | Example |
|---|---|---|---|---|---|---|---|---|---|
| AA | Bob | AE | bat | AH | but | AO | bought | AW | bout |
| AX | about | AXR | bar | AY | bite | EH | bet | ER | bird |
| EXR | bear | EY | bait | IH | bit | IX | impunity | IXR | beer |
| IY | beet | OW | boat | OXR | boar | OY | boy | UH | book |
| UW | boot | UXR | poor | YU | beauty | | | | |

**Sonorant consonants**

| Sym | Example | Sym | Example | Sym | Example | Sym | Example | Sym | Example |
|---|---|---|---|---|---|---|---|---|---|
| EL | bottle | HH | hat | HX | the hurrah | LL | let | LX | bill |
| RR | rent | RX | fire | WW | wet | WH | which | YY | yet |

**Nasals**

| Sym | Example | Sym | Example | Sym | Example | Sym | Example | Sym | Example |
|---|---|---|---|---|---|---|---|---|---|
| EM | keep'em | EN | button | MM | met | NN | net | NG | sing |

**Fricatives**

| Sym | Example | Sym | Example | Sym | Example | Sym | Example | Sym | Example |
|---|---|---|---|---|---|---|---|---|---|
| DH | that | FF | fin | SS | sat | SH | shin | TH | thin |
| VV | vat | ZZ | zoo | ZH | azure | | | | |

**Plosives**

| Sym | Example | Sym | Example | Sym | Example | Sym | Example | Sym | Example |
|---|---|---|---|---|---|---|---|---|---|
| BB | bet | DD | debt | DX | butter | GG | gore | GP | give |
| KK | core | KP | keen | PP | pet | TT | ten | TQ | at Alan |

**Affricates**: `CH` chin, `JJ` gin

**Pseudo-vowel**: `AXP` plosive release

(Count check: 23 vowels + 10 sonorants + 5 nasals + 8 fricatives + 10 plosives + 2 affricates
+ 1 pseudo-vowel = 59, not the 55 stated on p.82 [sic?]. The book does not say which are excluded;
possibly the allophones `DX`, `TQ`, `LX` and `AXP`, which would give 55.)

**Stress symbols**

| Sym | Meaning |
|---|---|
| `'` or `1` | primary lexical stress |
| `"` or `2` | secondary lexical stress |

**Word and morpheme boundaries**

| Sym | Meaning |
|---|---|
| `-` | syllable boundary (ignored) |
| `*` | morpheme boundary |
| `C:` | begin content word |
| `F:` | begin function word |

**Syntactic structure**

| Sym | Meaning |
|---|---|
| `.` | end of declarative utterance |
| `)?` | end of yes/no question |
| `,` | orthographic comma |
| `)N` | end of noun phrase |
| `)P` | potential breath pause |
| `)C` | end of clause |

(Six syntactic symbols are listed against "five syntactic structure indicators" on p.82 [sic?].)

<!-- book p.85 (book_p084.png) -->

### 8.2.4 (cont.) Placement rules

- Conjoined clauses: put a syntactic symbol immediately before the conjunction.
- Wherever a comma could appear in the spelled form of the utterance, insert the syntactic comma
  `,`. Syntactic commas are treated as **full clause boundaries** by the rules (used for lists and
  for chunking long units).
- `)N` marks end of noun phrase. Segments in the syllable before any syntactic boundary are
  lengthened.
- Exception (after Carlson et al. 1979): delete any `)N` that follows a noun phrase containing only
  **one** primary-stressed content word. The NP + VP is then one phonological phrase: no internal
  phrase-final lengthening and no fall-rise F0 between NP and VP.

## 8.3 Comparison between ideal synthesis input and system performance

Section 8.7 gives MITalk analysis output on a sample text; places where analysis made an "error"
(i.e. differs from what the synthesis routines want, not necessarily a linguistic error) are
underlined, and perceptually harmful errors get a footnote. The sample has over 200 words and over
1000 phonetic segments.

### 8.3.1 Phonetic transcription "errors"
25 errors, all minor, mostly `IH`-vs-schwa. Letter-to-sound rules fire only about 5% of the time
and cause few problems. Phonetic error rate ≈ 1% of segments (about one word in twenty).

### 8.3.2 Stress "errors"
12 lexical-stress errors. Words like "might" and "each" should carry primary stress in the lexicon
(they usually attract some focus) but do not. Some words (e.g. "prerecorded") are mishandled by
the morphological stress reassignment rules.

<!-- book p.86 (book_p085.png) -->

### 8.3.3 Morpheme boundary problems
`*` keeps medial `KK` in back*ache from being strongly aspirated. But in applic*ation the `*`
blocks the desired resyllabification, so the medial `KK` is not aspirated. Suggestion: delete `*`
between a root and a bound suffix (not between two roots). Likewise automatic*al*ly comes out with
six syllables instead of the preferred `AO DX - AX - MM 'AE DX - IH - KK LL IY`.

### 8.3.4 Syntactic "errors"
Many phrase/clause-boundary misplacements: 7 missing `)N`, 1 missing `)C`, 17 spurious `)C`
inserted between words. The spurious ones come from a deliberate policy of adding extra clause
boundaries to break long stretches with pauses and intonation breaks ("pausing for breath"); the
trade-off against breaking at syntactically bad points is not yet tuned.

### 8.3.5 Summary
Most phonetic, stress, and phonological-rule errors are easy to fix; few syntax errors are.
Automatic syntactic analysis is the main weakness; intelligibility results (ch. 13) are still good.

## 8.4 Stress rules (segmental stress feature)

Each output segment gets feature **Stress ∈ {0,1}**, default 0.
- A vowel preceded by a stress symbol (`'`, `"`, or `!`) in the input → Stress = 1.
  (Note: `!` appears here but is not in Table 8-1.)
- A consonant before a stressed vowel → Stress = 1 **iff** it is in the same morpheme **and** the
  consonants form an acceptable word-initial cluster.

<!-- book p.87 (book_p086.png) -->

This defines syllable affiliation: stressed consonants belong to the following vowel; unstressed
consonants belong to the preceding vowel (or affiliation is irrelevant later). Segmental stress
feeds rules for: flapping of `TT`/`DD`, consonant and vowel lengthening, strong aspiration of
voiceless plosives, and degree of formant-target undershoot.

Worked example: "Atlantic" vs "atrocious". In "atrocious" both `TT` and `RR` get Stress = 1 ("tr" is
a legal word-initial cluster) → strongly aspirated full alveolar `TT`. In "Atlantic" only `LL` gets
Stress = 1 ("tl" is not legal) → `TT` becomes a glottal(ized) stop. With correct flapping and
glottalized-t rules this choice of stressed consonants selects the right `TT` allophone.

## 8.5 Rules of segmental phonology

Few segmental rules live here; graded processes such as palatalization (e.g. partially palatalized
`SS` in "fish soup") are done in the phonetic component (ch. 11). These rules mainly help listeners
locate word and phrase boundaries (e.g. rule 3 prevents a word-final `TT` being heard as the start
of the next word, by glottalizing to suppress the release burst).

1. **Velarized L.** `LL` → `LX` when `LL` is preceded by a vowel and followed by anything other than
   a stressed vowel of the same word.
2. **Flapping.** `TT` or `DD` → `DX`, within words and across word boundaries but **not** across
   phrase or clause boundaries, when the plosive is followed by a **non-primary-stressed** vowel and
   preceded by a **nonnasal sonorant**. Examples: "butter", "ladder", "sat about".
3. **Glottalized t.** A word-final `TT` preceded by a sonorant → `TQ` (glottal rather than t-burst
   release) when the next word starts with a stressed sonorant — unless a clause boundary
   intervenes, in which case the `TT` is released into the pause.

<!-- book p.88 (book_p087.png) -->

   Examples: "that one", "Mat ran".
4. **Unreleased plosive.** A voiceless plosive is not released when the next phonetic segment is
   another voiceless plosive in the same clause.
5. **Glottal stop insertion.** Insert a glottal stop before a word-initial stressed vowel if the
   preceding segment is syllabic (and not a determiner), or if the preceding segment is a voiced
   nonplosive and a phrase boundary intervenes. Example: "Liz eats".
6. **"the".** "the" → `DH IY` when the next word starts with a vowel.

### 8.5.1 An example

Applying the six rules to Figure 8-1 makes three changes: rule 6 turns the schwa of "the" into `IY`;
rule 1 makes `LL` in "old" postvocalic `LX`; rule 2 flaps the `TT` of "sat" to `DX`. The lower part of
Figure 8-1 is the resulting broad phonetic transcription, which is the input to PROSOD (ch. 9).

## 8.6 Pauses

Pauses and prepausal lengthening cue syntactic structure (Klatt 1976b). Pause durations:

| Condition | Pause |
|---|---|
| After any sentence of more than five words | 800 ms (breath-length) |
| End of paragraph | 1200 ms |
| Sentence-internal: at punctuation in the text, or at clause boundaries detected and inserted by PHONO1 | 400 ms |

Very long sentences also need breath pauses at internal positions (limited lung volume). An algorithm
chooses them from the number of syllables on each side of the candidate site

<!-- book p.89 (book_p088.png) -->

and the strengths of nearby boundaries. If needed, such a pause may go between a content word and a
function word even where syntax analysis found no phrase boundary. (No further detail of this
algorithm is given on these pages.)

## 8.7 Evaluation of the analysis modules

**Input.** A recorded-demonstration passage (about speech synthesis by rule and text-to-speech
conversion; four application categories — single-word response, limited fixed-syntax messages,
large-vocabulary general syntax, unrestricted English text — and the information a synthesizer
needs: phonemes, word stress, phrase/clause boundaries, focus words; it closes by saying the speech
was produced in June 1979). The exact text is on printed p.89 of the scan; it is not reproduced here.

**Output.** The PHONO2 output for that passage is printed on pp.89–92 as a continuous stream of
tokens: `C:`/`F:` word markers, Klatt segments, stress marks `'` `"`, morpheme `*`, syntactic markers
`)N` `)C` `,` `.`, and `AXP`. Wrong segments are underlined with the correct segment as a subscript;
a null subscript ∅ means delete. In the requested notation that is `{WRONG->RIGHT}` / `{X->0}`.

The full listing is not copied here. Below is a catalogue of the correction types that occur in it,
with representative short contexts, so an implementation knows what the "desired" output differs in.

### Correction types observed (pp.89–92)

| Type | Notation | Notes |
|---|---|---|
| Unstressed `IH` should be schwa | `{IH->AX}` | Most frequent error; e.g. in the `IH KK` of "-ic"/"-ical" words: `... {IH->AX} KK ...` |
| Unstressed `IH` should be `IX` | `{IH->IX}` | e.g. before `ZZ`, `SH`, `DD` in suffixes |
| Schwa should be `UW` | `{AX->UW}` | "to": `F: TT {AX->UW} C:` (recurring) |
| Vowel should be schwa | `{OW->AX}`, `{"OW->AX}` | reduced first syllable |
| Morpheme boundary should go | `{*->0}` | footnote: root/bound-morph boundary harms output |
| Release vowel should go | `{AXP->0}` | before a deleted `)C` |
| Spurious clause boundary | `{)C->0}` | footnote: too many extra `)C` pauses |
| Clause boundary should be NP boundary | `{)C->)N}` | |
| Missing/extra comma | `{,->0}` | footnote: extra comma at sentence end removes terminal fall |
| Stress level wrong | `{'OW->"OW}`, `{'OXR->"OXR}`, `{IY->'IY}`, `{"IY->'IY}`, `{AY->'AY}`, `{'EH->"EH}` (?) | footnotes: two primary stresses in one word; "each" and "might" should be intrinsically stressed. The exact stress marks inside the subscripts are hard to read at this scan resolution [?] |
| Two segments → r-colored nucleus | `{"AO->"OXR} {RR->0}`, `{"EH->"EXR} {RR->0}`, `{"AE->"EXR} {RR->0}` | vowel+`RR` should be one nucleus |
| Flap should be other `TT` allophone | `{TT->DX}`, `{DX->TQ}` | |
| Syllabic nasal | `{EN->AX NN}` | |
| Syllabic L | `{AX->0} {LX->EL}` (see uncertainty list) | |
| Extra syllable from suffix expansion | footnote: DECOMP yields poor suffix expansion (`... {AX->0} {LX->0} {*->0} LL IY ...`) | |
| Number sequences | footnote: hard to get stress right in number sequences | |

Footnote texts (paraphrased) and where they attach:
- p.89 ¹ too many extra `)C` pauses added; ² morph boundary between root and bound morph is harmful.
- p.90 ¹ two primary stresses in one word; ² extra comma at sentence end → no terminal fall.
- p.91 ¹ "each" should be intrinsically stressed; ² "might" should be intrinsically stressed;
  ³ DECOMP yields poor suffix expansion.
- p.92 ¹ stress in number sequences is hard.

### Short excerpts (illustrating the format)

First line of the output (p.89), in the requested notation:

```
F: DH "IH SS C: RR {IH->AX} KK 'OXR DD * IH NG {)C->)N} F: IH ZZ F:
```

Last line of the output (p.92):

```
DD {IH->AX} VV 'EH LX AX PP * TT F: AE TQ C: 'EH MM C: 'AY C: TT 'IY .
```
