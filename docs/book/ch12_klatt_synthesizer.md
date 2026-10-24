# Chapter 12 — The Klatt formant synthesizer (technical spec notes)

Source: Allen, Hunnicutt & Klatt, *From Text to Speech: The MITalk System* (1987), Ch. 12, printed pp. 123–150.
Scan files: `book_fix/printed_p123.png` = printed p.123; `book_pNNN.png` = printed p.NNN+1 (so `book_p123.png` …
`book_p149.png` = printed pp. 124–150). `book_p150.png` = printed p.151, the first page of Ch. 13 (noted, not
transcribed). All `<!-- book p.NNN -->` markers below are **printed** page numbers.

**Form of this document.** Equations, Table 12-1, figure captions, numeric values and parameter names are
reproduced exactly as printed. The running prose is **not** a verbatim copy of the book: each paragraph is
restated as condensed technical notes that keep every quantitative fact, rule, condition and cross-reference
the paragraph contains (nothing technical is intentionally dropped or "fixed"). Printing anomalies are marked
`[sic?]`; illegible items `[?]`. Notes of mine are in *[Note: …]*.

---

<!-- book p.123 -->

# 12 The Klatt formant synthesizer

## 12.1 **Overview**

- The last two MITalk modules, **CWTRAN** and **COEWAV**, simulate a formant synthesizer (Klatt, 1980). Figure 12-1
  shows their interface to the speech-producing hardware.
- Control parameter data are specified **every 5 msec** by the rules of the phonetic component (Chapter 11).
- **39 control parameters** specify the software synthesizer, of which **only 20 are varied** as a function of time.
  *[sic? — Section 12.1.9 (p.130) says "as many as 22 of the 39", and Table 12-1 marks 22 rows V.]*

**Figure 12-1: Interface between synthesizer software and hardware**

Figure content, left to right:
`CONTROL PARAMETERS (1 SET/5 ms)` (dotted arrow up into) box **CWTRAN / COEWAV** → box **D/A CONVERTER** (dotted
arrow up into it labelled `WAVEFORM SAMPLES (10,000/s 12 BITS/SAMPLE)`, i.e. the CWTRAN/COEWAV output) → box
**4.9 kHz LOW-PASS FILTER** → box **LOUDSPEAKER**. A plain input arrow also enters CWTRAN/COEWAV from the left.

(continuation of 12.1)
- The synthesizer is two logically distinct modules:
  - **CWTRAN** takes control parameter data — formant frequencies, formant bandwidths and fundamental frequency (all
    in Hz), and source amplitudes and the amplitude of each parallel formant (in dB) — and derives a set of
    **difference-equation coefficients for each digital formant resonator** and a set of **linear amplitude
    coefficients**.
  - **COEWAV** takes this coefficient and amplitude array for one **5 msec frame** and computes the next **5 msec
    chunk of waveform**.
- Large speed gains come from implementing COEWAV as a hardware digital filter; a **TTL implementation** has been
  built (Miranker, 1978).
- Whether the software subroutine or the "VTM" *[term not defined on this page; presumably the hardware vocal tract
  model]* is used, output samples go through a digital-to-analog converter, analog low-pass filter, and loudspeaker.

<!-- book p.124 -->

### 12.1.1 *Software simulation vs. hardware construction*

- Software simulation is preferred over special-purpose analog hardware: no repeated calibration, stable, and
  signal-to-noise ratio (quantization noise, for a digital simulation) can be made arbitrarily large.
- Configuration is easy to change; e.g., women's and children's voices can be produced by modifying the voicing
  source and the cascade vocal-tract configuration.
- Graphic terminals can display control-parameter data or portions of the output waveform; short-time spectra can
  be computed for detailed spectral comparison of natural vs. synthetic waveforms.

### 12.1.2 *Formant synthesis vs. articulatory synthesis*

- Two broad categories: (1) articulatory synthesizers, modelling articulator motion and resulting volume-velocity /
  pressure distributions in lungs, larynx, vocal and nasal tracts (Flanagan *et al.*, 1975); (2) formant
  synthesizers, approximating the waveform with a simpler set of rules in the acoustic domain.
- Chapter covers formant models only: articulatory models need several orders of magnitude more computation and
  cannot be specified precisely enough for trial-and-error rule optimization against natural speech.
- Design is based on the acoustic theory of speech production of Fant (1960), summarized in Figure 12-2.
  One or more sound sources are activated by lung-pressure build-up. Each source is characterized in the frequency
  domain by a source spectrum $S(f)$, $f$ = frequency in Hz. Each source excites the vocal tract, a resonating
  system analogous to an organ pipe.
- The vocal tract is linear, so it has a transfer function $T(f)$ = ratio of lip-plus-nose volume velocity $U(f)$
  to source input $S(f)$. The radiated sound-pressure spectrum $P(f)$ at a distance from the lips is related to
  $U(f)$ by a radiation characteristic $R(f)$ (directional propagation from the head).
- Each relationship can also be recast in the time (waveform) domain [continues p.125].

<!-- book p.125 -->

**Figure 12-2: Components of the output spectrum of a speech sound**

Figure content: equation at top, printed as `P(f) = S(f)*T(f)*F(f)` *[sic? — the third factor is printed "F(f)";
the blocks and text call the radiation characteristic R(f), i.e. P(f) = S(f)·T(f)·R(f)]*.
Left-to-right chain of three boxes:
1. Box "SOUND SOURCE / VOICING / ASPIRATION / FRICATION" → arrow labelled "SOURCE VOLUME VELOCITY S(f)" →
2. Box "VOCAL TRACT TRANSFER FUNCTION T(f)" → arrow labelled "LIP VOLUME VELOCITY U(f)" →
3. Box "RADIATION CHARACTERISTIC R(f)" → output arrow labelled "RADIATED SOUND PRESSURE P(f)".

(continuation of 12.1.2)
- In the computer the waveform is generated in the time domain. The synthesizer contains components simulating
  several kinds of sound source (Section 12.1.10), the vocal-tract transfer function (Figure 12-3), and radiation
  from the head (Figure 12-14).

### 12.1.3 *Cascade vs. parallel*

- Many hardware/software synthesizers exist (Dudley *et al.*, 1939; Cooper *et al.*, 1951; Lawrence, 1953;
  Stevens *et al.*, 1955; Fant, 1959; Fant and Martony, 1962; Flanagan *et al.*, 1962; Holmes *et al.*, 1964;
  Epstein, 1965; Tomlinson, 1966; Scott *et al.*, 1966; Liljencrants, 1968; Rabiner *et al.*, 1971a; Klatt, 1972;
  Holmes, 1973), using different configurations toward high-quality approximation of human speech. Some have
  stability/calibration problems; some cannot produce a good voiced fricative; many are well designed. Two general
  configurations are common among the best.
- **Parallel formant synthesizer** (e.g. Lawrence, 1953; Holmes, 1973): formant resonators connected in parallel
  (lower part of Figure 12-3); each resonator is preceded by an amplitude control setting the relative amplitude of
  its spectral peak, for both voiced and voiceless sounds.
- **Cascade formant synthesizer** (e.g. Fant, 1959; Klatt, 1972): sonorants synthesized with formant resonators in
  cascade (upper part of Figure 12-3).
- Advantage of cascade: relative formant-peak amplitudes for vowels come out right (Fant, 1956) without individual
  amplitude controls per formant [continues p.126].

<!-- book p.126 -->

**Figure 12-3: Parallel and cascade simulation of the vocal tract transfer function**

Figure content, two diagrams:
- **CASCADE** (top): `INPUT → R5 → R4 → R3 → R2 → R1 → OUTPUT` (five boxes in series; order as drawn is R5 first,
  R1 last).
- **PARALLEL** (bottom): INPUT line splits into five branches; each branch is a circle (amplitude gain) followed by
  a box (resonator): `A1 → R1`, `A2 → R2`, `A3 → R3`, `A4 → R4`, `A5 → R5`. All five resonator outputs enter one
  summing node "+" → OUTPUT. All branches drawn with the same (+) sign.

(continuation of 12.1.3)
- Disadvantage of cascade: a parallel configuration is still needed for fricatives and plosive bursts, because the
  vocal-tract transfer function cannot be modelled adequately by a cascade when the source is above the larynx;
  hence cascade synthesizers are generally more complex overall.
- Second advantage of cascade: it is a more accurate model of the vocal-tract transfer function for nonnasal
  sonorants (Flanagan, 1957). It will be shown that some vowels' transfer functions cannot be modelled well by a
  parallel synthesizer. A parallel synthesizer is useful for stimuli that violate normal inter-formant amplitude
  relationships, or e.g. single-formant patterns.
- The described software is programmed for normal use as a **hybrid cascade/parallel synthesizer (Figure 12-4a)**,
  or alternatively for special-purpose use [continues p.127] …

<!-- book p.127 -->

… as a **strictly parallel synthesizer (Figure 12-4b)**.
- The configuration must be chosen beforehand; it is selected by the state of a single switch. The program skips
  computations for resonators that are not used.
- The synthesizer has been adjusted so that, as far as possible, it produces about the same output waveform in
  cascade/parallel and all-parallel configurations.

**Figure 12-4: Cascade/parallel configurations supported by MITalk**

Figure content, two block diagrams (top = (a) cascade/parallel; bottom = (b) all-parallel; the sub-labels a/b are
not printed in the figure, only implied by the text):
- **Top (12-4a):** "VOICING SOURCE" and "ASPIRATION SOURCE" → summing node "+" → "LARYNGEAL TRANSFER FUNCTION
  (CASCADE)". "FRICATION SOURCE" → "FRICATION TRANSFER FUNCTION (PARALLEL)". Outputs of the cascade box and the
  parallel box → second summing node "+" → "RADIATION CHARACTERISTIC" → "OUTPUT SPEECH".
- **Bottom (12-4b):** "VOICING SOURCE", "ASPIRATION SOURCE" and "FRICATION SOURCE" → one summing node "+" →
  "VOCAL TRACT TRANSFER FUNCTION (PARALLEL)" → "RADIATION CHARACTERISTIC" → "OUTPUT SPEECH".

### 12.1.4 *Waveform sampling rate*

- Most speech energy lies between about 80 and 8000 Hz (Dunn and White, 1940).
- Band-pass intelligibility tests: intelligibility not measurably changed if energy above about 5000 Hz is removed
  (French and Steinberg, 1947); such low-passed speech sounds natural.
- **Sampling rate chosen: 10,000 samples per second.**

<!-- book p.128 -->

### 12.1.5 *Parameter update rate*

- **Control parameters updated every 5 msec**; fast enough for the most rapid formant transitions and brief
  plosive bursts. Program can be changed to update every 10 msec with relatively little quality loss.

### 12.1.6 *Digital resonators*

- Basic building block: digital resonator (Figure 12-5), specified by resonant (formant) frequency $F$ and
  bandwidth $BW$ (in Figure 12-5: 1000 Hz and 50 Hz).
- Output samples $y(nT)$ from input $x(nT)$:

$$y(nT) = Ax(nT) + By(nT-T) + Cy(nT-2T) \qquad (1)$$

  where $y(nT-T)$, $y(nT-2T)$ are the previous two output samples. $A, B, C$ are related to $F$ and $BW$ by the
  impulse-invariant transformation (Gold and Rabiner, 1968):

$$
\begin{aligned}
C &= -e^{2\pi BWT} \quad\text{[sic? — printed exponent has no minus sign; cf. } B\text{, which has } e^{-\pi BWT}\text{]}\\
B &= 2e^{-\pi BWT}\cos(2\pi FT) \qquad (2)\\
A &= 1 - B - C
\end{aligned}
$$

  Printed form (plain text): `C=-e^(2πBWT)`, `B=2e^(-πBWT) cos(2πfT)` *(lower-case f printed in the cos argument)*,
  `A=1-B-C`.
- $T$ = reciprocal of sampling rate = **0.0001 s** "in the present 5-kHz simulation" *[sic? — T = 0.0001 s
  corresponds to a 10-kHz sampling rate, i.e. 5-kHz bandwidth]*.
- $F$ and $BW$ updated every 5 msec, so the difference-equation coefficients change in small discrete steps every
  5 msec. Large sudden coefficient changes can cause clicks/burps; acoustic theory says formant frequencies always
  change slowly/continuously relative to the 5-msec interval.
- A digital resonator is a second-order difference equation; sampled frequency response:

$$T(f) = \frac{A}{1 - Bz^{-1} - Cz^{-2}} \qquad (3)$$

  where $z = e^{j2\pi fT}$, $j=\sqrt{-1}$, $f$ in Hz from 0 to 5000 Hz. Its sampled impulse response equals that of
  the corresponding analog resonator at times $nT$ (Gold and Rabiner, 1968), but the analog and digital frequency
  responses are not identical (Figure 12-5).

<!-- book p.129 -->

**Figure 12-5: Block diagram and frequency response of a digital resonator**

Figure content:
- **Block diagram** (dashed box titled "DIGITAL RESONATOR"): "INPUT SEQUENCE" → gain circle **A** → summing node
  "+" → "OUTPUT SEQUENCE". The output of "+" also feeds a chain `UNIT DELAY → UNIT DELAY`. Output of the first
  unit delay (i.e. y(nT−T)) → gain circle **B** → back into "+". Output of the second unit delay (y(nT−2T)) → gain
  circle **C** → back into "+". (Implements Eq. 1.)
- **Plot:** y-axis "|T(f)| (dB)", ticks −40, −20, 0, 20; x-axis "Frequency (kHz)", 0–5. Single resonance peak at
  F (marked with arrow, at 1 kHz), peak level a little above +20 dB (≈ +26 dB *[Note: theoretical for
  F=1000, BW=50 at 10 kHz]*); "3dB" marked below the peak and "BW" marked as the width between the −3 dB points.
  Curve starts at ≈ 0 dB at f=0 (unity dc gain). Above ~3 kHz two curves diverge: solid "Digital" flattens at about
  −20 dB toward 5 kHz; dotted "Analog" keeps falling slightly below the digital curve (≈ −22 to −23 dB at 5 kHz).

### 12.1.7 *Digital antiresonator*

- An antiresonance (antiformant, transfer-function zero pair) is obtained by slight modification of the resonator
  equations. Its frequency response is the mirror image of Figure 12-5 (dB → −dB).
- One antiresonator shapes the voicing-source spectrum; another simulates nasalization in the cascade model.
- Output $y(nT)$ of an antiformant resonator from input $x(nT)$ [equation on p.130]:

<!-- book p.130 -->

$$y(nT) = A'x(nT) + B'x(nT-T) + C'x(nT-2T) \qquad (4)$$

  where $x(nT-T)$, $x(nT-2T)$ are the previous two *input* samples and

$$
\begin{aligned}
A' &= 1/A\\
B' &= -B/A \qquad (5)\\
C' &= -C/A
\end{aligned}
$$

  with $A, B, C$ obtained by inserting the antiresonance centre frequency $F$ and bandwidth $BW$ into Equation 2.

### 12.1.8 *Low-pass resonator*

- Special case: resonator frequency $F$ set to zero → low-pass filter with nominal skirt of **−12 dB/octave** and
  3-dB-down break frequency **= BW/2**.
- The voicing source contains digital resonator **RGP** used as a low-pass filter, turning a glottal impulse into a
  pulse with waveform/spectrum like normal voicing.
- A second resonator **RGS** low-pass filters the normal voicing waveform to give the quasi-sinusoidal glottal
  waveform seen during the closure of an intervocalic voiced plosive.

### 12.1.9 *Synthesizer block diagram*

- Block diagram: Figure 12-6. **39 control parameters**; names and ranges in Table 12-1.
- As many as **22 of the 39** are varied to match an arbitrary English utterance. Constant parameters have values
  appropriate for a particular male voice; they would need slight adjustment for other male or female talkers.
- The variable-parameter list is long by design: emphasis is on strategies for high-quality synthesis, not on
  minimizing control information.

### 12.1.10 *Sources of sound*

- Two kinds of source (Stevens and Klatt, 1974): (1) quasi-periodic vibration of a structure, usually the vocal
  folds ("voicing"; lip/tongue-tip/uvula vibration occurs in some languages but not English); (2) turbulence noise
  [continues p.131] …

<!-- book p.131 -->

**Figure 12-6: Block diagram of the cascade/parallel formant synthesizer**

Labels under the lower half: "PARALLEL TRANSFER FUNCTION"; beside the right-hand column: "CASCADE TRANSFER
FUNCTION". Circles = amplitude (gain) controls; boxes = resonators/filters; "+" = summing nodes.

Every block and connection, as drawn:

1. **Voicing source.**
   - `F0` → arrow up into box **IMPULSE GEN.** The same F0 line also runs right and down to the right-hand input of
     the modulator **MOD.** (see 3).
   - `IMPULSE GEN. → RGP` (box). Output of RGP splits into two branches:
     - upper: gain circle **AV** → box **RGZ** → summing node "+" (voicing sum);
     - lower: gain circle **AVS** → box **RGS** → same voicing-sum "+".
   - Voicing-sum "+" output → pole of switch **SW**. SW has two contacts:
     - upper contact → left input of the **cascade-input "+"** (above SW);
     - lower contact → line running down and left to the parallel branch (see 4).
2. **Cascade branch** (right column, top to bottom): cascade-input "+" output → up and over to
   `RNP → RNZ → R1 → R2 → R3 → R4 → R5` → output enters the **final output "+"** (top input).
   The cascade-input "+" has a second (right) input coming from the **AH** gain circle (aspiration, see 3).
3. **Noise source.** Box **RANDOM NUMBER GEN.** → multiplier circle (⊗) labelled **MOD.** (second input = F0 line,
   i.e. modulation by the voicing period) → box **LPF**. LPF output splits:
   - up through gain circle **AH** → long line up/right → right input of the cascade-input "+" (aspiration into the
     cascade branch);
   - right through gain circle **AF** → (a) into the **parallel-input "+"** (below FIRST DIFF.), and (b) onto a
     vertical bus (at the x-position of the FIRST DIFF. column) that feeds, below the "+", the circles A5, A6 and AB.
     *[Note: the AF-to-bus junction and whether this bus is joined to the FIRST DIFF. output line are hard to resolve
     at the scan resolution; as drawn, the FIRST DIFF. output turns right into the "+" just above where the AF line
     meets the bus, and no junction dot is printed.]*
4. **Parallel branch.** Line from SW lower contact runs left then down:
   - a tap off this line (before FIRST DIFF.) → gain circle printed **"R1"** *[sic? — evidently A1]* → box **R1** →
     final output "+";
   - the line continues down into box **FIRST DIFF.** → FIRST DIFF. output → parallel-input "+" (together with the
     AF signal). Parallel-input "+" output → vertical bus feeding four branches:
     `AN → RNP`, `A2 → R2`, `A3 → R3`, `A4 → R4`, each resonator → final output "+".
   - from the AF bus (3b): `A5 → R5`, `A6 → R7` *[sic? — box printed "R7"; the sixth parallel formant, i.e. R6 /
     F6, B6]*, and `AB` (gain only, no resonator, the bypass path) → final output "+".
5. **Output.** Final output "+" has 9 inputs as drawn (top to bottom: cascade R5, parallel R1, RNP, R2, R3, R4,
   R5, R7[sic?], AB) — all drawn as plain "+" inputs, no signs marked → box **FIRST DIFF.** (radiation
   characteristic, Section 12.3) → output arrow.

*[Note: Figure 12-6 shows the cascade R1–R5 only (NFC=5); no glottal-pulse/RNZ parameters are written on the
figure — parameter mapping is given by Table 12-1: RGP↔FGP,BGP; RGZ↔FGZ,BGZ; RGS↔BGS (frequency 0); RNP↔FNP,BNP;
RNZ↔FNZ,BNZ; Rn↔Fn,Bn.]*

(continuation of 12.1.10)
- Turbulence noise is produced by rapid airflow through a narrow constriction. **Aspiration** if the constriction is
  at the vocal folds (e.g. `HH`); **frication** if above the larynx (e.g. `SS`). A plosive-release explosion is
  mainly frication noise.
- When voicing and turbulence coexist (voiced fricative `ZZ`, voiced `HH`), the noise is amplitude-modulated
  periodically by [continues p.133] …

<!-- book p.132 -->

**Table 12-1: List of control parameters for the software formant synthesizer**

(V = variable, C = constant; reproduced exactly; 6 columns: N, V/C + Symbol, Name, Min., Max., Typ.)

| N  | V/C | Symbol | Name                                   | Min.  | Max.   | Typ.  |
|----|-----|--------|----------------------------------------|-------|--------|-------|
| 1  | V   | AV     | Amplitude of voicing (dB)              | 0     | 80     | 0     |
| 2  | V   | AF     | Amplitude of frication (dB)            | 0     | 80     | 0     |
| 3  | V   | AH     | Amplitude of aspiration (dB)           | 0     | 80     | 0     |
| 4  | V   | AVS    | Amplitude of sinusoidal voicing (dB)   | 0     | 80     | 0     |
| 5  | V   | F0     | Fundamental freq. of voicing (Hz)      | 0     | 500    | 0     |
| 6  | V   | F1     | First formant frequency (Hz)           | 150   | 900    | 500   |
| 7  | V   | F2     | Second formant frequency (Hz)          | 500   | 2500   | 1500  |
| 8  | V   | F3     | Third formant frequency (Hz)           | 1300  | 3500   | 2500  |
| 9  | V   | F4     | Fourth formant frequency (Hz)          | 2500  | 4500   | 3300  |
| 10 | V   | FNZ    | Nasal zero frequency (Hz)              | 200   | 700    | 250   |
| 11 | V   | AN     | Nasal formant amplitude (dB)           | 0     | 80     | 0     |
| 12 | V   | A1     | First formant amplitude (dB)           | 0     | 80     | 0     |
| 13 | V   | A2     | Second formant amplitude (dB)          | 0     | 0 [sic? — 80 expected] | 0 |
| 14 | V   | A3     | Third formant amplitude (dB)           | 0     | 80     | 0     |
| 15 | V   | A4     | Fourth formant amplitude (dB)          | 0     | 80     | 0     |
| 16 | V   | A5     | Fifth formant amplitude (dB)           | 0     | 80     | 0     |
| 17 | V   | A6     | Sixth formant amplitude (dB)           | 0     | 80     | 0     |
| 18 | V   | AB     | Bypass path amplitude (dB)             | 0     | 80     | 0     |
| 19 | V   | B1     | First formant bandwidth (Hz)           | 40    | 500    | 50    |
| 20 | V   | B2     | Second formant bandwidth (Hz)          | 40    | 500    | 70    |
| 21 | V   | B3     | Third formant bandwidth (Hz)           | 40    | 500    | 110   |
| 22 | C   | SW     | Cascade/parallel switch                | 0(c)  | 1(p)   | 0     |
| 23 | C   | FGP    | Glottal resonator 1 frequency (Hz)     | 0     | 600    | 0     |
| 24 | C   | BGP    | Glottal resonator 1 bandwidth (Hz)     | 100   | 2000   | 100   |
| 25 | C   | FGZ    | Glottal zero frequency (Hz)            | 0     | 5000   | 1500  |
| 26 | C   | BGZ    | Glottal zero bandwidth (Hz)            | 100   | 9000   | 6000  |
| 27 | C   | B4     | Fourth formant bandwidth (Hz)          | 100   | 500    | 250   |
| 28 | V   | F5     | Fifth formant frequency (Hz)           | 3500  | 4900   | 3850  |
| 29 | C   | B5     | Fifth formant bandwidth (Hz)           | 150   | 700    | 200   |
| 30 | C   | F6     | Sixth formant frequency (Hz)           | 4000  | 4999   | 4900  |
| 31 | C   | B6     | Sixth formant bandwidth (Hz)           | 200   | 2000   | 1000  |
| 32 | C   | FNP    | Nasal pole frequency (Hz)              | 200   | 500    | 250   |
| 33 | C   | BNP    | Nasal pole bandwidth (Hz)              | 50    | 500    | 100   |
| 34 | C   | BNZ    | Nasal zero bandwidth (Hz)              | 50    | 500    | 100   |
| 35 | C   | BGS    | Glottal resonator 2 bandwidth (Hz)     | 100   | 1000   | 200   |
| 36 | C   | SR     | Sampling rate (Hz)                     | 5000  | 20000  | 10000 |
| 37 | C   | NWS    | Number of waveform samples per chunk   | 1     | 200    | 50    |
| 38 | C   | G0     | Overall gain control (dB)              | 0     | 80     | 48    |
| 39 | C   | NFC    | Number of cascaded formants            | 4     | 6      | 5     |

*[Note: counts as printed — 22 rows marked V (1–21 and 28) and 17 marked C, matching "22 of the 39" in 12.1.9.
F5 (row 28) is V even though it sits among the C rows. "0(c)" = cascade, "1(p)" = parallel.]*

<!-- book p.133 -->

(continuation of 12.1.10)
- … the vocal-fold vibrations. The folds may also vibrate without meeting at the midline; then higher harmonics of
  the voicing spectrum are much reduced and the waveform is nearly sinusoidal.
- Therefore the synthesizer must generate at least: two voicing waveforms (normal and quasi-sinusoidal), two
  frication waveforms (normal and amplitude-modulated), two aspiration waveforms (normal and amplitude-modulated).
  These suffice for English (trills/clicks of other languages might need extra source controls in future).

### 12.1.11 *Voicing source*

- Structure: top left of Figure 12-6. Variable controls: **F0** (fundamental frequency), **AV** (normal voicing
  amplitude), **AVS** (quasi-sinusoidal voicing amplitude).
- An impulse train for normal voicing is generated **whenever F0 > 0**. Each impulse's amplitude is set by AV (dB).
  AV ranges from about **60 dB** in a strong vowel to **0 dB** when voicing is off.
- F0 in Hz; F0=100 → 100-Hz impulse train. Samples between impulses **T0 = SR/F0**; e.g. SR=10,000, F0=200 →
  impulse every 50th sample.
- Quantizing the period to an integer number of samples may be heard in a slow, prolonged F0 transition as a
  "staircase" of mechanical sounds (like monotone synthetic speech). Not serious enough to warrant a higher source
  sampling rate. Optionally, some aspiration noise can be added to normal voicing to partly mask this and give a
  somewhat breathy quality.

### 12.1.12 *Normal voicing*

- Ignoring RGZ: impulse train → low-pass filter **RGP** → smooth waveform resembling a typical glottal
  volume-velocity waveform (Flanagan, 1958).
- **FGP = 0 Hz, BGP = 100 Hz.** Filtered impulses have a spectrum falling smoothly at about **−12 dB/octave above
  50 Hz**.
- The waveform's phase spectrum differs from a typical glottal pulse [continues p.134] …

<!-- book p.134 -->

- … and it lacks the spectral zeros often present in natural voicing; neither difference judged perceptually very
  important.
- Antiresonator **RGZ** refines the detailed voicing-spectrum shape for individual talkers more precisely than a
  single low-pass filter allows. The FGZ, BGZ values in Table 12-1 tilt the voicing spectrum up somewhat to match
  speaker **DHK**. Waveform and spectral envelope of normal voicing (impulses through RGP and RGZ): Figure 12-7.

### 12.1.13 *Quasi-sinusoidal voicing*

- **AVS** sets the amount of smoothed voicing in voiced fricatives, voiced aspirates, and voicebars of intervocalic
  voiced plosives.
- Waveshape obtained by low-pass filtering an impulse with low-pass resonators **RGP and RGS**. RGS frequency set to
  **zero** (low-pass); **BGS = 200** sets the cutoff above which harmonics are strongly attenuated.
- Waveform/spectral envelope: Figure 12-7. After vocal-tract and radiation effects, quasi-sinusoidal voicing output
  has significant energy only at the first and second harmonics of F0.
- AVS ranges from about **60 dB** (voicebar or strongly voiced fricative) to **0 dB** (none).
- Some quasi-sinusoidal voicing plus aspiration noise can be added to normal voicing for breathy voice, e.g.
  **AH = AV−3, AVS = AV−6**.

### 12.1.14 *Frication source*

- Turbulent noise simulated by: pseudo-random number generator, modulator, amplitude control **AF**, and a
  **−6 dB/octave low-pass digital filter LPF** (Figure 12-6).
- Ideally the frication source spectrum is approximately flat (Stevens, 1971) with Gaussian amplitude distribution.
  Random-number output is spectrally flat but uniformly distributed between limits set by AF.
- **Pseudo-Gaussian distribution obtained by summing 16 numbers from the random number generator.**

<!-- book p.135 -->

**Figure 12-7: Four periods from voicing waveforms**

Figure content:
- **a) Normal voicing waveform**: four periods; each period is a sharp near-vertical rise to a peak followed by a
  smooth, roughly exponential decay to a flat baseline before the next impulse (RGP impulse response shape).
- **b) Smoothed voicing waveform**: four periods of a nearly sinusoidal, rounded waveform.
- **c) Voicing magnitude spectra**: y-axis "Spectral envelope (dB)", 0 to −80 in 10-dB steps; x-axis "Frequency
  (kHz)", 0–5. Curve "a) −12 dB per octave" starts at 0 dB, falls to about −30 dB by ~0.8 kHz, −40 dB near 2 kHz,
  ≈ −50 dB near 3 kHz, and flattens at ≈ −55 dB at 4–5 kHz. Curve "b) −24 dB per octave" falls much faster: ≈ −60 dB
  near 1.4 kHz, ≈ −70 dB near 1.8 kHz, reaching the −80 dB floor at ≈ 2.3 kHz.

<!-- book p.136 -->

(continuation of 12.1.14)
- In theory the noise source is an ideal pressure source; its volume velocity depends on the impedance it sees.
  Since $T(f)$ relates source volume velocity to lip volume velocity, noise volume velocity must be estimated.
  Assumption: source volume velocity ∝ integral of source pressure (excellent for a source at the lips, where
  radiation impedance is largely inductive; approximate elsewhere).
- The integral is approximated by first-order low-pass digital filter **LPF** (Figure 12-6):

$$y(nT) = x(nT) + y(nT-T)$$

  *(unnumbered equation, printed exactly so — a pure accumulator with coefficient 1)*
- The radiation characteristic (later) is a digital high-pass filter that exactly cancels LPF. (For efficiency the
  radiation characteristic can be moved into the voicing-source circuit and LPF removed from the noise source.)
- Example noise volume velocity: Figure 12-8. Its spectrum fluctuates randomly about the expected long-term average
  spectrum (dashed curve, shifted up 10 dB for clarity). Short noise samples vary spectrally by nature.
- **Modulation:** the random-number output is amplitude-modulated by "MOD" (Figure 12-6) **whenever F0 > 0 and
  AV > 0**. Voiceless sounds (AV=0) are not modulated (folds spread and stiffened, not vibrating).
  **Degree of modulation fixed at 50 percent. Modulation envelope = square wave with period equal to the
  fundamental period.** Degree need not vary over a sentence; only its presence in voiced fricatives and voiced
  aspirated sounds matters.
- **AF** in dB: 60 → strong frication; 0 → frication source effectively off.

### 12.1.15 *Aspiration source*

- Aspiration noise is essentially the same as frication noise but generated in the larynx. In a strictly parallel
  model AF can be used to [continues p.137] …

<!-- book p.137 -->

**Figure 12-8: Waveform segment and magnitude spectrum of frication noise**

Figure content:
- Top: random-looking noise waveform segment, duration marked "25.6 ms." (= 256 samples at 10 kHz).
- Bottom: y-axis "Magnitude spectrum (dB)", 0–60 in 10-dB steps; x-axis "Frequency (kHz)", 0–5. Solid jagged
  spectrum starting ≈ 55–58 dB at 0 Hz, ≈ 40–48 dB around 0.5–1 kHz, ≈ 25–35 dB from 1.5–2 kHz, then fluctuating
  ≈ 15–33 dB from 2–5 kHz, with deep notches (to ≈ 7–10 dB) near 2.25 and 2.7 kHz. Dashed curve (expected long-term
  spectrum + 10 dB) falls smoothly from above 60 dB near 0.5 kHz to ≈ 50 dB at 2 kHz, ≈ 44 dB at 3 kHz, ≈ 41 dB at
  4–5 kHz (a −6 dB/octave low-pass shape flattening at high frequency).

(continuation of 12.1.15)
- … generate both frication and aspiration. In the cascade configuration, aspiration goes through the cascade
  vocal-tract model (designed for laryngeal sources), whereas fricatives need the parallel configuration; so the
  cascade/parallel configuration needs separate frication and aspiration controls.
- **AH** (dB) = amplitude of aspiration sent to the cascade model; 60 → strong aspiration; 0 → aspiration off.
- Frication and aspiration are produced by identical processes, so Figure 12-8 also characterizes aspiration.

<!-- book p.138 -->

### 12.1.16 *Pitch-synchronous updating of voicing source amplitudes*

- **AV and AVS take effect only when a glottal impulse is issued** (amplitudes change discontinuously at the start
  of each glottal period), to avoid pops/clicks from mid-period amplitude jumps.

### 12.1.17 *Generation of plosive bursts with a predictable spectrum*

- **AF and AH are linearly interpolated over the 5-msec (50-sample) interval** (so a new noise amplitude is reached
  with a 5-msec delay). Gives more gradual onsets for fricatives and `HH`.
- Exception: **if AF increases by more than 50 dB over its value in the previous 5-msec segment, AF jumps
  instantaneously to the new target** (plosive bursts need faster onset).
- **The pseudo-random number generator is reset at plosive burst onset**, so each burst has exactly the same source
  waveform; the reset value was chosen to make the burst spectrum as flat as possible. *[Note: the seed value is not
  given.]*

### 12.1.18 *Control of fundamental frequency*

- Sometimes the first glottal pulse (voicing onset) must be timed precisely relative to a burst; e.g. in **pa**: a
  5-msec frication burst, 40 msec aspiration, voicing onset exactly 45 msec after burst onset.
- Normally a glottal pulse is issued at a time given by the reciprocal of the F0 value in effect when the last
  glottal pulse was issued.
- **If AV or F0 is zero, no glottal pulse is issued in that 5-msec interval; no pulses are issued until the exact
  moment both AV and F0 become nonzero.**
- In the pa example, AV and F0 are zero through closure, burst and aspiration; AV ≈ 60 dB and F0 ≈ 130 Hz exactly
  45 msec after burst onset.
- With a 5-msec update interval, VOT can be specified in 5-msec steps; for finer precision change the update
  interval from 5 msec (**NWS=50**) to e.g. 2 msec (**NWS=20**).

<!-- book p.139 -->

## 12.2 **Vocal tract transfer functions**

- Vocal-tract acoustics are determined by cross-sectional area vs. distance from larynx to lips — a nonuniform
  transmission line, solvable below about 5 kHz with the one-dimensional wave equation (Fant, 1960); above 5 kHz
  three-dimensional modes matter. Solutions give a transfer function from glottal-source volume-velocity samples to
  lip output volume velocity.
- Figure 12-6 realizes two vocal-tract transfer-function types: (1) a **cascade** of digital resonators, modelling the
  tract whenever the source is within the larynx; (2) a **parallel** set of resonators with amplitude controls,
  modelling the tract for frication noise. The parallel set can also model laryngeal sources, but less well than
  the cascade.

### 12.2.1 *Cascade vocal tract model*

- Below 5 kHz, assuming the 1-D wave equation, $T(f)$ is a product of poles and zeros; for nonnasalized articulation
  with a laryngeal source it has about **five complex pole pairs and no zeros** (Fant, 1960): all-pole (no side
  branches or multiple paths).
- During aspiration the glottis is partly open, so subglottal poles/zeros appear; the only approximation in the
  synthesizer is to **increase the first formant bandwidth to about 300 Hz**. Remaining distortions are probably
  perceptually minor.
- Five resonators suit a ~**17 cm** (typical male) vocal tract: average formant spacing = speed of sound / (2 × length)
  ≈ **1000 Hz**. A typical female tract is 15–20 percent shorter, so only four formant resonators should represent a
  female voice in a 5-kHz simulation (or the simulation should extend to about 6 kHz).
- Suggested: approximate women's and children's voices by **NFC = 4**, removing the fifth formant from the
  [continues p.140] …

<!-- book p.140 -->

- … cascade branch of Figure 12-6. A very long male tract may need a sixth cascade resonator. **NFC can be 4, 5 or
  6.** Changing NFC implies a different tract length, so change it with care.
- Ignoring RNP and RNZ, the five-resonator cascade has volume-velocity transfer function (Gold and Rabiner, 1968):

$$T(f) = \prod_{n=1}^{5} \frac{A(n)}{1 - B(n)z^{-1} - C(n)z^{-2}} \qquad (6)$$

  with $A(n), B(n), C(n)$ from the $n$th formant frequency $F(n)$ and bandwidth $BW(n)$ via Equation 2. The numerator
  constants $A(n)$ make $T(f)$ = 1 at zero frequency (dc airflow unimpeded). $|T(f)|$ is plotted in Figure 12-9 for
  several formant frequency/bandwidth sets.

### 12.2.2 *Relationship to analog models of the vocal tract*

- In continuous (Laplace s-plane) form, Eq. 6 becomes an infinite product of poles:

$$T(f) = \prod_{n=1}^{\infty} \frac{s(n)s^*(n)}{[s+s(n)][s+s^*(n)]} \qquad (7)$$

  where $s = 2j\pi f$ and

$$s(n) = \pi BW(n) + 2j\pi F(n)$$
$$s^*(n) = \pi BW(n) - 2j\pi F(n)$$

- Eqs. 6 and 7 are exactly equivalent for an ideal vocal tract (uniform tube closed at the glottis, all bandwidths
  equal, e.g. 100 Hz), and indistinguishable below 5 kHz. In a practical (analog) synthesizer the infinite product
  can only be approximated, e.g. five electronic resonators plus a higher-pole correction network (Fant, 1959).

<!-- book p.141 -->

**Figure 12-9: Magnitude of the vocal tract transfer function**

Figure content: four panels, each y-axis "|T(f)| (dB)", x-axis "Frequency (kHz)" 0–5.
- **"Uniform tube"** (top-left, y −20…20): five equal peaks ≈ +12 dB at 0.5, 1.5, 2.5, 3.5, 4.5 kHz; valleys ≈ −3 dB;
  0 dB at f=0.
- **IY** (top-right, y −20…20): peaks ≈ +11 dB near 0.27 kHz, ≈ +10 dB near 2.1–2.2 kHz, ≈ +20 dB near 2.9–3.0 kHz,
  ≈ +7 dB near 3.6 kHz; deep valley ≈ −13 dB around 0.8–1.0 kHz; falls to ≈ −8 dB at 5 kHz.
- **AA** (bottom-left, y −10…30): peaks ≈ +22 dB near 0.7 and 1.1 kHz (two close peaks), ≈ +12 dB near 2.5 kHz,
  ≈ +12 dB near 2.9–3.0 kHz *[approx.]*, ≈ +7 dB near 3.7 kHz; valley ≈ −2 dB near 1.7 kHz; ≈ −5 dB at 5 kHz.
- **UW** (bottom-right, y −30…10): peaks ≈ +10 dB near 0.3 kHz, ≈ +7 dB near 0.85 kHz, ≈ −2 dB near 2.2 kHz,
  ≈ −15 dB near 3.2 kHz, ≈ −16 dB near 3.9 kHz; falls to ≈ −30 dB at 5 kHz.
  *[All peak values read by eye from a small plot; formant positions approximate.]*

### 12.2.3 *Formant frequencies*

- Each formant resonator gives a peak in Figure 12-9; peak "n" frequency is set by control parameter **Fn**. Peak
  amplitude depends on Fn, bandwidth control **BWn**, and the other formant frequencies (discussed below).
- Formant values for DHK's phonetic segments: Chapter 11. F1–F3 vary substantially with articulation; observed
  ranges for a typical male: **F1 ≈ 180–750 Hz, F2 ≈ 600–2300 Hz, F3 ≈ 1300–3100 Hz.** F4 and F5 frequencies and
  bandwidths vary less and could be held [continues p.142] …

<!-- book p.142 -->

- … constant with little quality loss; they shape the overall spectrum but add little intelligibility. The Table 12-1
  F4/F5 values give an energy concentration around **3 to 3.5 kHz** and rapid falloff above about **4 kHz**, typical
  of many talkers.

### 12.2.4 *Formant bandwidths*

- Bandwidths result from losses: heat conduction, viscosity, cavity-wall motion, radiation from the lips, and the
  real part of the glottal source impedance. Hard to derive from natural speech analysis (glottal spectrum
  irregularities); estimated with swept-tone sources (Fujimura and Lindqvist, 1971). They vary by a factor of two or
  more across phonetic segments. Typical values: Chapter 11.
- Variation is small enough that all bandwidths may be held constant in some applications, varying only F1, F2, F3
  for nonnasalized vowels and sonorant consonants.

### 12.2.5 *Nasals and nasalization of vowels*

- Nasal murmurs and nasalization of vowels adjacent to nasals cannot be approximated by five cascade resonators
  alone: "More that five formants" *[sic? — "than"]* are often present, and formant amplitudes do not follow cascade
  relationships because of transfer-function zeros (Fujimura, 1961, 1962). Example transfer functions for a nasal
  murmur and nasalized `IH` (from the recorded syllable "dim"): Figure 12-10.
- Nasalization adds poles and zeros due to a side-branch resonator. In Figure 12-10 the murmur and nasalized `IH`
  have an extra pole pair and zero pair near F1. The oral cavity is the side branch in a nasal murmur; the nose is
  the side branch in a nasalized vowel (sound radiated through the nostrils is insignificant compared with the
  effect of the lowered velum on the formant structure of lip output).
- Nasalization of adjacent vowels is important in nasal-consonant synthesis. Perceptually the most important change
  [continues p.144] …

<!-- book p.143 -->

**Figure 12-10: Nasalization of the vowel `IH` in the syllable "dim"**

Figure content: three stacked panels, y-axis "|T(f)| (dB)", x-axis "Frequency (kHz)" 0–5 (labelled on the bottom
panel). Each panel shows a smooth upper envelope curve (model transfer function) and a jagged lower curve (measured
harmonic spectrum).
- **IH** (y 0–60): envelope peaks ≈ 51 dB near 0.4 kHz, ≈ 51 dB near 1.85 kHz, ≈ 50 dB near 2.5 kHz, ≈ 52 dB near
  3.15 kHz, ≈ 47 dB near 3.4 kHz *[approx.]*, small bump ≈ 28 dB near 4.2 kHz; valley ≈ 27 dB near 1.1 kHz; falls to
  ≈ 15 dB at 5 kHz.
- **IH (nasalized)** (y 0–60): first peak reduced/broadened to ≈ 49 dB near 0.5 kHz; peaks ≈ 50 dB near 1.8 kHz,
  ≈ 54 dB near 2.5 kHz, ≈ 55 dB near 3.1–3.3 kHz (merged double peak); valley ≈ 27 dB near 1.1 kHz; falls to ≈ 5 dB
  at 5 kHz.
- **MM** (nasal murmur, y 0–40): envelope peaks ≈ 37 dB near 0.25 kHz, ≈ 34 dB near 1.1 kHz, broad low hump ≈ 17 dB
  near 2 kHz, ≈ 23 dB near 2.55 kHz, ≈ 23 dB near 3.6 kHz; falls to ≈ 2 dB at 5 kHz.

<!-- book p.144 -->

- … from vowel nasalization is a **reduction in first-formant amplitude**, caused by a nearby low-frequency pole pair
  and zero pair. F1 also tends to shift slightly toward about **500 Hz**.
- Nasal murmurs and vowel nasalization are approximated by inserting resonator **RNP** and antiresonator **RNZ** into
  the cascade model. **FNP and FNZ should be set to a fixed value of about 250 Hz**, but FNZ must be **increased**
  during nasals and nasalization (control strategies in Chapter 11). **The RNP–RNZ pair is effectively removed from
  the cascade during nonnasalized sounds if FNP = FNZ.**

### 12.2.6 *Parallel vocal tract model for frication sources*

- For frication excitation the transfer function has poles and zeros. Poles are continuous with adjacent segments'
  formants (they are natural resonances of the whole tract regardless of source location), so using the vocalic
  formant-frequency parameters to place frication maxima is well motivated (and keeps the noise from "dissociating"
  from the rest of the signal).
- Zeros are frequencies where the impedance looking back toward the larynx from the source is infinite (series
  pressure source then produces no output volume velocity). Zeros create notches and modify formant amplitudes.
  Notches matter little perceptually, since masking by adjacent harmonics limits notch detectability (Gauffin and
  Sundberg, 1974).
- Finding: **a parallel set of digital formant resonators with amplitude controls and no antiresonators** gives a
  satisfactory approximation for frication excitation.
- Formant amplitudes are set to excite selected formants, usually those of the cavity in front of the constriction
  (Stevens, 1972); any zeros are accounted for by the amplitude settings. Simple rules for formant amplitudes (and
  bypass amplitude) vs. place of articulation follow from a quantal theory (Stevens, 1972): only formants
  associated with the front cavity [continues p.145] …

<!-- book p.145 -->

- … are strongly excited; supported by the fricative/burst amplitude specifications in Chapter 11, derived by
  matching natural frication spectra.
- **Six formant resonators** in the parallel configuration of Figure 12-6. The sixth was added for very-high-frequency
  noise in `SS`, `ZZ`, whose main energy is near **6 kHz**, above the 5-kHz limit of a 10,000 sample/s simulation.
  In `SS` frication noise rises gradually just below 5 kHz (low-frequency skirt of the 6-kHz resonance), which is
  well approximated by a resonator at about **4900 Hz**. An extra resonator is preferred to raising F5 for sibilants,
  avoiding clicks and moving energy concentrations.
- **Bypass path** with amplitude **AB**: needed because `FF`, `VV`, `TH`, `DH` have no prominent resonant peaks; the
  synthesizer must be able to bypass all resonators for a flat transfer function.
- Voiced fricative: two sources (voicing at the glottis, frication at the constriction). **Quasi-sinusoidal voicing
  output is sent through the cascade model, while frication excites the parallel branch.**

### 12.2.7 *Simulation of the cascade configuration by the parallel configuration*

- The laryngeally excited tract can also be approximated by five parallel formant resonators — the same ones used
  for frication — with suitable amplitude settings.
- Rules for formant amplitudes in a cascade $T(f)$ as F1–F5 and bandwidths change (from Equation 6, assuming each
  $F(n)$ is at least five to ten times $BW(n)$):
  1. Formant peaks are equal for the case [continues p.146] …

<!-- book p.146 -->

**Figure 12-11: Effect of parameter changes on the vocal tract transfer function**

Figure content: four panels, y-axis "Transfer function |T(f)| (dB)", x-axis "Frequency (kHz)" 0–5.
- **(a) "Uniform tube"** (y −10…20): five equal peaks ≈ +12 dB at 0.5, 1.5, 2.5, 3.5, 4.5 kHz.
- **(b)** (y −10…20): same as (a) but second-formant peak shown for three bandwidths, labelled "BW2=100",
  "BW2=50" (tallest, ≈ +6 dB above baseline peak) and "BW2=200" (lowest, ≈ −6 dB).
- **(c)** (y −20…30): two curves — "F1=500" (uniform tube) and "250" (F1 lowered to 250 Hz). With F1=250 the F1 peak
  moves to 250 Hz and all higher peaks are ≈ 12 dB lower (higher-formant peaks ≈ +12 → ≈ 0 dB).
- **(d)** (y −20…30): four curves labelled at the right edge "3", "4", "1", "2" (top to bottom), showing two
  adjacent formants approaching each other (curves with formant pairs moved together at various positions);
  approaching peaks are raised by an extra few dB.

(continuation of 12.2.7 rules)
  1. (cont.) … formant frequencies **500, 1500, 2500, 3500, 4500 Hz** and all bandwidths equal at **100 Hz** —
     a uniform-area tract, closed glottis, open lips (unrealistic bandwidths): Figure 12-11(a).
  2. **Peak amplitude ∝ 1/bandwidth.** Doubling bandwidth lowers the peak **6 dB**; halving raises it **6 dB**
     (Figure 12-11(b)).
  3. **Peak amplitude ∝ formant frequency.** Doubling a formant frequency raises its peak **6 dB** (Figure 12-11(c)).
     True of $T(f)$ but not of the output spectrum: glottal source falls about −12 dB/octave and radiation adds
     +6 dB/octave, net $+6 -12 +6 = 0$ dB.
  4. Changing a formant frequency changes all **higher** formant peaks by a factor ∝ frequency squared. E.g.
     [continues p.147] …

<!-- book p.147 -->

  4. (cont.) … halving a formant frequency lowers all higher formants by **12 dB**, i.e. $(.5)^2$
     (Figure 12-11(c)).
  5. Two adjacent formants cannot come closer than about **200 Hz** (coupling of natural modes). When two formants
     come within about this distance, both peaks rise an additional **3 to 6 dB** (Figure 12-11(d)).
- Parallel-model formant peak amplitudes are constrained so that **with A1–A5 all at 60 dB, the transfer function
  approximates the cascade model**. Achieved by: (1) adjusting the gain of higher formants for frequency changes in
  lower formants (a higher formant rides on the skirts of all lower formants in a cascade (Fant, 1960)); (2) rules
  raising formant amplitudes when two formant frequencies come close; (3) **a first-difference calculation removing
  low-frequency energy from the higher formants**, which would otherwise distort the F1 region for some vowels
  (Holmes, 1973). *[Note: no numeric form of the gain-correction rules (1) and (2) is given in these pages.]*
- Cascade vs. parallel $|T(f)|$ compared for several vowels in Figure 12-12. Good match near formant peaks, but the
  parallel model introduces transfer-function zeros (notches) between peaks; these matter little perceptually,
  because energy in the peak on the notch's low-frequency side masks the notch (Gauffin and Sundberg, 1974).
- Many early parallel synthesizers summed formant outputs without filtering out low-frequency energy from resonators
  other than F1; others combined outputs with alternating signs. Effects shown in Figure 12-13: some vowels poorly
  modelled by both. Alternating-sign is perceptually less bad (notches less perceptible than energy filling a valley
  between formants). Comparing Figures 12-12 and 12-13: the present parallel configuration beats both.
- A nasal formant resonator **RNP** in the parallel branch helps approximate nasal murmurs and vowel nasalization
  when the cascade branch [continues p.150] …

<!-- book p.148 -->

**Figure 12-12: Preemphasized output spectra from cascade and parallel models**

Figure content: 4 rows × 2 columns; left column labelled "Cascade", right "Parallel". Every panel: y-axis
"|T(f)| (dB)" 0–50, x-axis "Frequency (kHz)" 0–5; smooth envelope plus jagged harmonic spectrum below it.
- **IY**: cascade — peaks ≈ 43 dB near 0.27 kHz, ≈ 43 dB near 2.0 kHz, ≈ 50 dB double peak near 2.8–3.1 kHz, falling
  to ≈ 16 dB at 5 kHz; valley ≈ 15 dB near 1 kHz. Parallel — similar peaks, but high-frequency level stays higher
  (≈ 32 dB at 5 kHz).
- **AA**: cascade — peaks ≈ 47 and 52 dB near 0.7 and 1.1 kHz, ≈ 43 dB near 2.6 kHz, ≈ 46 dB near 3.1 kHz, falling to
  ≈ 15 dB at 5 kHz; valley ≈ 30 dB near 1.8 kHz. Parallel — peaks ≈ 44, 46 dB at 0.7, 1.1 kHz, ≈ 43–45 dB at
  2.6–3.3 kHz, ≈ 33 dB at 5 kHz.
- **UW**: cascade — peaks ≈ 45 dB near 0.27 kHz, ≈ 32 dB near 0.85 kHz, ≈ 20 dB near 2.2 kHz, ≈ 19 dB plateau near
  3.3–3.6 kHz, falling to 0 near 4.4 kHz. Parallel — ≈ 45, 30, 20 (2.1 kHz), 20 (3.2 kHz) dB, ≈ 7 dB at 5 kHz.
- **Uniform tract**: cascade — peaks ≈ 45, 45, 39, 37, 35 dB at 0.5, 1.5, 2.5, 3.5, 4.5 kHz. Parallel — peaks ≈ 46,
  46, 42, 41 dB at 0.5, 1.5, 2.5, 3.5 kHz; ≈ 28 dB at 5 kHz (no distinct fifth peak visible).
  *[All values read by eye.]*

<!-- book p.149 -->

**Figure 12-13: Spectra from two different parallel synthesis configurations**

Figure content: 4 rows × 2 columns; left column labelled "Parallel +-+" (alternating signs), right column
"Parallel +++" (all same sign, no low-frequency removal). Axes as in Figure 12-12 (|T(f)| (dB) 0–50; 0–5 kHz).
Vowels per row: IY, AA, UW, Uniform tract.
- **Left (+−+)**: smooth envelopes with peaks at the formants and no deep notches, but valley regions partly
  filled/misshapen; e.g. IY: harmonic spectrum drops to ≈ 0 between ≈ 0.4 and 1.8 kHz (deep gap) while envelope
  valley ≈ 15 dB; UW harmonics vanish above ≈ 2.2 kHz.
- **Right (+++)**: deep notches between adjacent formants (e.g. AA: notches near 0.95 kHz and 1.8 kHz down to ≈ 5–10
  dB; Uniform tract: notches near 0.9 and 1.85 kHz down to ≈ 0–2 dB; IY: notch near 2.3 kHz).
  *[Note: the column labels and plotted shapes are reproduced as printed; the mapping between "+-+"/"+++" and the two
  methods described in the text (no low-frequency filtering vs. alternating signs) is not stated explicitly in the
  caption.]*

<!-- book p.150 -->

(continuation of 12.2.7)
- … is not used. **Neither the parallel nasal formant nor the parallel first-formant resonator is needed in the
  normal cascade/parallel configuration (SW=0)**; both are required for nasalization in the special-purpose
  **all-parallel configuration (SW=1)**.

## 12.3 **Radiation characteristic**

- The "radiation characteristic" box in Figure 12-6 models frequency-dependent directivity of sound radiated from
  the head. Sound pressure about a metre in front of the lips is ∝ the time derivative of lip-plus-nose volume
  velocity and ∝ 1/r, r = distance from lips (Fant, 1960). Simulated by the **first difference** of lip-nose volume
  velocity:

$$p(nT) = u(nT) - u(nT-T) \qquad (8)$$

- It adds a gradual rise to the overall spectrum (Figure 12-14).

**Figure 12-14: Transfer function of the radiation characteristic**

Figure content: y-axis "|R(f)| (dB)", −50 to 0 in 10-dB steps; x-axis "Frequency (kHz)", 0–5. Curve starts very low
near 0 Hz (≈ −45 dB or below at the left edge), rises steeply to ≈ −20 dB near 0.3 kHz, ≈ −12 dB near 0.8 kHz,
≈ −6 dB near 2 kHz, approaching 0 dB at 5 kHz. *[Note: consistent with |1 − e^{−j2πfT}| normalized to 0 dB at
5 kHz; the normalization is my inference, not stated in the book.]*

<!-- book p.151 -->

*[Page 151 begins Chapter 13, "Some measures of intelligibility and comprehension" (footnote: written by D. Pisoni in
1978-9). It is outside Chapter 12 and is not transcribed here.]*
