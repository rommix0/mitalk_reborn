# Appendix A — Part-of-speech processor; Appendix B — Klatt symbols (implementation notes)

Source: Allen, Hunnicutt & Klatt, *From Text to Speech: The MITalk System* (1987), Appendices A–B.

**Format note.** Appendix A's pseudocode is restated here as a structured decision procedure in our
own layout. It is not a verbatim copy, but every condition, part of speech, and feature is as
printed. Appendix B's symbol tables are factual data and are recorded exactly.

**Page numbering.** Markers give PRINTED page numbers. `book_pNNN.png` shows printed page NNN+1.
Printed p.177 comes from `book_fix/printed_p177.png`. Appendix B ends at printed p.180
(`book_p179.png`). `book_p180.png` (printed p.181, Appendix C) is skipped.

---

<!-- book p.177 (book_fix/printed_p177.png) -->

## Appendix A: Part-of-speech processor

Terminology used by the procedure:
- **last morph** = rightmost morph of the decomposition. **next morph** = the morph immediately to
  its left (the scan runs right-to-left). **first morph** = leftmost morph.
- **assign X** = the POS set gets X as written.
- **copy (P (F V))** = take the next morph's entry of part of speech P with all its features and add
  feature (F V). This matches ch. 4: "the previous morph's features and the additional property".
- POS entries that are assigned together form a set (comma-separated below).

Decision procedure. Branches are tested top to bottom and the first match wins (IF / ELSEIF chain).
Inside a branch, "then also" items are separate IFs, each tested independently.

1. **No decomposition** → assign `(NOUN (NUM SING))`, `(VERB (INF TR) (PL TR))`, `(ADJ)`.
2. **Last morph is not a suffix:**
   - first morph is a verb prefix → assign `(VERB (INF TR) (PL TR))`
   - else first morph is `A` → assign `(ADJ)`, `(ADV)`
   - else → assign from last morph.
3. **Last morph is `ING`** → assign `(VERBING)`.
4. **Last morph is `ED`** → assign `(VERBEN)`, `(VERB (SING TR) (PL TR))`.
5. **Last morph is `S` or `ES`:**
   - if next morph is not a suffix AND first morph is a verb prefix → assign `(VERB (SING TR))`
   - otherwise, apply all of the following in order:
     - a. if next morph is a verb → assign `(VERB (SING TR))`
     - b. if next morph is a NOUN, ADJ, INTG, `ER`, or `ING` → assign `(NOUN (NUM PL))`
     - c. if next morph is an ORD AND next morph is not `SECOND` → assign `(ORD (NUM PL))`
     - d. if there is still no assignment → assign `(NOUN (NUM PL))`
6. **Last morph is `ER`**, apply all of the following:
   - a. next morph is an ADV → assign `(ADV)`
   - b. next morph is an ADJ → assign `(ADJ)`
   - c. next morph is a NOUN or VERB →

<!-- book p.178 (book_p177.png) -->

     assign `(NOUN (NUM SING))`
7. **Last morph is `S'`** → assign `(NOUN (POSS TR))`.
8. **Last morph is `'S`:**
   - next morph is a NOUN → assign `(NOUN (POSS TR))`, and copy `(NOUN (CONTR TR))`
   - else next morph is a PRN → copy `(PRN (CONTR TR))`. Also, if the next morph has feature
     `(PRNADJ TR)`, copy `(PRN (CASE POSS))`.
9. **Last morph is `N'T`:**
   - next morph is `NEED` → assign `(MOD (AUX A) (NOT TR))`
   - else next morph is a BE → copy `(BE (NOT TR))`
   - else next morph is a HAVE → copy `(HAVE (NOT TR))`
   - else next morph is a MOD → copy `(MOD (NOT TR))`
10. **Last morph is `'VE` AND next morph is a MOD** → copy `(MOD (CONTR TR))`.
11. **Last morph is `'VE`, `'D`, `'LL`, or `'RE`:**
    - next morph is `S` → assign `(NOUN (NUM PL) (CONTR TR))`
    - else next morph is a NOUN → copy `(NOUN (CONTR TR))`
    - else next morph is a PRN → copy `(PRN (CONTR TR))`
12. **Otherwise** → assign part of speech from the rightmost morph.

### Differences from the prose description in ch. 4 §4.5 (recorded, not resolved)

| Point | Ch. 4 prose (pp.44–45) | Appendix A |
|---|---|---|
| No decomposition, word ends in `'S` | special set `((NOUN (POSS TR), NOUN (NUM SING) (CONTR TR))` | not mentioned |
| Verb prefixes | names `EM`, `EN`, `BE` → `VERB` | "a verb prefix" → `(VERB (INF TR) (PL TR))` |
| `S`/`ES` after a VERB | "the same part of speech is assigned" | `(VERB (SING TR))` |
| `S`/`ES` after ORD | ORD → `ORD (NUM PL)` | adds the exception: not when next morph is `SECOND` |
| `N'T` after BE/HAVE/MOD | "adds the property (NOT TR)" | copy with `(NOT TR)` (same effect) |
| Still no POS after dispatch (e.g. *only's*) | call the default routine | not shown (branch 12 only) |

<!-- book p.179 (book_p178.png) -->

## Appendix B: Klatt symbols

Scan note: on this page someone has hand-drawn circles around most symbols. These marks are not
part of the print. Lightly circled or uncircled symbols include `EY`, `IX`, `RX`, `DD`, `KP`, `GG`,
`JJ`, `AXP`. The circles carry no information for the implementation.

### Table B-1: Klatt symbols for phonetic segments

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

(This is identical to the segment part of Table 8-1 on printed p.84.)

<!-- book p.180 (book_p179.png) -->

### Table B-2: Klatt symbols for nonsegmental units

**Stress symbols**

| Sym | Meaning |
|---|---|
| `'` or `1` | primary lexical stress |
| `"` or `2` | secondary lexical stress |

**Word and morpheme boundaries**

| Sym | Meaning |
|---|---|
| `-` | syllable boundary |
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

Difference from Table 8-1: Table 8-1 says the syllable boundary `-` is "(ignored)". Table B-2 omits
that note.
