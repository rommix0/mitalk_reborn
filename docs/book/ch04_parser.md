# Chapter 4 — The phrase-level parser (implementation notes)

Source: Allen, Hunnicutt & Klatt, *From Text to Speech: The MITalk System* (1987), ch. 4.

**Format note.** This file is a structured implementation spec written in our own words from the
scanned pages. It is **not** a verbatim transcription of the book's prose. Functional data
(part-of-speech codes, feature names, ATN arc tables, state-diagram edges, short test phrases) is
recorded exactly as printed. Consult the scans for the original wording.

**Page numbering.** Markers give PRINTED page numbers. `book_pNNN.png` shows printed page NNN+1;
printed p.40 comes from `book_fix/printed_p040.png`. Printed p.52 (`book_p051.png`) is the start of
ch. 5 and is skipped.

---

<!-- book p.40 (book_fix/printed_p040.png) -->

## 4.1 Overview

Design constraints and rationale for the parser (paraphrased):

- It must handle arbitrary text quickly; it does **not** need to derive semantics. It produces a
  **surface-structure** parse whose purpose is to feed the prosodic algorithms. Some clause
  boundaries are additionally set by rules in ch. 8. Phrase- and clause-level structure supplies
  most of the syntax the prosodic rules need.
- A failed parse is unacceptable in TTS (whereas arbitrary choice among ambiguous parses is fine).
  Observation motivating the design: across competing full-sentence parses, the bottom-level
  phrase nodes tend to be the same; ambiguity comes from grouping phrases at clause level
  (especially in binary-tree parsers), and failed parses usually still have correct phrase
  structure. So the parser outputs as many **phrase nodes** as it can, for the prosodic component.
- It runs in real time with few resources. There is no recursion in the grammar or
  backtracking control above the phrase level (where most expensive backtracking would occur), which
  avoids combinatorial explosion.
- Phrases are recognized by an **ATN interpreter** (Woods 1970) with two grammars: noun groups and
  verb groups.

**Noun group (NGR)** — definition starts at the bottom of p.40 and continues on p.41. It is one of:

<!-- book p.41 (book_p040.png) -->

- a pronoun (examples: *him*, *several*)
- a pronoun with modification (*almost anything green*)
- an integer, modified or not (*five*, *nearly a hundred thousand*)
- a noun phrase up to and including its head noun (*every third car*, *his own red and black car*)
- any of the above preceded by a preposition (this is how prepositional phrases arise)

**Verb group (VGR):** a verb phrase without direct or indirect objects
(*could almost see*, *might not have been moving*, *had been very yellow*).

**Verbal (VBL):** also recognized by the verb-group network; either an infinitive phrase
(*to walk slowly*, *to be broken*) or a participial phrase (*walking slowly*, *have almost given*).

## 4.2 Input

The input is the DECOMP output file (described in ch. 3). Per word it supplies, for each homograph
of each morph in the word's analysis: morph spelling, morph pronunciation, morph type, and parts of
speech with features. It also supplies one part-of-speech set for the whole word.

## 4.3 Output

The parser emits a sequence of **nodes**. A node is either a parsed constituent (a phrase) or a
single word/punctuation mark that did not get included in any phrase. A phrase node lists the words
it covers in text order. Output file contents:

1. Per node: number of words covered, the node's part of speech (constituent type), and a
   **property list** (a set of attribute–value pairs).
2. Per word: its spelling and a part-of-speech set. Words covered by a node carry exactly one part
   of speech.
3. Per part of speech: a property list.

## 4.4 Parts of speech

### 4.4.1 Standard parts of speech in the lexicon

These are open-class POS and words with no special syntactic/prosodic features. Uppercase names are
exactly as in the lexicon source. A word may have any number of POS. `TR` = true, `FL` = false.

| Lexicon form | Meaning |
|---|---|
| `NOUN (NUM SING)` | singular noun |
| `NOUN (NUM PL)` | plural noun |

<!-- book p.42 (book_p041.png) -->

| Lexicon form | Meaning |
|---|---|
| `VERB (INF TR) (PL TR)` | infinitive form of verb |
| `VERB (SING TR) (PL TR)` | past tense verb |
| `ADJ` | adjective |
| `ADV (VMOD TR) (ADJMOD TR)` | adverb that can modify either an adjective or a verb |
| `ADV (ADJMOD TR)` | adverb that can modify an adjective |
| `PREP` | preposition |
| `CONJ` | conjunction |
| `INTG` | integer |
| `INTG (NUM SING)` | *one* |
| `INTG (DEF FL)` | integer that requires *a* (e.g. *thousand*) |
| `VERBING` | present participle |
| `VERBEN` | past participle |
| `TO` | *to* |
| `SCONJ` | sentential conjunction (e.g. *whether*) |
| `CONTR` | contraction (e.g. *'re*) |
| `INTERJ` | interjection (e.g. *oh*) |

### 4.4.2 Special parts of speech

Punctuation gets one of three internal POS:

- `COMMA` — only the comma.
- `IPM` (internal punctuation mark) — sentence-internal marks: `:` `;` `(` `)` `'` `"`
- `EPM` (end punctuation mark) — marks that can end a sentence.

Words with special syntactic or prosodic features (determiner distinctions follow from the grammar):

**Adverb features**

| Feature | Meaning / licensed construction |
|---|---|
| `(MEAS TR)` | can occur as in *nearly a hundred ladders* |
| `(DETMOD TR)` | can modify a determiner, as in *almost any space* |
| `(NEGADV TR)` | can occur with indefinite article + count noun, as in *hardly a salesperson* |
| `(NOT TR)` | on *not* and *never*; triggers certain prosodic effects |

<!-- book p.43 (book_p042.png) -->

**Predeterminer `PDET`** — POS of a word that can precede a determiner (*half his land*,
*twice the money*). Features:

| Feature | Meaning |
|---|---|
| `(MEAS TR)` | predeterminer can co-occur with a measure adverb (*almost all two hundred divers*) |
| `(DEF TR)` | following determiner must be definite (*all the pain* OK; *all a pain* bad) |
| `(DEF FL)` | the opposite (*such the problem* bad; *such a problem* OK) |
| `(DET TR)` | predeterminer must be followed by some determiner |
| `(OF FL)` | predeterminer cannot be followed by *of* (contrast *both of the gnomes*) |
| `(TYPE A)` | predeterminer causes certain quantifier-related prosodic effects |

**Pronouns `PRN`**: `(QUANT A)` is the pronoun counterpart of PDET `(TYPE A)`. `CASE` and `NUM` are
ordinary case and number. `TYPE` is present for prosodic reasons. `(DETMOD TR)` means the pronoun can
be modified by a determiner-modifying adverb (*nearly everyone*).

**Determiners** — four types:

| POS | Meaning |
|---|---|
| `DETW` | wh-word determiner |
| `DETQ` | quantified determiner. Differs from quantifiers (`QUANT`) in that it can share a noun group with an ordinal or integer (*every third Eskimo*, *any six infants*). `DETMOD` means the same as for pronouns; `TYPE` and `QUANT` are prosodic indicators. |
| `DEM` | demonstrative |
| `ART` | article |

**Ordinals `ORD`**: include *next* and *last* plus the ordinal integers. Fraction words such as
*quarters*, *thirds* are listed as `ORD (NUM PL)` so that *three quarters the money* and
*two thirds the money* parse. `(DEF TR)` on these means the preceding determiner must be definite.

**Quantifiers `QUANT`**: normally marked for number agreement and definiteness agreement with the
preceding determiner.

**Modals `MOD`**: carry attribute `AUX` (prosodic information). `(TO TR)`: modal can occur as in
*ought to deliver*. `(TO BE)`: word must appear with both *be* and *to*, as in
*was going to abscond*. Remaining `BE` and `HAVE` words have their ordinary meaning.

## 4.5 The part-of-speech processor (summary; full procedure in Appendix A)

Part of DECOMP. Computes a POS set for each input word from its morph decomposition and the morph
POS. Based on Allen's 1968 "Preprocessor".

<!-- book p.44 (book_p043.png) -->

Core idea: scan morphs right-to-left; for compounds and for suffixed words, the rightmost morph's
POS decides. Rationale given: (1) the outermost (rightmost) suffix regularly determines word POS
(e.g. *-ness* → NOUN); (2) compound POS is idiosyncratic (largely semantic), and the rightmost
root's POS set is the best heuristic.

Procedure, in order (wording ours; "previous/next-to-last morph" = the morph immediately left of the
last one):

1. **No decomposition** → call the *default* routine: assign
   `(NOUN (NUM SING), VERB (PL TR) (INF TR), ADJ)`, unless the word ends in `'S`, in which case
   assign `((NOUN (POSS TR), NOUN (NUM SING) (CONTR TR))`.
   *(The opening parenthesis count in the printed set is unbalanced: `((NOUN (POSS TR), NOUN (NUM SING) (CONTR TR))` [sic?].)*
   *(Appendix A's version of the no-decomposition case lists `(NOUN (NUM SING)), (VERB (INF TR) (PL TR)), (ADJ)`
   with the feature order `INF`/`PL` swapped relative to this page, and does not mention the `'S`
   exception there.)*
2. **Last morph is not a suffix** → check POS-determining prefixes: `EM`, `EN`, `BE` → VERB;
   `A` → `(ADJ, ADV)`. Suffixes take priority over these prefixes (example *befuddlement*).
   If none of these prefixes is present → use the POS set of the last morph.
3. **Otherwise dispatch on the last suffix** (often also inspecting the previous morph):
   - `ING` → `VERBING`.
   - `ED` → `(VERBEN, VERB (SING TR) (PL TR))`.
   - `S` or `ES`, checks in order:
     - previous morph is not a suffix and there is a verb-producing prefix → `VERB (SING TR)` (*entitles*);
     - previous morph has POS VERB → the same POS is assigned;
     - previous morph is NOUN, ADJ, or INTG, or is `ER` or `ING` → add `NOUN (NUM PL)` to the set;
     - previous morph is ORD → also add `ORD (NUM PL)`;
     - still no POS → `NOUN (NUM PL)` (*the whys and wherefores*).
   - `ER`, three checks: previous morph ADV → comparative adverb; ADJ → comparative adjective;
     NOUN or VERB → singular NOUN (*worker*).
   - `S'` → NOUN with `(POSS TR)`.

<!-- book p.45 (book_p044.png) -->

   - `'S`, three checks:
     - previous morph is NOUN → `(NOUN (POSS TR), NOUN .... (CONTR TR))`, where `....` stands for
       the previous morph's features (e.g. `(NUM PL)`);
     - previous morph is PRN → PRN with the previous morph's features plus `(CONTR TR)`; if that
       morph also has `(PRNADJ TR)` (pronouns ending in *body*, *one*, *thing*), additionally
       include PRN with the prior morph's features plus `(CASE POSS)` (*anybody's*).
   - Contractions (last three dispatch cases):
     - `N'T`: if previous morph is `NEED` → POS `MOD` with `(AUX A) (NOT TR)` (*needn't*);
       if previous morph is BE, HAVE, or MOD → just add `(NOT TR)`.
     - `'VE` with a modal as previous morph → previous morph's POS plus `(CONTR TR)` (*must've*).
     - `'VE`, `'D`, `'LL`, `'RE`: if the prior morph is the plural morph `S` → NOUN with
       `(NUM PL) (CONTR TR)` (*The kids've been busy*, *The boys'll go*); otherwise, if previous
       morph is NOUN or PRN → add `(CONTR TR)` to its feature set.
   - Any other last suffix → word POS set = that morph's POS set.
4. If the word still has no POS (e.g. *only's*) → call the default routine as in step 1.

## 4.6 The parser algorithm

### 4.6.1 Parsing strategy

Reads DECOMP output one sentence at a time and looks for phrases. Two levels:

**Global level (strategy).** Motivated by three empirical observations: noun groups and PPs vastly
outnumber verb groups in running text; noun-group beginnings are easier to spot than verb-group
beginnings (verb groups often start with the verb, which is frequently NOUN/VERB ambiguous); and

<!-- book p.46 (book_p045.png) -->

nouns are often compounded into classifier strings (*cathode ray tube cleaning fluid*).

Algorithm at the current word pointer:

1. Find the **longest** noun group (NP up to the head noun, optionally with an initial preposition)
   starting at the pointer. If found: build a node for it, move the pointer to the word after it,
   repeat.
2. Else find the longest verb group starting at the pointer. If found: build a node, advance, repeat.
3. Else: build no node, advance the pointer by one word, repeat.

**Local level.** Just interprets the ATN grammar. Two pointers: current word, and current net state
(initialized to the noun-group or verb-group start state). Arcs leaving a state are tried **in the
order they appear in the net** (Figures 4-1 and 4-2; the text says "Tables 4-1 … and 4-2", which are
printed as Figures 4-1 and 4-2 [sic?]).

Arc testing:

1. Arc label `JUMP` or `POP`: test the arc's exit routine.
   - Success, `JUMP`: move state pointer to the destination; word pointer unchanged; continue there.
   - Success, `POP`: build a node if the popped constituent is longer than any found so far, then
     continue with the **next arc of the same state** (search is exhaustive).
   - Failure: try the next arc of the state.
2. Arc label is a part of speech:
   - Current word lacks that POS → try next arc.
   - Word has that POS → test exit routine. Success: increment word pointer, move to destination
     state. Failure: try next arc.
   - **Backtracking:** if no arcs remain in a state, reset the state pointer to the state from which
     the arc that led here originated, and resume with the next untried arc in that state.

<!-- book p.47 (book_p046.png) -->

### Figure 4-1: Noun group ATN listing

Each arc is `LABEL DESTINATION EXIT-ROUTINE`, listed in trial order. Recorded exactly as printed
(including the capitalization `NG-INtg`).

```
NG          VERBING NG-Ving IngVbl
            TO VG-Inf Vbl
            PREP NG-Adv PP
            TO NG-Adv PP
            JUMP NG-Adv NG

NG-Ving     JUMP NG-Adj1 IngAdj
            JUMP NG-N1 IngNoun
            POP NG-Ving Vbl

NG-Adv      ADV NG-Pdet NA
            QUANT NG-Than MorLes
            ADJ NG-Adj Adj
            JUMP NG-Pdet OK

NG-Than     SCONJ NG-ThanA Than

NG-ThanA    ART NG-INtg ThanA
            JUMP NG-Intg OK

NG-Pdet     INTG NG-Times FracNum
            PRN NG-Own Pmps
            PDET NG-Det PD
            DETW NG-Ord DW
            DETQ NG-Ord DQ
            JUMP NG-Det OK
            PRN NG-Prn Pr

NG-Prn      ADV NG-Prn PrnAdv
            ADJ NG-PA PA
            PREP NG-Pdet PrnOf
            POP NG-Prn PrPop

NG-Times    ORD NG-Det FracDen
            NOUN NG-Det Times

NG-Det      NOUN NG-Own Poss
            PRN NG-Own PPoss
            DEM NG-Ord Dem
            ART NG-Ord Art
            PREP NG-Pdet PdetOf
            JUMP NG-Ord NeedDet

NG-Quant    QUANT NG-Adj Quant
            PREP NG-Pdet DetOf
            JUMP NG-Intg OK

NG-PA       POP NG-PA OK

NG-Ord      ORD NG-Quant Ord
            JUMP NG-Quant OfTest
            ADJ NG-Intg IntMod

NG-Own      ADJ NG-Ord Own
            DETQ NG-Adj Every
            JUMP NG-Ord OK
            POP NG-Own PopP

NG-Intg     INTG NG-Intg1 Intg
            JUMP NG-Adj NoMeas

NG-Intg1    INTG NG-Intg1 IntA
            CONJ NG-Frac And
            JUMP NG-IntOrd OK

NG-IntOrd   ORD NG-Adj IntOrd
            JUMP NG-Adj OK

NG-Frac     INTG NG-Denom Numer
            ART NG-Denom A

NG-Denom    ORD NG-Adj Denom

NG-Adj      ADV NG-Adj AdvAdj
            ADJ NG-Adj1 Adj
            VERBEN NG-Adj1 Adj
            VERBING NG-Adj1 VngAdj
            PREP NG-Pdet QuantOf
            JUMP NG-N NeedAdj
            POP NG-Adj IntPop

NG-Adj1     CONJ NG-Adj AConj
            COMMA NG-Adj AComma
            JUMP NG-Adj OK

NG-N        NOUN NG-Own PossN
            NOUN NG-N1 Noun
            VERBING NG-N1 ConjVing
            INTG NG-N1 NIntg
            PREP NG-Adv NounOf
            POP NG-N PopN

NG-N1       CONJ NG-N NConj
            COMMA NG-N NComma
            JUMP NG-N OK
```

Notes on the listing:
- `NG-Prn` is printed in the scan as "NG-Pm"/"NG-Prn" (the low-resolution "rn" looks like "m");
  read as `NG-Prn` from the diagram (state `PRN`). The exit routines `PmAdv`/`PrnAdv`, `PmOf`/`PrnOf`,
  `Pmps`/`Prnps` have the same ambiguity [?]; the most likely readings are `PrnAdv`, `PrnOf`, and
  `Pmps` vs. `Prnps` is undecidable from the scan [?].
- `NG-ThanA → ART NG-INtg` is printed with capital "IN" [sic?]; presumably `NG-Intg`.
- The book does not define the exit routines (OK, PP, Vbl, NA, MorLes, …) beyond their names on
  these pages.

<!-- book p.48 (book_p047.png) -->

### Figure 4-2: Verb group ATN listing

```
VG          ADV VG Adv
            MOD VG-Inf Mod
            MOD VG-To ModTo
            HAVE VG-Have Have
            MOD VG-Have ModCntr
            BE VG-Part Be
            VERB VG-Pop Verb
            JUMP VG-Inf Vbl
            JUMP VG-Have Vbl

VG-Inf      ADV VG-Inf Adv
            MOD VG-To Get
            HAVE VG-Have HavInf
            BE VG-Part BInf
            VERB VG-Pop VInf

VG-To       ADV VG-To Adv
            TO VG-Inf OK

VG-Have     ADV VG-Have Adv
            MOD VG-To Got
            BEEN VG-Part Been
            JUMP VG-Part NoIng
            JUMP VG-To OK

VG-Part     MOD VG-To BeMod
            ADV VG-Part CopAvj
            BEING VG-Part Being
            VERBING VG-Pop Ving
            VERBEN VG-Pop En
            JUMP VG-Cop Cop

VG-Cop      ADJ VG-Pop OK
            JUMP VG-Pop CopNoAj

VG-Pop      ADV VG-Pop NoPrep
            POP VG-Pop PopV
```

Note: arc labels `BEEN` and `BEING` appear here although they are not in the POS list of 4.4.1
(presumably features/sub-POS of BE words).

### 4.6.2 The verb group grammar

The verb-group net (Figure 4-3) is the simpler grammar: fewer arcs, states, alternate paths and exit
routines; only two POP arcs; a well-defined auxiliary structure; and no case where one word's
multiple POS force two paths to be explored.

Test phrases that traverse the basic net successfully:

```
sometimes runs                 usually would have been jumping
is being run                   have to go
would have been seen           about to be done
```

The basic grammar was extended with modal arcs for spoken English. Examples:

```
get to run                     does get used
get to go                      can't possibly get to see
```

**Known limitation — particles are not handled.** In *He picked up the books* the parser yields three
noun groups: *he*, *picked up*, *the books* (*picked* taken as a past participle used adjectivally,
*up* as a noun, cf. *It's a real up*). In *He ran out of the room*, *ran* is a verb group, *out* is a
noun group (cf. *How many outs does the team have?*), and *of* begins the PP *of the room*.

<!-- book p.49 (book_p048.png) -->

### Figure 4-3: ATN diagram for verb groups

The diagram is a drawing of the Figure 4-2 net. States (boxes): `VG` (start), `INF`, `TO`, `HAVE`,
`PART`, `COP`, `POP`, plus an external box `NG`. Every arc drawn, with its label(s):

| From | To | Arc label(s) on the drawing |
|---|---|---|
| NG | INF | TO |
| VG | VG (self-loop) | ADV |
| VG | INF | JUMP, MOD |
| VG | TO | MOD |
| VG | HAVE | HAVE, MOD, JUMP |
| VG | PART | BE |
| VG | POP | VERB (long line over the top of the diagram) |
| INF | INF (self-loop) | ADV |
| INF | TO | MOD |
| INF | HAVE | HAVE |
| INF | PART | BE |
| INF | POP | VERB |
| TO | TO (self-loop) | ADV |
| TO | INF | TO |
| HAVE | HAVE (self-loop) | ADV |
| HAVE | TO | MOD, JUMP |
| HAVE | PART | BEEN, JUMP |
| PART | PART (self-loop) | ADV, BEING |
| PART | TO | MOD |
| PART | POP | VERBEN, VERBING |
| PART | COP | JUMP |
| COP | POP | JUMP, ADJ |
| POP | POP (self-loop) | ADV |
| POP | (exit) | POP |

This agrees arc-for-arc with Figure 4-2.

(Remainder of p.49 continues 4.6.2's particle discussion, then:)

Particle constructions might be parsed using a feature on the verb, but that feature would also let
prepositions be wrongly taken as particles.

### 4.6.3 The noun group grammar

The noun-group net (Figure 4-4) is the more complex grammar, with many arcs, branches, and exit
tests. Test phrases: left column should be accepted; starred ones should be blocked.

```
almost any book                *book
about two fifths the book      *almost him
the many books                 *a many books
both these women               *both this women
every other book               *every other books
any two books                  *some few books
every few books                *a few book
that few books                 *that much book
that many books                *his much money
a woman's many books           *a book women
```

The grammar also has optional arcs ("sidetracks"). Examples that traverse them:

```
three quarters                 his own shoes
more than five shoes           three and a half
```

<!-- book p.50 (book_p049.png) -->

### Figure 4-4: ATN diagram for noun groups

A drawing of the Figure 4-1 net. Boxes: `NG` (start), `VG/INF` (external), `ADV`, `PRN`, `PA`,
`PDET`, `TIMES`, `DET`, `OWN`, `ORD`, `THAN`, `THANA`, `QUANT`, `INTG`, `INTG1`, `FRAC`, `VING`,
`ADJ1`, `ADJ`, `INTORD`, `DENOM`, `N1`, `N`. Arcs drawn:

| From | To | Label(s) |
|---|---|---|
| NG | VG/INF | TO |
| NG | VING | VERBING |
| NG | ADV | JUMP, PREP, TO |
| ADV | ADJ | ADJ |
| ADV | THAN | QUANT |
| ADV | PDET | ADV, JUMP |
| THAN | THANA | SCONJ |
| THANA | INTG | ART, JUMP |
| PDET | PRN | PRN |
| PDET | TIMES | INTG |
| PDET | OWN | PRN |
| PDET | DET | PDET, JUMP |
| PDET | ORD | DETW, DETQ |
| PRN | PRN (self-loop) | ADV |
| PRN | PA | ADJ |
| PRN | PDET | PREP |
| PRN | (exit) | POP |
| PA | (exit) | POP |
| TIMES | DET | NOUN, ORD |
| DET | PDET | PREP |
| DET | OWN | NOUN, PRN |
| DET | ORD | DEM, ART, JUMP |
| OWN | ORD | JUMP, ADJ |
| OWN | ADJ | DETQ |
| OWN | (exit) | POP |
| ORD | QUANT | ORD, JUMP |
| ORD | INTG | ADJ |
| QUANT | PDET | PREP |
| QUANT | INTG | JUMP |
| QUANT | ADJ | QUANT |
| INTG | INTG1 | INTG |
| INTG | ADJ | JUMP |
| INTG1 | INTG1 (self-loop) | INTG |
| INTG1 | FRAC | CONJ |
| INTG1 | INTORD | JUMP |
| FRAC | DENOM | INTG, ART |
| DENOM | ADJ | ORD |
| INTORD | ADJ | ORD, JUMP |
| VING | ADJ1 | JUMP |
| VING | N1 | JUMP |
| VING | (exit) | POP |
| ADJ | ADJ (self-loop) | ADV |
| ADJ | ADJ1 | VERBING, ADJ, VERBEN |
| ADJ1 | ADJ | JUMP, CONJ, COMMA |
| ADJ | PDET | PREP |
| ADJ | N | JUMP |
| ADJ | (exit) | POP |
| N | N1 | VERBING, NOUN, INTG |
| N1 | N | JUMP, CONJ, COMMA |
| N | OWN | NOUN |
| N | ADV | PREP |
| N | (exit) | POP |

This agrees with Figure 4-1. The listing (Figure 4-1) is authoritative for arc order.

<!-- book p.51 (book_p050.png) -->

Further sidetrack examples (continuing the two-column list):

```
red, white and blue            three and twenty blackbirds
```

and simple pronoun noun groups such as *he*.

## 4.7 Some examples

Figure 4-5 shows parser output on part of a Stanford computer-taught course (end of a section on
predicate calculus).

### Figure 4-5: Example of PARSER operation — output format

Each output line is `PARSER: <TYPE>: <words>`; the stream ends with `PARSER: <EOF>`. Observed
`<TYPE>` values: `NOUN GROUP`, `VERB GROUP`, `PREPOSITIONAL PHRASE`, `UNCLASSIFIED`. Words are
uppercase. Opening lines of the figure, as format reference:

```
PARSER: NOUN GROUP: MOST OF THE EXERCISES
PARSER: VERB GROUP: ARE
PARSER: NOUN GROUP: TRANSLATIONS
PARSER: UNCLASSIFIED: .
PARSER: VERB GROUP: THERE ARE
PARSER: NOUN GROUP: SEVERAL IMPORTANT CHANGES
PARSER: PREPOSITIONAL PHRASE: IN THE WAY
```

Behaviours visible in the rest of the figure (not reproduced in full here; see scan):
- Every punctuation mark (`.`, `,`) is its own `UNCLASSIFIED` node; runs of several consecutive
  `UNCLASSIFIED: .` lines occur (sentence/paragraph ends).
- Single words that start neither a noun group nor a verb group become `UNCLASSIFIED`
  (examples in the figure: *FIRST*, *IMMEDIATELY*, *IF*, *ANYMORE*).
- A PP may absorb a following *of*-PP (`FOR THE REMAINDER OF THE COURSE`).
- Contracted negatives form verb groups (`WON'T COST`, `CAN'T USE`).
- Pronoun objects form their own noun groups (`YOU`).
- Consecutive PPs are separate nodes (`WITH A SHARP` then `IN THEM` [sic in source output]).
