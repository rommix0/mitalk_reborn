# Chapter 11 — The phonetic component (PHONET)

Source: Allen, Hunnicutt & Klatt, *From Text to Speech: The MITalk System* (1987), pp. 108–122.

> **Format note:** This is a technical restatement for reimplementation, **not a verbatim transcription**. All numbers, tables, symbols and section numbering are exactly as printed. The prose is paraphrased. The full rule set is in Appendix C of the book, not in this chapter.

<!-- book p.108 -->

## 11.1 Overview

- **Input:** from the F0 component **F0TARG**, an array with, for each phone: phonetic segment name, segmental stress feature, segmental duration, and **two F0 targets**.
- **Output:** values for **20 synthesizer control parameters every 5 msec**.

### 11.1.1 "Stored prosodics" synthesis

- PHONET plus the synthesizer can run **stand-alone**, with the segment string, durations and F0 specification hand-tuned (for example taken from a natural recording: extract F0, measure durations, choose segments to match the speaker's pronunciation, and format the result as PHONET input).
- This gives natural-sounding speech from an input of **about 250 bits per second**.
- Comparison: the authors believe it is slightly better in intelligibility and naturalness at **250 bits/second** than TI Speak-'N-Spell at **1200 bits/second**. The drawback is that PHONET input cannot be derived automatically, whereas Speak-'N-Spell data come from an LPC analyzer with minimal hand work.

### 11.1.2 Structure of PHONET

- A large array of **target values** for the control parameters for each of **about 60** phonetic segment types.
- Smoothing between targets depends on **time constants computed by rule** and on

<!-- book p.109 -->

  the **parameter value assigned at the segment boundary**.
- These constants come from rules on the features of the current segment **PHOCUR**, the previous segment **PHOLAS**, and the next segment **PHONEX**. Rarely, a rule looks further away. Example: in **pin**, voicing onset in the vowel after voiceless plosive **PP** is delayed by **about 50 msec**, unless the segment before the plosive is **SS**, as in **spin**.
- The variable control parameters are listed in Table 11-3.

### 11.1.3 History of formant synthesis-by-rule

- (Paraphrased background.) Following John Holmes, imitating natural speech depends mainly on matching observed short-term spectra, which reproduces all potential cues.
- Earlier rule strategies: Holmes et al., 1964; Mattingly, 1968a; Rabiner, 1968a; Coker et al., 1973; Klatt, 1972, 1976a. Their CV intelligibility falls well short of natural speech. Rabiner (1968a) estimated **85 percent** consonant intelligibility for synthetic CV nonsense stimuli versus **about 99 percent** for natural tokens, with phonetically trained listeners.
- Section 11.2 suggests earlier rule systems made the wrong generalizations.

## 11.2 "Synthesis-by-analysis" of consonant-vowel syllables

### 11.2.1 Analysis of CV syllables

- Data base: **six** talkers from one midwestern dialect region, **three male and three female** (Klatt, 1979b). All six were used to fix the **form** of the rules, but the parameter values in the rules

<!-- book p.110 -->

  came from a fuller analysis of **one male talker (DHK)**.
- Subjects read a list of **336** different CVC nonsense syllables once. DHK read it **twice on three separate occasions**.
- Analysis (Figure 11-1): low-pass filtered at **4.9 kHz**, digitized at **10k samples per second**. Linear prediction spectra were taken at hand-selected points in a syllable. Each waveform segment was **first differenced** (to reduce very low frequency noise), multiplied by a **Kaiser window (Beta=7.0)**, and analyzed with **11-pole** linear prediction. The **25.6 msec** window has an effective averaging duration of **about 10 msec**. The same window was used everywhere except the sustained frication of fricatives, where it was lengthened.
- Spectral samples were taken: 1) during the consonantal steady state (or at burst onset for a plosive); 2) at voicing onset (or early in the CV transition for voiced consonants); 3) shortly after the end of the CV transition.
- Formant frequencies were estimated from LP spectrum peaks. Formant motions were plotted every **10 msec** in voiced portions. Intensity and F0 were also tracked.
- Figure 11-2 shows F1/F2 trajectories of **sixteen** vowel nuclei averaged over all consonant environments for DHK. Most vowels are somewhat diphthongized (true diphthongs are drawn dashed). In this dialect the short vowels **IH, EH, AE, UH** end in a schwa-like offglide. These averages are the starting point for CV synthesis.
- Two conclusions about consonant rules before a vowel:
  1. Some consonants, especially obstruents, change markedly depending on whether the following vowel is a **front vowel**, a **back unrounded vowel**, or a **back rounded vowel**.

<!-- book p.111 -->

### Figure 11-1: Spectrum analysis of a speech waveform

- **Top:** a voiced speech waveform running from **t=180ms.** (left marker) to **t=280ms.** (right marker), showing roughly 7–8 glottal periods (sharp periodic spikes). A bell-shaped **analysis window** (Kaiser) is drawn over it, centred at **t=240ms.** (arrow above). The window is narrow compared with the 100-ms excerpt, spanning about two to three pitch periods.
- **Bottom:** amplitude spectrum. x-axis **Frequency (kHz)**, 0 to 5. y-axis **Amplitude (dB)**, 0 to 70 in steps of 10.
  - **Smooth upper curve (LP envelope):** starts at about 35–37 dB at 0 Hz. Peaks at about **0.7 kHz (~59 dB)**, **1.5 kHz (~64 dB)**, **2.5 kHz (~58 dB)** and **3.3–3.4 kHz (~58 dB)**, with valleys at about 1.0 kHz (~49 dB), 2.0 kHz (~49 dB) and 2.9 kHz (~52 dB). Above 3.5 kHz it falls steadily and flattens near **~26 dB at 5 kHz**. (Readings are approximate.)
  - **Jagged lower curve (DFT):** harmonic line spectrum, with harmonic peaks following the envelope about 5–10 dB below it and deep notches between harmonics (down to ~20–30 dB). It falls to about 10–15 dB near 5 kHz.

  2. Within each vowel set, the spectra tied to each consonant are surprisingly invariant, and formant transitions into the vowel follow a modified **"locus" equation**.

- Figure 11-3: average **plosive burst spectra** before each vowel. Each curve averages **six** tokens of a syllable from one talker. Burst spectra are similar within a vowel category even without level normalization. **GG** bursts differ strikingly across vowel sets. **DD** bursts change before rounded vowels. Voiceless plosives and fricatives show similar changes.

<!-- book p.112 -->

### Figure 11-2: First and second formant motions in English vowels

- x-axis **Second formant frequency**, labelled 800 to 2200 (ticks every 200). y-axis **First formant frequency**, 300 to 800 (ticks every 100). Annotations: "Back ↔ Front" across the top (back on the left), and "Low ↕ High" at the right (low at the top).
- Each vowel is an arrow from its onset to its offglide position. Solid arrows are monophthongs/short vowels with offglides; dashed arrows are true diphthongs. Approximate readings (F2, F1 in Hz, start → end of arrow; read from the scan, ±~30 Hz):

| Vowel | Arrow type | Approx. start (F2, F1) | Approx. end (F2, F1) | Notes |
|---|---|---|---|---|
| AA | point (dot) | (1220, 680) | — | drawn as a single dot labelled at ~700 |
| AE | solid | (1880, 610) | (1680, 640) | short arrow pointing left/up |
| EH | solid | (1690, 590) | (1850, 490) | label "EH" near ~1660/590 |
| IH | solid | (1640, 460) | (1830, 400) | |
| EY | solid | (1720, 480) | (2050, 330) | long arrow down-right toward IY region |
| IY | solid | (2050, 330) | (2080, 290) | short arrow, tail near "IY" label |
| AO | solid | (920, 600) | (1040, 630) | short arrow up-right |
| AH | dashed? | (1150, 560) | — | label near (1170, 590) |
| AW | dashed | (1180, 640) | (920, 410) | diphthong from near AA down-left |
| OW | solid | (1080, 530) | (880, 450) | arrow down-left |
| UH | solid | (1090, 450) | (1220, 520) | arrow up-right |
| ER | solid | (1270, 470) | (1330, 410) | arrow down-right |
| AY | dashed | (1230, 650) | (1900, 400) | from near AA down-right; label "AY" ~ (1520, 570) |
| OY | dashed | (950, 520) | (1820, 370) | label "OY" ~ (1360, 470) |
| UW | solid | (1250, 340) | (890, 310) | arrow pointing left |
| YU | dashed | (1850, 290) | (1050, 330) | arrow pointing left, label "YU" ~ (1560, 320) |

(These readings are approximate. For exact vowel targets use Table 11-1, which gives two rows, onset and offglide, for diphthongized vowels.)

- Figure 11-4: average frequencies of **F1, F2, F3 at voicing onset** after plosive release in syllables with **BB, DD, GG**. Values before **16** vowel nuclei are plotted (vertical) against the formant frequency in the early part of the vowel (horizontal). For **F1** the data fit a straight line well. But F1 at voicing onset (when F1 first becomes perceptible) is **as high as 500 Hz** before low vowels, because most of the fast F1 rise after release happens before voicing onset.
- If onset values fall on a straight line, a **locus theory** description fits: a locus equation with **two free parameters** predicts the onset formant value from the vowel target.
- The three vowel sets have articulatory motivation: anticipatory coarticulation of **front-back** and **rounded-unrounded** explains all the acoustic observations above.

<!-- book p.113 -->

### Figure 11-3: Linear prediction of plosive bursts before vowels

- A 3×3 grid. Columns: **[BB]**, **[DD]**, **[GG]**. Rows: **BEFORE FRONT VOWELS**, **BEFORE BACK UNROUNDED VOWELS**, **BEFORE BACK ROUNDED VOWELS**. Each panel: x-axis FREQ (kHz) 0–5, y-axis (dB) 0–60 (ticks 0, 20, 40, 60). Several overlaid curves per panel (one per vowel in the set).
- Visible shapes (approximate):
  - **BB:** flat, diffuse spectra, about 20–30 dB across 0–5 kHz with several small ripples. Similar in all three rows, slightly falling at high frequency.
  - **DD:** front and back-unrounded rows rise from ~15–20 dB at low frequency to a **high-frequency peak near 4 kHz (~40–45 dB)**. Back-rounded row: the peak moves lower, to a **sharp peak near 2.5 kHz (~45–50 dB)**.
  - **GG:** front row: rising spectrum with a broad peak near **3–3.5 kHz (~40 dB)**. Back unrounded: **two peaks near 1.5–2 kHz and ~3.5 kHz (~30–35 dB)**. Back rounded: a strong **low peak near 1–1.5 kHz (~40 dB)** with a secondary peak near 3–3.5 kHz.

### 11.2.2 Synthesis strategy

- Figure 11-5 shows a strategy for predicting formant motion in CV syllables, using the syllable **go**:
  - First, define the **vowel** with straight-line segments (upper panel).
  - Then impose the formant values tied to the **consonant** and the **CV transition**, again with straight-line segments plus a **locus equation** for the formant value at the CV boundary (lower panel).
- Other parameters (formant bandwidths, formant amplitudes in frication spectra, etc.) were set by repeated trial-and-error comparison of synthetic and natural LP spectra. Small errors in frication spectra, frication level, or the time course of intensity buildup raised identification errors, so this tuning matters. The resulting values are tied to a par-

<!-- book p.114 -->

  ticular software synthesizer (Klatt, 1980; Chapter 12).

### Figure 11-4: Frequency of the lowest three formants measured at voicing onset for syllables involving BB, DD, and GG

- 3×3 grid of scatter plots with fitted lines. Columns are BB, DD, GG (left to right, per caption order). Rows:
  - **Row 1:** y = "F1 at voicing onset (kHz)" (.2, .4, .6); x = "F1 target of vowel (kHz)" (.2, .4, .6, .8). All three columns show one rising straight line through points from about (0.25, 0.25) to (0.75, 0.55–0.6), slope roughly 0.6–0.7.
  - **Row 2:** y = "F2 at voicing onset (kHz)" (.8, 1.2, 1.6, 2.0); x = "F2 target of vowel (kHz)" (.8, 1.2, 1.6, 2.0).
    - BB: two separate line segments: a shallow one for back vowels (x ≈ 0.9–1.3, y ≈ 0.9–1.1) and a steep one for front vowels (x ≈ 1.6–2.0, y ≈ 1.4–1.85).
    - DD: shallow segment for back vowels (x ≈ 0.9–1.3, y ≈ 1.45–1.6) and a separate rising segment for front vowels (x ≈ 1.55–1.85, y ≈ 1.5–1.75).
    - GG: rising segment for back vowels (x ≈ 0.85–1.25, y ≈ 1.05–1.4), a cluster of points near (1.25, 1.55–1.65), and a nearly flat high segment for front vowels (x ≈ 1.55–2.0, y ≈ 1.9–2.0).
  - **Row 3:** y = "F3 at voicing onset (kHz)" (1.5, 2.0, 2.5, 3.0); x = "F3 target of vowel (kHz)" (1.5, 2.0, 2.5, 3.0).
    - BB: rising line from about (1.6, 1.7) to (2.4, 2.35), then flatter to (2.9, 2.45).
    - DD: nearly flat line from about (1.6, 2.3) to (2.9, 2.6).
    - GG: a shallow line from about (1.6, 1.85) to (2.6, 2.1), plus a steep rising line from about (2.45, 2.35) to (3.0, 3.0).
- Point markers (dots, crosses, asterisks) presumably mark vowel categories. No legend is printed.

### 11.2.3 Intelligibility evaluation

- **336** different CVt syllables were synthesized in random order and played to **five** phonetically trained listeners, who transcribed consonants and vowels.
- Vowel identification **99 percent**. Consonant identification **95 percent**.
- Planned work (paraphrased): improve consonants and extend to clusters and

<!-- book p.115 -->

  postvocalic allophones.

### Figure 11-5: Synthesis strategy for a CV syllable

Two panels. Each has y-axis **Frequency (kHz)** 0–2 and x-axis **Time (ms)** 0–800.

- **Top: "Second formant transition for OW"**
  - F2 = **1100** Hz held level from **200** to **230** ms.
  - Linear fall from 1100 at 230 ms to **850** Hz at **380** ms.
  - Level at 850 Hz from **380** to **460** ms (end of vowel).
  - Dotted vertical guide lines at 200, 230, 380, 460 ms.
- **Bottom: "Second formant transition for GG OW"**
  - Label: **1448=1700+0.42(F2 -1700)**. This is the locus equation for the F2 value at the CV boundary, with F2 = vowel target 1100: 1700 + 0.42 × (1100 − 1700) = 1448.
  - A level line from **0** to **200** ms at about 1.45 kHz (the consonant/locus value, 1448).
  - From 200 ms a straight-line transition falls from 1448 to meet the vowel trajectory at **280** ms.
  - A dotted line marked **1100** shows the underlying vowel contour from the top panel (starting at 200 ms, falling toward 850). The transition joins it at 280 ms.
  - The vowel trajectory continues down to **850** at **380** ms and stays level to **460** ms.
  - Dotted guides at 200, 280, 380, 460 ms.

### Further discussion (paraphrased)

- Two open questions: 1) do the vowel-category generalizations and locus-equation form need changing, or only the numbers; 2) is spectral matching enough to optimize intelligibility? The authors expect the generalizations to hold. Each perceptual problem with a CV syllable or class turned up as a sizeable spectral mismatch against DHK's average spectra, and fixing the

<!-- book p.116 -->

  mismatch clearly improved intelligibility. At least one more iteration is needed. Within the synthesizer's limits, LP spectral matching is adequate.

## 11.3 General rules for the synthesis of phonetic sequences

- MITalk's PHONET is not the limited CV algorithm above. It is modelled on the **Fortran synthesis-by-rule program of Klatt (1976a)**, with changed program structure and target tables, to bring in some of the new CV rules and to simplify the rule structure.
- General procedure for drawing control parameter values:
  1. Draw the target value for the first segment.
  2. Draw the target value for the next segment.
  3. Smooth the boundary between the segments using one of the templates in Figure 11-6 (**DISCON does no smoothing**).
  4. Go to step 2 unless there are no more segments.
- Each parameter's transition between targets is either discontinuous or smooth. The **boundary value** and the **transition duration in each direction** from the logical phoneme boundary are computed by rules that use the **manner features** of the segments involved.

### 11.3.1 Vowels

- Parameters normally varied for an isolated vowel: amplitude of voicing **AV**; fundamental frequency **F0**; formant frequencies **F1, F2, F3**; bandwidths **B1, B2, B3**. F4 and F5 could be varied for spectral detail but are not needed for high intelligibility.
- For a natural breathy vowel ending, aspiration amplitude **AH** and quasi-sinusoidal voicing amplitude **AVS** are turned on.
- Table 11-1 gives suggested targets that distinguish English vowels. Formant and bandwidth targets came from trial-and-error spectral matching to many CV syllables from DHK. Bandwidths are often larger than the closed-glottis values of Fujimura and Lindqvist (1971), because the Table 11-1 bandwidths also account for changes to ob-

<!-- book p.117 -->

### Figure 11-6: Templates for smoothing adjacent phonetic segment targets

Four panels, each with y-axis "Value" and x-axis time marks **t0**, **t1**, **t2** (dotted verticals). t0–t1 is the first segment, t1–t2 is the second, **t1 is the segment boundary**. In every panel the first segment's target is higher than the second's.

- **a) Transition type: DISCON.** The first target is level from t0 to t1. At t1 the value **steps** straight to the second target, which is level to t2. No smoothing.
- **b) Transition type: SETSMO.** The first target is level from t0 until **t1 − Tcb**. From there a straight line runs to the **boundary value at t1**, labelled **BVf** (left of the point) and **BVb** (right of the point), which is a single shared point here. From t1 another straight line runs down to the second target, reached at **t1 + Tcf**, and the value is then level to t2. **Tcb** is the interval just before t1 and **Tcf** the interval just after t1. As drawn, the slope after t1 is steeper than before it, so the two pieces form a broken line through the boundary value.
- **c) Transition type: SMODIS.** The first target is level until **t1 − Tcb**, then a straight line runs to boundary value **BVb** at t1. At t1 the value **jumps** to the second target, which is level to t2. (Smooth into the boundary, discontinuous after.)
- **d) Transition type: DISSMO.** The first target is level until t1. At t1 the value **jumps** to boundary value **BVf**, then runs in a straight line to the second target at **t1 + Tcf**, and is level to t2. (Discontinuous at the boundary, smooth after.)

(Label reading: in panels b–d, "BVb" sits on the backward/left-segment side of the transition and "BVf" on the forward side. The text gives no formal definitions of BVf/BVb/Tcb/Tcf.)

<!-- book p.118 -->

- (continued) served formant amplitudes caused by glottal losses and voicing-source spectrum irregularities.
- **Where Table 11-1 gives two values** (two rows), the vowel is diphthongized or has a schwa-like offglide in DHK's speech. Steady-state and transition durations of diphthongized vowels depend on total vowel duration and differ for each vowel.
- Figure 11-7 shows how a diphthongized vowel is synthesized. Each constant in the figure is stored in tables for **all diphthongized vowels, including those with schwa offglides**.

### 11.3.2 Consonants

- Consonant classes need more parameters. Table 11-2 gives targets for parts of English consonants: frication spectra of fricatives, burst spectra of plosives, nasal murmurs, and steady portions of sonorants.
- **Sonorants WW, YY, RR, LL** behave like vowels and use the same parameter set. Table 11-2 formant values for **prevocalic RR and LL** depend somewhat on the following vowel. **AV** for a prevocalic sonorant should be **about 10 dB less** than in the vowel.
- **HH**: take formant frequencies and bandwidths from the **following vowel**, raise **B1 to about 300 Hz**, and replace voicing with **aspiration**.
- **Fricatives** in Table 11-2: voiceless (**AF=60, AV=0, AVS=0**) and voiced (**AF=50, AV=47, AVS=47**).
- The formants excited by frication noise are set by amplitude controls **A2, A3, A4, A5, A6, AB**. The parallel second-formant amplitude **A2** is **zero for all these consonants before front vowels**. Before nonfront vowels, F2 is a front-cavity resonance for **velars** and A2 is excited.
- The **F2 and F3** values hold during the fricative and also serve as **"loci"** for CV formant transitions **before front vowels**. These are **virtual loci**: the formant values at onset of glottal excitation lie between the locus and the vowel target, and how far depends on formant-cavity affiliations.
- The frication spectra in the table are accurate **only before front vowels** for DHK. Before back and rounded vowels the fricative spectra change systematically through anticipatory coarticulation.

<!-- book p.119 -->

### Table 11-1: Parameter values for the synthesis of selected vowels

All values in Hz. Where a vowel has two rows, the first row is the onset target and the second is the offglide/second target (see 11.3.1 and Figure 11-7).

| Vowel | Row | F1 | F2 | F3 | B1 | B2 | B3 |
|---|---|---|---|---|---|---|---|
| IY | 1 | 310 | 2020 | 2960 | 45 | 200 | 400 |
| IY | 2 | 290 | 2070 | 2960 | 60 | 200 | 400 |
| IH | 1 | 400 | 1800 | 2570 | 50 | 100 | 140 |
| IH | 2 | 470 | 1600 | 2600 | 50 | 100 | 140 |
| EY | 1 | 480 | 1720 | 2520 | 70 | 100 | 200 |
| EY | 2 | 330 | 2020 | 2600 | 55 | 100 | 200 |
| EH | 1 | 530 | 1680 | 2500 | 60 | 90 | 200 |
| EH | 2 | 620 | 1530 | 2530 | 60 | 90 | 200 |
| AE | 1 | 620 | 1660 | 2430 | 70 | 150 | 320 |
| AE | 2 | 650 | 1490 | 2470 | 70 | 100 | 320 |
| AA | 1 | 700 | 1220 | 2600 | 130 | 70 | 160 |
| AO | 1 | 600 | 990 | 2570 | 90 | 100 | 80 |
| AO | 2 | 630 | 1040 | 2600 | 90 | 100 | 80 |
| AH | 1 | 620 | 1220 | 2550 | 80 | 50 | 140 |
| OW | 1 | 540 | 1100 | 2300 | 80 | 70 | 70 |
| OW | 2 | 450 | 900 | 2300 | 80 | 70 | 70 |
| UH | 1 | 450 | 1100 | 2350 | 80 | 100 | 80 |
| UH | 2 | 500 | 1180 | 2390 | 80 | 100 | 80 |
| UW | 1 | 350 | 1250 | 2200 | 65 | 110 | 140 |
| UW | 2 | 320 | 900 | 2200 | 65 | 110 | 140 |
| ER | 1 | 470 | 1270 | 1540 | 100 | 60 | 110 |
| ER | 2 | 420 | 1310 | 1540 | 100 | 60 | 110 |
| AY | 1 | 660 | 1200 | 2550 | 100 | 70 | 200 |
| AY | 2 | 400 | 1880 | 2500 | 70 | 100 | 200 |
| AW | 1 | 640 | 1230 | 2550 | 80 | 70 | 140 |
| AW | 2 | 420 | 940 | 2350 | 80 | 70 | 80 |
| OY | 1 | 550 | 960 | 2400 | 80 | 50 | 130 |
| OY | 2 | 360 | 1820 | 2450 | 60 | 50 | 160 |

(AA and AH have a single row. Note that Figure 11-5 uses OW F2 = 1100 → 850, while Table 11-1 gives OW F2 = 1100 → 900.)

<!-- book p.120 -->

### Figure 11-7: Constants used to specify the inherent formant and durational characteristics of a sonorant

- y-axis "Frequency" (no numbers), x-axis "Time" with marks **0**, **TDMID**, **INHDUR**.
- Three formant tracks:
  - **F3:** level line from **VTAR3** at time 0 to **DTAR3** at INHDUR (drawn with no change in this example).
  - **F2:** level at **VTAR2** from 0 until **TDMID − TCDIPH**, then a smooth S-shaped (ogive) rise to **DTAR2**, reached at **TDMID + TCDIPH**, then level to INHDUR.
  - **F1:** level at **VTAR1** from 0 until **TDMID − TCDIPH**, then a smooth S-shaped fall to **DTAR1** at **TDMID + TCDIPH**, then level to INHDUR.
- Dotted verticals mark 0, TDMID − TCDIPH, TDMID, TDMID + TCDIPH, and INHDUR. The two spans on either side of TDMID are each labelled **TCDIPH**, so the transition is centred on TDMID with half-width TCDIPH.
- VTARn = first (vowel) targets, DTARn = second (diphthong/offglide) targets. These correspond to the two rows of Table 11-1.

### Consonants (continued)

- Voiced and voiceless fricatives also differ in that, with the glottis open, **F1 is higher and B1 is larger**.
- **Affricate** parameters in Table 11-2 describe the **fricative portion**. **Plosive** parameters describe the **brief frication burst** at release. Their formant values again act as loci for formant positions at voicing onset.
- **Nasal murmur** parameters include nasal pole and zero frequencies **FNP** and **FNZ**. These are used mainly to approximate **vowel nasalization at nasal release**, by splitting F1 into a pole-zero-pole complex. The nasal-murmur details described by Fujimura (1962) are approximated with **formant bandwidth adjustments**, not pole-zero insertion. The reason given: simulating both the higher-frequency murmur pole-zero details and vowel nasalization would require moving the nasal pole and zero very fast at release, which makes an audible click. Vowel nasalization was found perceptually more important.
- **Nasalized vowel:** raise **F1 by about 100 Hz**, and set the **nasal zero frequency = average of this new F1 and 270 Hz** (270 Hz is the frequency of the **fixed nasal pole**).
- Tables 11-1 and 11-2 **omit** steady-state targets for: unstressed allophones, postvocalic allophones, flaps, glottal stops, voicebars, and

<!-- book p.121 -->

### Table 11-2: Parameter values for the synthesis of selected components of English consonants before front vowels

Frequencies and bandwidths in Hz. A2–A6 and AB in dB.

**Sonorant** (printed columns: F1 F2 F3 B1 B2 … B3; B3 is printed far right, under the column where B3 sits in the fricative block)

| Seg | F1 | F2 | F3 | B1 | B2 | B3 |
|---|---|---|---|---|---|---|
| WW | 290 | 610 | 2150 | 50 | 80 | 60 |
| YY | 260 | 2070 | 3020 | 40 | 250 | 500 |
| RR | 310 | 1060 | 1380 | 70 | 100 | 120 |
| LL | 310 | 1050 | 2880 | 50 | 100 | 280 |

**Fricative**

| Seg | F1 | F2 | F3 | B1 | B2 | B3 | A2 | A3 | A4 | A5 | A6 | AB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| FF | 340 | 1100 | 2080 | 200 | 120 | 150 | 0 | 0 | 0 | 0 | 0 | 57 |
| VV | 220 | 1100 | 2080 | 60 | 90 | 120 | 0 | 0 | 0 | 0 | 0 | 57 |
| TH | 320 | 1290 | 2540 | 200 | 90 | 200 | 0 | 0 | 0 | 0 | 28 | 48 |
| DH | 270 | 1290 | 2540 | 60 | 80 | 170 | 0 | 0 | 0 | 0 | 28 | 48 |
| SS | 320 | 1390 | 2530 | 200 | 80 | 200 | 0 | 0 | 0 | 0 | 52 | 0 |
| ZZ | 240 | 1390 | 2530 | 70 | 60 | 180 | 0 | 0 | 0 | 0 | 52 | 0 |
| SH | 300 | 1840 | 2750 | 200 | 100 | 300 | 57 | 48 | 48 | 46 | 0 | 0 |

**Affricate**

| Seg | F1 | F2 | F3 | B1 | B2 | B3 | A2 | A3 | A4 | A5 | A6 | AB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| CH | 350 | 1800 | 2820 | 200 | 90 | 300 | 0 | 44 | 60 | 53 | 53 | 0 |
| JH | 260 | 1800 | 2820 | 60 | 80 | 270 | 0 | 44 | 60 | 53 | 53 | 0 |

**Plosive**

| Seg | F1 | F2 | F3 | B1 | B2 | B3 | A2 | A3 | A4 | A5 | A6 | AB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PP | 400 | 1100 | 2150 | 300 | 150 | 220 | 0 | 0 | 0 | 0 | 0 | 63 |
| BB | 200 | 1100 | 2150 | 60 | 110 | 130 | 0 | 0 | 0 | 0 | 0 | 63 |
| TT | 400 | 1600 | 2600 | 300 | 120 | 250 | 0 | 30 | 45 | 57 | 63 | 0 |
| DD | 200 | 1600 | 2600 | 60 | 100 | 170 | 0 | 47 | 60 | 62 | 60 | 0 |
| KK | 300 | 1990 | 2850 | 250 | 160 | 330 | 0 | 53 | 43 | 45 | 45 | 0 |
| GG | 200 | 1990 | 2850 | 60 | 150 | 280 | 0 | 53 | 43 | 45 | 45 | 0 |

**Nasal** (printed columns: FNP FNZ F1 F2 F3 B1 B2 … B3)

| Seg | FNP | FNZ | F1 | F2 | F3 | B1 | B2 | B3 |
|---|---|---|---|---|---|---|---|---|
| MM | 270 | 450 | 480 | 1270 | 2130 | 40 | 200 | 200 |
| NN | 270 | 450 | 480 | 1340 | 2470 | 40 | 300 | 300 |

(The affricate is printed "JH" here but "JJ" in Table 9-1.)

<!-- book p.122 -->

- (continued from p.120) …consonant clusters. Describing even the static properties of these segments is outside the chapter's scope.

### 11.3.3 Structure of the output parameter file

One complete set of control parameter values per **5 msec** of speech. The varied parameters are in Table 11-3.

### Table 11-3: Variable control parameters specified in PHONET

| N | Symbol | Name |
|---|---|---|
| 1 | AV | amplitude of voicing in dB |
| 2 | AF | amplitude of frication in dB |
| 3 | AH | amplitude of aspiration in dB |
| 4 | AVS | amplitude of sinusoidal voicing in dB |
| 5 | F0 | voicing fundamental frequency in Hz |
| 6 | F1 | first formant frequency in Hz |
| 7 | F2 | second formant frequency in Hz |
| 8 | F3 | third formant frequency in Hz |
| 9 | F4 | fourth formant frequency in Hz |
| 10 | FNZ | nasal zero frequency in Hz |
| 11 | B1 | first formant bandwidth in Hz |
| 12 | B2 | second formant bandwidth in Hz |
| 13 | B3 | third formant bandwidth in Hz |
| 14 | A2 | second parallel formant amplitude in dB |
| 15 | A3 | third parallel formant amplitude in dB |
| 16 | A4 | fourth parallel formant amplitude in dB |
| 17 | A5 | fifth parallel formant amplitude in dB |
| 18 | A6 | sixth parallel formant amplitude in dB |
| 19 | AB | bypass path amplitude in dB |
| 20 | — | not currently used |

(FNP is in Table 11-2 but not among the 20 variable parameters. Per p.120 the nasal pole is fixed at 270 Hz.)

## 11.4 Summary

PHONET differs from other formant synthesis-by-rule programs (e.g. Votrax, Kurzweil, Holmes, Mattingly, Rabiner, Hertz) mainly in **how many context-dependent rules** it has for modelling the spectral detail of phonetic transitions. **A complete description of the rules is in Appendix C.**

<!-- book p.123 (not transcribed) -->
<!-- File book_p122.png is printed p.123, the first page of Chapter 12 "The Klatt formant synthesizer" (CWTRAN/COEWAV, Figure 12-1). It is outside this file's scope. Facts on it relevant to Ch. 11: 39 synthesizer control parameters in total, of which only 20 vary over time. Parameters are given every 5 msec. Output is 10,000 samples/s at 12 bits/sample through a 4.9 kHz low-pass filter. -->
