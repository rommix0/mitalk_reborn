# Chapter 9 — The prosodic component (PROSOD): segmental duration rules

Source: Allen, Hunnicutt & Klatt, *From Text to Speech: The MITalk System* (1987), pp. 93–99.

> **Format note:** This is a technical restatement for reimplementation, **not a verbatim transcription**. All numbers, symbols, tables, equations and rule numbering are exactly as printed. The prose is paraphrased. Page markers use **printed** page numbers.

<!-- book p.093 -->

## 9.1 Overview

- **Input:** the sentence representation from the phonological component **PHONO2**.
- **Output (PROSOD):** a string of phonetic segments. Each segment gets a **stress feature** and a **duration in msec**. Figure 9-1 shows an example input and output.
- The **F0 targets** in the PROSOD output listing come from an **obsolete algorithm**. **F0TARG discards them** and generates the proper F0 targets (Ch. 10).

## 9.2 Segmental durations

- Based on a review of the factors affecting segmental durations in English sentences (Klatt, 1976b): only a few of the many rule-governed durational changes are big enough to be perceptually discriminable. The rule system (here and in Klatt, 1979b) targets these perceptually important first-order effects.
- **Segment-boundary conventions** used when measuring durations:
  - **Stop:** the duration covers the **closure**. Any burst and aspiration at release count as part of the **following** segment.
  - **Fricative:** the interval of **visible frication noise** (or of voicing-source changes if no frication is visible).
  - **Sonorant sequences:** the boundary is the **half-way point of the formant transition** of whichever formant has the greatest extent of transition.
  - These conventions give a convenient, largely reproducible measurement procedure. Their physiological and perceptual validity has not been established.
- The rules aim to match observed durations for **one speaker (DHK)** reading paragraph-length material.
- The underlying model (Klatt, 1973): 1) each rule tries to make a **percentage** increase or decrease in the segment's duration; 2) segments **cannot be compressed below a minimum duration**. The model is the formula given as Equation 1 below (printed at the top of p.94, after Figure 9-1).

<!-- book p.094 -->

## Figure 9-1: Example of the processing performed by PROSOD

Input sentence: **The old man sat in a rocker.**

PHONO2 input stream (word class, phonemes):

| # | PHONO2 item | Phonemes / content |
|---|---|---|
| 1 | Function word | DH IY |
| 2 | Content word | 'OW LX DD |
| 3 | Content word | MM 'AE NN [End NOUN phrase] |
| 4 | Content word | SS 'AE DX |
| 5 | Function word | IH NN |
| 6 | Function word | AX |
| 7 | Content word | RR 'AA KK * - ER |
| 8 | Punctuation | . |
| 9 | <EOF> | |

PROSOD output (segment, duration, F0, stress flag), in printed order:

| Word header | Segment | Duration | F0 | Flag |
|---|---|---|---|---|
| | [Silence] | 30ms. | 133.4Hz. | |
| Function word | DH | 50ms. | 123.4Hz. | |
| | IY | 105ms. | 131.4Hz. | |
| Content word | 'OW | 170ms. | 174.5Hz. | Stressed |
| | LX | 75ms. | 151.0Hz. | |
| | DD | 50ms. | 146.0Hz. | |
| Content word | MM | 70ms. | 151.0Hz. | Stressed |
| | 'AE | 210ms. | 157.0Hz. | Stressed |
| | NN | 55ms. | 117.9Hz. | |
| | [End NOUN phrase] | | | |
| Content word | SS | 100ms. | 122.9Hz. | Stressed |
| | 'AE | 175ms. | 153.9Hz. | Stressed |
| | DX | 20ms. | 140.1Hz. | |
| Function word | IH | 55ms. | 148.1Hz. | |
| | NN | 50ms. | 142.5Hz. | |
| Function word | AX | 60ms. | 142.5Hz. | |
| Content word | RR | 80ms. | 140.2Hz. | Stressed |
| | 'AA | 160ms. | 146.2Hz. | Stressed |
| | KK | 65ms. | 113.1Hz. | |
| | * | | | |
| | - | | | |
| | ER | 170ms. | 108.1Hz. | |
| Punctuation: . | [Silence] | 400ms. | 111.2Hz. | |
| | [End sentence] | | | |
| | <EOF> | | | |

(The F0 column in this figure comes from the F0 module of Ch. 10. `*` and `-` are syllable/morph boundary symbols carried through with no duration.)

## Equation 1

    DUR = ((INHDUR - MINDUR) * PRCNT) / 100 + MINDUR        (1)

- INHDUR: the segment's inherent duration, in msec.
- MINDUR: the segment's minimum duration, in msec.
- PRCNT: the percentage shortening found by applying rules 1 to 10 below.

Procedure: look up INHDUR and MINDUR for the current segment in Table 9-1 and set PRCNT = 100. INHDUR has no special status beyond being the starting point for the rules. It is roughly the duration expected in nonsense CVCs spoken in the carrier phrase "Say bVb again" / "Say Cab again". Rules 1–10 are then applied in turn, each one modifying the PRCNT left by the earlier rules by a factor PRCNT1:

<!-- book p.095 -->

    PRCNT = (PRCNT * PRCNT1) / 100        (2)

Then the final PRCNT goes into Equation 1 to get the duration, and **Rule 11** is applied last. The rules were tuned by trial and error against hand-segmented, labeled spectrograms of paragraphs read by speaker DHK.

## Rules

1. **Pause insertion rule.** Insert a **200-msec** pause before every sentence-internal main clause and at boundaries marked by a syntactic comma. Do not insert one before relative clauses. (Goldman-Eisler, 1968; Cooper et al., 1978.) In the duration rules, the "(R" symbol behaves like a ")N".

2. **Clause-final lengthening.** The vowel or syllabic consonant in the syllable just before a pause gets **PRCNT1 = 140**. Any consonants between that vowel and the pause also get **PRCNT1 = 140**. (Gaitenby, 1965; Lindblom and Rapp, 1973; Oller, 1973; Klatt, 1975.)

3. **Non-phrase-final shortening.** Syllabic segments (vowels and syllabic consonants) get **PRCNT1 = 60** if they are not in a phrase-final syllable. A phrase-final postvocalic liquid or nasal gets **PRCNT1 = 140**. (Lindblom and Rapp, 1973; Klatt, 1975.)

4. **Non-word-final shortening.** Syllabic segments get **PRCNT1 = 85** if they are not in a word-final syllable. (Lindblom and Rapp, 1973; Oller, 1973.)

5. **Polysyllabic shortening.** Syllabic segments in a polysyllabic word get **PRCNT1 = 80**. (Lindblom and Rapp, 1973; Lehiste, 1975a.)

6. **Non-initial-consonant shortening.** Consonants that are not word-initial get **PRCNT1 = 85**. (Klatt, 1974; Umeda, 1977.)

<!-- book p.096 -->

## Table 9-1: Minimum and inherent durations in msec for each segment type

Column order as printed: segment, **minimum** (MINDUR), **inherent** (INHDUR). (The worked example for ER below agrees with this order: MINDUR 80, INHDUR 180.)

| Class | Segment | MINDUR | INHDUR |
|---|---|---|---|
| Vowels | AA | 100 | 240 |
| Vowels | AE | 80 | 230 |
| Vowels | AH | 60 | 140 |
| Vowels | AO | 100 | 240 |
| Vowels | AW | 100 | 260 |
| Vowels | AX | 60 | 120 |
| Vowels | AXR | 120 | 260 |
| Vowels | AY | 150 | 250 |
| Vowels | EH | 70 | 150 |
| Vowels | ER | 80 | 180 |
| Vowels | EXR | 130 | 270 |
| Vowels | EY | 100 | 190 |
| Vowels | IH | 40 | 135 |
| Vowels | IX | 60 | 110 |
| Vowels | IXR | 100 | 230 |
| Vowels | IY | 55 | 155 |
| Vowels | OW | 80 | 220 |
| Vowels | OXR | 130 | 240 |
| Vowels | OY | 150 | 280 |
| Vowels | UH | 60 | 160 |
| Vowels | UW | 70 | 210 |
| Vowels | UXR | 110 | 230 |
| Vowels | YU | 150 | 230 |
| Sonorant consonants | EL | 110 | 260 |
| Sonorant consonants | HH | 20 | 80 |
| Sonorant consonants | HX | 25 | 70 |
| Sonorant consonants | LL | 40 | 80 |
| Sonorant consonants | LX | 70 | 90 |
| Sonorant consonants | RR | 30 | 80 |
| Sonorant consonants | RX | 70 | 80 |
| Sonorant consonants | WW | 60 | 80 |
| Sonorant consonants | WH | 60 | 70 |
| Sonorant consonants | YY | 40 | 80 |
| Nasals | EM | 110 | 170 |
| Nasals | EN | 100 | 170 |
| Nasals | MM | 60 | 70 |
| Nasals | NN | 50 | 60 |
| Nasals | NG | 60 | 95 |
| Fricatives | DH | 30 | 50 |
| Fricatives | FF | 80 | 100 |
| Fricatives | SS | 60 | 105 |
| Fricatives | SH | 80 | 105 |
| Fricatives | TH | 60 | 90 |
| Fricatives | VV | 40 | 60 |
| Fricatives | ZZ | 40 | 75 |
| Fricatives | ZH | 40 | 70 |
| Plosives | BB | 60 | 85 |
| Plosives | DD | 50 | 75 |
| Plosives | DX | 20 | 20 |
| Plosives | GG | 60 | 80 |
| Plosives | GP | 40 | 80 |
| Plosives | KK | 60 | 80 |
| Plosives | KP | 40 | 80 |
| Plosives | PP | 50 | 90 |
| Plosives | TT | 50 | 75 |
| Plosives | TQ | 50 | 75 |
| Affricates | CH | 50 | 70 |
| Affricates | JJ | 50 | 70 |
| Pseudo-vowel | AXP | 70 | 70 |

(The printed table has three segment/min/inh groups per row. It is flattened here to one segment per row. The affricate is printed "JJ", not "JH". Table 11-2 uses "JH".)

<!-- book p.097 -->

7. **Unstressed shortening.** Unstressed segments are half again as compressible as stressed ones: set **MINDUR = MINDUR/2**. Then both unstressed and 2-stressed segments get a factor PRCNT1 that depends on segment type (table below). As a result, secondary-stress segments shorten relative to 1-stress, but less than unstressed segments do. (Umeda, 1975, 1977; Lehiste, 1975a.)

   | Context | PRCNT1 for -stress and 2-stress |
   |---|---|
   | syllabic (word-medial syllable) | 50 |
   | syllabic (others) | 70 |
   | prevocalic liquid or glide | 10 |
   | all others | 70 |

   (As printed, the MINDUR/2 step applies to unstressed segments only. The PRCNT1 factor applies to both unstressed and 2-stressed segments.)

8. **Lengthening for emphasis.** An emphasized vowel gets **PRCNT1 = 140** percent. (Bolinger, 1972; Carlson and Granstrom, 1973; Umeda, 1975.)

9. **Postvocalic context of vowels.**

   a) A vowel's duration depends on the postvocalic consonant **in the same word** (table below). In a postvocalic sonorant-obstruent cluster, the obstruent sets the effect on the vowel and also on the sonorant consonant. (House and Fairbanks, 1953; Peterson and Lehiste, 1960.)

   | Context | PRCNT1 |
   |---|---|
   | open syllable, word-final | 120 |
   | before a voiced fricative | 160 |
   | before a voiced plosive | 120 |
   | before a nasal | 85 |
   | before a voiceless plosive | 70 |
   | before all others | 100 |

   b) The effect is strongest at phrase and clause boundaries. If the vowel is **non-phrase-final**, pull PRCNT1 toward 100 with:

       PRCNT1 = 70 + 0.3*PRCNT1

   (Klatt, 1975.)

10. **Shortening in clusters.** Segments shorten in consonant-consonant sequences (word boundaries are ignored, but the sequence does not run across phrase boundaries) (Klatt, 1973), and in vowel-vowel sequences.

<!-- book p.098 -->

   | Context | PRCNT1 |
   |---|---|
   | vowel followed by a vowel | 120 |
   | vowel preceded by a vowel | 70 |
   | consonant surrounded by consonants | 50 |
   | consonant preceded by a consonant | 70 |
   | consonant followed by a consonant | 70 |

11. **Lengthening due to plosive aspiration.** A 1-stressed or 2-stressed vowel or sonorant preceded by a voiceless plosive is lengthened by **25 msec**. This is added after Equation 1. (Peterson and Lehiste, 1960.)

## Worked example (as printed)

The book applies the rules to "the RR of 'rocker' in Figure 9-1" [sic? The numbers fit **ER**, the word-final vowel of "rocker", not RR. See the note below.]:

- Rule 2 sets PRCNT to **140**.
- Rule 5 reduces PRCNT to **112**.
- Rule 7 reduces MINDUR to **30 msec** and PRCNT to **78.4**.
- Rule 9 raises PRCNT to **94**.
- INHDUR, MINDUR and PRCNT go into Equation 1. The result is **rounded up to the nearest 5 msec**, giving **175 msec**, which the text says is the value shown in the lower part of Figure 9-1.

Check against the printed numbers (my arithmetic, not from the book): with ER (MINDUR 80, INHDUR 180): 140 → ×0.80 = 112 → ×0.70 = 78.4 → ×1.20 = 94.08 ≈ 94. These match Rules 2, 5, 7 ("syllabic (others)" = 70) and 9a ("open syllable, word-final" = 120). MINDUR/2 for ER would be 40, not the printed 30. DUR = (180−30)×0.94+30 = 171, or with MINDUR 40, (180−40)×0.94+40 = 171.6. Either way it rounds up to 175. Figure 9-1, however, lists ER as **170ms**, and RR as **80ms**. These are inconsistencies in the source.

## Speaking rate

- Durations depend partly on the speaking-rate variable **SPRATE**, which can be set to any value from **60 to 300 words per minute**. The default is **180** wpm.
- At rates **slower than 150 wpm**, a short pause is inserted between a content word and a following function word. At normal rate, brief pauses are inserted only at clause ends.
- Individual segments lengthen or shorten slightly with rate, but most of the rate change comes from changing pause durations (Goldman-Eisler, 1968).
- The book gives no numeric formula for scaling segments or pauses by SPRATE on these pages.

## Evaluation (summary of reported figures)

- The rules are only a crude approximation (for example, consonant interactions in clusters) and ignore other factors.
- Against spectrograms of new paragraphs from DHK, rule durations differ from measured ones with a **standard deviation of 17 msec** (pause durations excluded). The rules account for **84 percent** of observed total variance in segmental durations. 17 msec is generally below the just-noticeable difference for a single change to segmental duration in sentences (Klatt, 1976a).
- Perceptual evaluation: Carlson et al. (1979).

<!-- book p.099 -->

- Naturalness and intelligibility ratings with rule durations were very close to those with durations taken from a natural recording.
- Related work: complete rule systems for English (Coker et al., 1973) and Swedish (Carlson and Granstrom, 1976). MITalk borrows heavily from the Lindblom and Rapp system as augmented and implemented by Carlson and Granstrom. Partial systems exist for vowels (Umeda, 1975; Liberman, 1977) and consonants (Umeda, 1977).
- Coker et al. use multiple stress levels conditioned by syntactic category (verbs less stressed than nouns) and by word frequency (common and repeated words reduced). Liberman (1977) has rhythm/isochrony rules. MITalk includes **neither explicitly**. Partial isochrony comes from the rules that shorten unstressed syllables and consonant clusters (see Carlson et al., 1979).
- Noun/verb durational differences are captured through phrase-final lengthening. The emphasis symbol **"!"** in the input captures word-frequency and discourse-expectancy effects in a binary way (it triggers Rule 8).
- Closing remarks (paraphrased): absolute judgements about which rule system is correct may be impossible. Effort should go into systematic optimization of one linguistically motivated rule system that draws on production data and perceptual constraints.
