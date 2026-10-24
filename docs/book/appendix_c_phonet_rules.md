# Appendix C: Context-dependent rules for PHONET

Source: Allen, Hunnicutt and Klatt, *From Text to Speech: The MITalk System* (1987), Appendix C, printed pp. 182-201.

## Transcription conventions

- **Exact:** every table, every number, every segment-class list, every rule formula, every section heading and every section pattern line. Rules are written on one line each. Long rules are joined into one line.
- **Paraphrased, not verbatim:** the explanatory prose. That covers the paragraphs, the one-line comment printed before each rule, and the state-variable definitions. These are short restatements in my own words, marked with a leading `#` or written as *italic notes*. They keep every number, variable name and table/figure reference.
- Notation:
  - `<-` stands for the book's ←, `INC` for ↑ and `DEC` for ↓.
  - `[features]_` is the current segment (the matrix with "—" under it in the book).
  - A bare `_` is the current segment with no features.
  - `[features ; S]` is a neighbouring segment with `S` under it.
  - `[features ; X]` is a segment bound to the name X, for example in `TARGET[f2, X]` or `Tcobst[X]`.
  - A lone `S` between matrices is any one segment.
  - `{ A / B }` lists alternatives.
- `[sic?]` marks text that looks like a misprint, kept as printed. `[?]` marks a character I could not read with confidence.
- About the page files: `book_pNNN.png` shows printed page NNN+1. So book_p181.png through book_p201.png are printed pp. 182-202. The first page of Appendix C (printed p.181: the heading, C.1 and the start of the rule-format description) is **not in the scan set**. Printed p.202 is the start of Appendix D and is only noted below.

---

<!-- book p.181 -->

*(Added afterwards from a corrected render of printed p.181; paraphrased.)*

### C.1 Notation

*Note: the rules use the same form as the lexical stress rules in Chapter 6, with these changes. `S` stands for any phonetic segment, replacing the `V` and `C` used there. Besides +stress and -stress, segments are classified by the general features listed in the next section (C.3).*

General form: `variable <- value / pattern`, meaning *variable* is set to *value* when *pattern* matches. `INC` and `DEC` (the book's ↑ and ↓) raise or lower *variable* by *value*.

*Note: the variable is one of several parameters holding state about the current segment. For example, "Target" is a table of target values for each parameter at the end of the current segment.* Example rule:

    Target[avc] DEC 30 / [+fricative,+voiced]_ [-vowel ; S]

*means: lower the Target for avc by 30 dB when the current segment is a voiced fricative and the next segment is not a vowel.*

*Note: program structure. The top level loops over the phonetic segments of the input, one at a time. For each segment it initializes the state variables, then applies the rules in the order printed. After the rules have run, the state variables are used to update the output parameter values over the current time interval, from the segment's start time to its end time as already set by PROSOD.*

<!-- book p.182 -->

*(Text continues from p.181.)*

*Note: each rule pattern is matched on its own. A rule that starts with "ELSE" is tried only when the rule just before it did not match. The next section describes the state variables.*

### C.2 State variables

*Note: most state variables are one-dimensional arrays with one value per output parameter. `variable[parameter]` is the value of `variable` for `parameter`. `variable[p1, p2]` covers both p1 and p2. `variable[p1..p2]` covers p1 through p2, inclusive.*

The state variables:

- **Cumdur[av..f0]**: the start time of the current segment for each parameter. It is absolute time in msec from the start of the utterance and corresponds to t1 in Figure 11-6.
- **Segdur[av..f0]**: the duration in msec of the current segment for each parameter. Parameter x ends at Cumdur[x]+Segdur[x]. Corresponds to t2 in Figure 11-6.
- **Mintime[av..f0]**: the earliest absolute time that backward smoothing of each parameter may reach. Corresponds to t0 in Figure 11-6.
- **Trantype[av..f0]**: the transition type, as in Figure 11-6.
- **Target[av..f0]**: the target value of each parameter at time Segdur+Cumdur. Amplitudes are in dB; frequencies and bandwidths are in Hz.
- **Diptar[f1..f3]**: diphthong target values, in Hz.
- **Oldval[av..f0]**: the value of each parameter at time Cumdur, which is the value at the end of the previous segment.

<!-- book p.183 -->

- **Nextar[av..f0]**: the Target value of the next segment.
- **Tcf[av..f0]**: the duration of forward smoothing, measured from Cumdur.
- **Tcb[av..f0]**: the duration of backward smoothing, measured from Cumdur and limited by Mintime.
- **Bper[av..f0]**: the percentage of movement from the locus toward the target in a CV or VC transition. (The printed sentence lacks "is", as in "This the percent". [sic?])
- **Bvf[av..f0]**: the value each parameter should have just after time Cumdur, usually derived from Bper. (The rules later call this `Bvalf`.)
- **Bvb[av..f0]**: the value each parameter should have just before time Cumdur. (The rules later call this `Bvalb`.)

### C.3 Phonetic segment classes

| class | members |
|---|---|
| affricate | CH, JJ |
| alveolar | DD, DX, EN, NN, SS, TQ, TT, ZZ |
| aspseg | HH, HX, WH |
| dental | DH, TH |
| diphthong | AE, AO, AW, AXR, AY, EH, EXR, EY, IH, IXR, IY, OW, OXR, OY, UH, UW, YU |
| f2back | IY, YU, YY |
| fricative | DH, FF, SS, SH, TH, VV, ZH, ZZ |
| front | AE, EH, EXR, EY, IH, IX, IXR, IY, YU |
| glottal | HH, HX, QQ, SIL |
| high | IH, IX, IXR, IY, UH, UW, UXR, WH, WW, YU, YY |
| labial | BB, EM, FF, MM, PP, VV, WW, WH |

<!-- book p.184 -->

| class | members |
|---|---|
| lateral | EL, LL, LX |
| lax | AE, AO, AX, AXP, EH IH, IX, UH  (no comma after EH in print [sic?]) |
| liqglide | EL, LL, LX, RR, RX, WH, WW, YY |
| low | AA, AE, AO, AW, AXR, AY |
| nasal | EM, EN, MM, NN, NG |
| palatal | CH, JJ, SH, YY, ZH |
| palvel | GP, KP |
| plosive | BB, CH, DD, GG, GP, JJ, KK, KP, PP, TQ, TT |
| retro | ER, RR, RX |
| rglide | AXR, EXR, IXR, OXR, UXR |
| round | AO, OW, OXR, OY, UH, UW, WH, WW, YU |
| schwa | AX, IX |
| sonorant | AA, AE, AH, AO, AW, AX, AXR, AY, EH, EL, EM, EN, ER, EXR, EY, HH, HX, IH, IX, IXR, IY, LL, LX, MM, NG, NN, OW, OXR, OY, RR, RX, UH, UW, UXR, WH, WW, YU, YY |
| stop | BB, CH, DD, DX, EM, EN, GG, GP, JJ, KK, KP, MM, NG, NN, PP, QQ, TQ, TT |
| syllabic | AA, AE, AH, AO, AW, AX, AXR, AY, EH, EL, EM, EN, ER, EXR, EY, IH, IX, IXR, IY, OW, OXR, OY, UH, UW, UXR, YU |
| velar | GG, KK, NG |
| voiced | AA, AE, AH, AO, AW, AX, AXR, AY, BB, DD, DH, DX, EH, EL, EM, EN, ER, EXR, EY, GG, GP, HX, IH, IX, IXR, IY, JJ, LL, LX, MM, NG, NN, OW, OXR, OY, QQ, RR, RX, TQ, UH, UW, UXR, VV, WH, WW, YU, YY, ZH, ZZ |
| vowel | AA, AE, AH, AO, AW, AX, AXR, AY, EH, ER, EXR, EY, IH, IX, IXR, IY, OW, OXR, OY, UH, UW, UXR, YU |
| wglide | AW, OW, UW, YU |
| yglide | AY, EY, IY, OY |

*(Notes on the print: "f2back" really is IY, YU, YY. There is a speck over the S in "SS" and a stray mark before "TH" in the fricative list; I read both as print noise.)*

<!-- book p.185 -->

### C.4 Initialization

```
/_
```

```
# set manner class from segment class
Manner <- vowel / [+vowel]_
ELSE Manner <- stop / [+stop]_
ELSE Manner <- fricative / [+fricative]_
ELSE Manner <- sonorant
```

*Note: amplitude, frequency and bandwidth targets come from Tables C-1 and C-2. HH and HX take their frequency targets from the next segment.*

```
Target[fnz] <- 250 /_
Target[an] <- 0 /_
```

*Note: Tcf comes from Table C-3.*

```
Buramp <- 57 /_
Aspamp <- 51 /_
```

*Note: the default transition type is SETSMO. The transition boundary-value target Bper comes from Table C-4.*

### C.5 General rules

```
/_
```

```
Bper[f0] <- 75
# no amplitude smoothing after a plosive
Mintime[af] <- Cumdur[af] / [+plosive ; S] _
# discontinuous transition out of an unvoiced segment
Trantype[f0, av] <- DISSMO / [-voiced ; S] _
Mintime[f0] <- Cumdur[f0] / [-voiced ; S] _
# breathy offset into a pause
Aspdux <- 30, Oldval[avc] INC 6, Aspaml <- Aspamp / [-SIL ; S] [+SIL]_
```

<!-- book p.186 -->

**Table C-1: Parameter targets for nonvocalic segments**

| seg | av | avc | asp | af | a2 | a3 | a4 | a5 | a6 | ab | f1 | f2 | f3 | f4 | b1 | b2 | b3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| AXP | 57 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 430 | 1500 | 2500 | 3300 | 120 | 60 | 120 |
| BB | 0 | 54 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 72 | 200 | 900 | 2100 | 3300 | 65 | 90 | 125 |
| CH | 0 | 0 | 0 | 0 | 0 | 60 | 75 | 70 | 70 | 0 | 300 | 1700 | 2400 | 3300 | 200 | 110 | 270 |
| DD | 0 | 54 | 0 | 0 | 0 | 0 | 0 | 50 | 82 | 0 | 200 | 1400 | 2700 | 3300 | 70 | 115 | 180 |
| DH | 36 | 54 | 0 | 60 | 0 | 0 | 0 | 0 | 30 | 54 | 300 | 1150 | 2700 | 3300 | 60 | 95 | 185 |
| DX | 44 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 200 | 1600 | 2700 | 3300 | 120 | 140 | 250 |
| EL | 57 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 450 | 800 | 2850 | 3300 | 65 | 60 | 80 |
| EM | 51 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 200 | 900 | 2100 | 3300 | 120 | 60 | 70 |
| EN | 51 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 200 | 1600 | 2700 | 3300 | 120 | 70 | 110 |
| FF | 0 | 0 | 31 | 60 | 0 | 0 | 0 | 0 | 0 | 64 | 400 | 1130 | 2100 | 3300 | 225 | 120 | 175 |
| GG | 0 | 54 | 0 | 0 | 70 | 30 | 30 | 60 | 10 | 0 | 250 | 1600 | 1900 | 3300 | 70 | 145 | 190 |
| GP | 0 | 54 | 0 | 0 | 30 | 70 | 60 | 62 | 62 | 0 | 200 | 1950 | 2800 | 3300 | 120 | 140 | 250 |
| HH | 0 | 0 | 60 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 450 | 1450 | 2450 | 3300 | 300 | 160 | 300 |
| HX | 44 | 60 | 57 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 450 | 1450 | 2450 | 3300 | 200 | 120 | 200 |
| JJ | 0 | 54 | 0 | 0 | 0 | 60 | 75 | 70 | 70 | 0 | 200 | 1700 | 2400 | 3300 | 50 | 110 | 270 |
| KK | 0 | 0 | 0 | 0 | 73 | 30 | 30 | 60 | 10 | 0 | 350 | 1600 | 1900 | 3300 | 280 | 220 | 250 |
| KP | 0 | 0 | 0 | 0 | 30 | 70 | 60 | 62 | 62 | 0 | 300 | 1950 | 2800 | 3300 | 150 | 140 | 250 |
| LL | 50 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 330 | 1050 | 2800 | 3300 | 50 | 100 | 280 |
| LX | 57 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 450 | 800 | 2850 | 3300 | 65 | 60 | 80 |
| MM | 51 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 480 | 1050 | 2100 | 3300 | 40 | 175 | 120 |
| NG | 51 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 480 | 1600 | 2050 | 3300 | 160 | 150 | 100 |
| NN | 51 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 480 | 1400 | 2700 | 3300 | 40 | 300 | 260 |
| PP | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 72 | 300 | 900 | 2100 | 3300 | 300 | 190 | 185 |
| QQ | 0 | 0 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 400 | 1400 | 2450 | 3300 | 120 | 140 | 250 |
| RR | 50 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 330 | 1060 | 1380 | 3300 | 70 | 100 | 120 |
| RX | 57 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 460 | 1260 | 1560 | 3300 | 60 | 60 | 70 |
| SH | 0 | 0 | 31 | 60 | 0 | 60 | 75 | 70 | 70 | 0 | 400 | 1650 | 2400 | 3300 | 200 | 110 | 280 |
| SIL | 0 | 0 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 400 | 1400 | 2400 | 3300 | 120 | 140 | 250 |
| SS | 0 | 0 | 31 | 60 | 0 | 0 | 0 | 50 | 82 | 0 | 400 | 1400 | 2700 | 3300 | 200 | 95 | 220 |
| TH | 0 | 0 | 31 | 60 | 0 | 0 | 0 | 0 | 30 | 54 | 400 | 1150 | 2700 | 3300 | 225 | 95 | 200 |
| TQ | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 50 | 82 | 0 | 200 | 1400 | 2700 | 3300 | 120 | 140 | 250 |
| TT | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 50 | 82 | 0 | 300 | 1400 | 2700 | 3300 | 300 | 180 | 220 |
| VV | 40 | 54 | 0 | 60 | 0 | 0 | 0 | 0 | 0 | 64 | 300 | 1130 | 2100 | 3300 | 55 | 95 | 125 |
| WH | 0 | 57 | 51 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 330 | 600 | 2100 | 3300 | 150 | 60 | 60 |
| WW | 50 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 285 | 610 | 2150 | 3300 | 50 | 80 | 60 |
| YY | 50 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 240 | 2070 | 3020 | 3300 | 40 | 250 | 500 |
| ZH | 40 | 54 | 0 | 60 | 0 | 60 | 75 | 70 | 70 | 0 | 300 | 1650 | 2400 | 3300 | 220 | 140 | 250 |
| ZZ | 40 | 54 | 0 | 60 | 0 | 0 | 0 | 50 | 82 | 0 | 300 | 1400 | 2700 | 3300 | 70 | 85 | 190 |

<!-- book p.187 -->

**Table C-2: Parameter targets for vocalic segments**

A "(2nd)" row is the second line printed under a vowel. It gives f1/f2/f3 only: the diphthong/offglide targets (Diptar). AA, AH, AX and IX have no second line.

| seg | av | avc | asp | af | a2 | a3 | a4 | a5 | a6 | ab | f1 | f2 | f3 | f4 | b1 | b2 | b3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| AA | 57 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 700 | 1220 | 2600 | 3300 | 130 | 70 | 160 |
| AE | 57 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 620 | 1660 | 2430 | 3300 | 70 | 130 | 300 |
| AE (2nd) | | | | | | | | | | | 650 | 1490 | 2470 | | | | |
| AH | 59 | 59 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 620 | 1220 | 2550 | 3300 | 80 | 50 | 140 |
| AO | 58 | 58 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 600 | 990 | 2570 | 3300 | 90 | 100 | 80 |
| AO (2nd) | | | | | | | | | | | 630 | 1040 | 2600 | | | | |
| AW | 57 | 57 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 640 | 1230 | 2550 | 3300 | 80 | 70 | 110 |
| AW (2nd) | | | | | | | | | | | 420 | 940 | 2350 | | | | |
| AX | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 550 | 1260 | 2470 | 3300 | 80 | 50 | 140 |
| AXR | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 680 | 1170 | 2380 | 3300 | 60 | 60 | 110 |
| AXR (2nd) | | | | | | | | | | | 520 | 1400 | 1650 | | | | |
| AY | 58 | 58 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 660 | 1200 | 2550 | 3300 | 100 | 120 | 200 |
| AY (2nd) | | | | | | | | | | | 400 | 1880 | 2500 | | | | |
| EH | 61 | 61 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 530 | 1680 | 2500 | 3300 | 60 | 90 | 200 |
| EH (2nd) | | | | | | | | | | | 620 | 1530 | 2530 | | | | |
| ER | 62 | 62 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 470 | 1270 | 1540 | 3300 | 100 | 60 | 110 |
| ER (2nd) | | | | | | | | | | | 420 | 1310 | 1540 | | | | |
| EXR | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 460 | 1650 | 2400 | 3300 | 60 | 80 | 140 |
| EXR (2nd) | | | | | | | | | | | 450 | 1500 | 1700 | | | | |
| EY | 59 | 59 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 480 | 1720 | 2520 | 3300 | 70 | 100 | 200 |
| EY (2nd) | | | | | | | | | | | 330 | 2200 | 2600 | | | | |
| IH | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 400 | 1800 | 2670 | 3300 | 50 | 100 | 140 |
| IH (2nd) | | | | | | | | | | | 470 | 1600 | 2600 | | | | |
| IX | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 420 | 1680 | 2520 | 3300 | 50 | 100 | 140 |
| IXR | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 320 | 1900 | 2900 | 3300 | 70 | 80 | 120 |
| IXR (2nd) | | | | | | | | | | | 420 | 1550 | 1750 | | | | |
| IY | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 310 | 2200 | 2960 | 3300 | 50 | 200 | 400 |
| IY (2nd) | | | | | | | | | | | 290 | 2070 | 2980 | | | | |
| OW | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 540 | 1100 | 2300 | 3300 | 80 | 70 | 70 |
| OW (2nd) | | | | | | | | | | | 450 | 900 | 2300 | | | | |
| OXR | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 550 | 820 | 2200 | 3300 | 60 | 60 | 60 |
| OXR (2nd) | | | | | | | | | | | 490 | 1300 | 1500 | | | | |
| OY | 62 | 62 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 550 | 960 | 2400 | 3300 | 80 | 120 | 160 |
| OY (2nd) | | | | | | | | | | | 360 | 1820 | 2450 | | | | |
| UH | 63 | 63 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 450 | 1100 | 2350 | 3300 | 80 | 100 | 80 |
| UH (2nd) | | | | | | | | | | | 500 | 1180 | 2390 | | | | |
| UW | 64 | 64 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 350 | 1250 | 2200 | 3300 | 65 | 110 | 140 |
| UW (2nd) | | | | | | | | | | | 320 | 900 | 2200 | | | | |
| UXR | 60 | 60 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 360 | 800 | 2000 | 3300 | 60 | 60 | 80 |
| UXR (2nd) | | | | | | | | | | | 390 | 1150 | 1500 | | | | |
| YU | 64 | 64 | 0 | 0 | 60 | 60 | 60 | 60 | 60 | 0 | 290 | 1900 | 2600 | 3300 | 70 | 160 | 220 |
| YU (2nd) | | | | | | | | | | | 330 | 1200 | 2100 | | | | |

<!-- book p.188 -->

**Table C-3: Default values for duration of forward smoothing (Tcf)**

| param | value | param | value | param | value | param | value |
|---|---|---|---|---|---|---|---|
| av | 25 | avc | 20 | af | 40 | asp | 20 |
| an | 40 | a2p | 40 | a3p | 40 | a4p | 40 |
| a5p | 40 | a6p | 40 | ab | 40 | f0 | 120 |
| f1 | 80 | f2 | 80 | f3 | 80 | f4 | 80 |
| fnz | 150 | b1 | 80 | b2 | 80 | b3 | 80 |

**Table C-4: Default values for Bper**

| Previous manner class | vowel | stop | fricative | sonorant |
|---|---|---|---|---|
| vowel | 50 | 35 | 50 | 75 |
| stop | 65 | 50 | 50 | 65 |
| fricative | 50 | 50 | 50 | 75 |
| sonorant | 25 | 35 | 25 | 50 |

(The column headings are the *current* manner class.)

```
# aspiration from a voiced sonorant into a following unvoiced consonant cuts into the voicing
Aspdux <- 10 / [+sonorant ; S] [-voiced]_
# frication starts early when a fricative follows a sonorant
Cumdur[af] DEC 20, Segdur[af] INC 20 / [+sonorant ; S] [-voiced,+fricative]_
Aspaml DEC 6 / [+sonorant ; S] [-voiced,+stop]_
```

*Note: if Aspdux was set above, move Cumdur[asp] earlier by Aspdux. Set asp at the new Cumdur[asp] to Aspaml. Smooth asp linearly over the 30 msec before Cumdur[asp]. See Figure C-1.*

```
# F0 transitions into and out of voiceless segments are discontinuous
```

<!-- book p.189 -->

```
Trantype[av] <- SMODIS / [-voiced]_
Trantype[f0] <- DISCON / [-voiced ; S] [-voiced]_
Trantype[f0] <- SMODIS / [+voiced ; S] [-voiced]_
Tcf[f1..b3] <- Tcobst[X] / [+stop ; X] [-stop]_
Trantype[av,avc] <- DISCON, Trantype[af..b3] <- DISSMO / [+stop,+nasal ; S] [-stop,+voiced]_
ELSE Trantype[av..ab,avc] <- DISSMO, Trantype[f1..b3] <- SETSMO
```

[Figure: the asp track against time. It is flat at the Oldval level until a point 30 ms after Mintime, then rises linearly to Aspaml. That point is Aspdux before Cumdur. Labels: "30ms.", "Aspdux", "Aspaml", "Oldval", "Mintime", "<- Cumdur".]

**Figure C-1: Pre-aspiration parameter smoothing**

### C.6 Sonorant rules

```
/[+sonor]_
```

```
# lower f4 when the lips are rounded
Target[f4] DEC 100 / [+round]_
# f4 higher in retroflex and lateral segments
Target[f4] INC 300 / { [+retro]_ / [+lateral]_ }
# shorter transitions out of liquids and glides
```

<!-- book p.190 -->

```
Tcf[f1..b3] <- 110 / [+YY ; S] [+round]_
ELSE Tcf[f1..b2] <- 75 / [+liqgli ; S] _
Tcf[f3] <- 90 / [+liqgli,+retro ; S] _
# W- and Y-glides behave like sonorants
Bper[f1..f3] <- 35 / { [+wglide ; S] / [-yglide ; S] } _     [sic?] ("-yglide" as printed)
# larger transition between back vowels and palatalized consonants
Tcf[f2] INC 50 / [+palatal ; S] [+round]_
```

#### C.6.1 *Aspiration of an unvoiced fricative or plosive intrudes on the sonorant*

```
/[-voiced,-glotstop,-SIL,-HH ; S] _
```

```
# assume a stressed, word-initial voiceless plosive
Aspdur <- 40
Apsaml <- Aspamp     [sic?] ("Apsaml" as printed; elsewhere "Aspaml")
# less aspiration when the preceding obstruent is unstressed
Aspdur <- 20, Aspaml DEC 3 / [-stress ; S] _
# longer aspiration in plosive-obstruent clusters
Aspdur <- 55 / [-labial ; S] [-vowel,-lateral]_
ELSE Aspdur <- 50 / [-vowel]_
# little aspiration when the preceding segment is voiced and the current syllable unstressed
Aspdur DEC 10 / [+voiced ; S] S [-stress]_
# a lot of aspiration into silence
Aspdur <- 70 / [+AXP]_
```

<!-- book p.191 -->

```
# aspiration starts during the plosive burst
Cumdur[asp,av,b1] INC Burdur[X] - 5, Aspdur INC Burdur[X] - 5 / [+plosive ; X] _
# longer aspiration when a fricative is followed by a sonorant consonant
Aspdur <- 10 / [+fricative ; S] [+vowel]_
Aspdur <- Segdur[asp]/2 / [+fricative ; S] [-vowel]_
# draw the aspiration segment
Aspdur <- min(Aspdur, Segdur[asp], segdur[av])
Aspaml INC 9 / [+alveolar ; S] [+retro]_
```

*Note: if Aspdur is now nonzero, draw an aspiration segment that starts at Cumdur[asp] and lasts Aspdur. In it, asp = Aspaml, av = 0, avc = 0 and b1 = 150. Then move Cumdur forward by Aspdur for those four parameters. If the previous segment is a fricative, smooth asp backward over the 30 msec before Cumdur[asp].*

#### C.6.2 *Nonnasal sonorants*

```
/[-vowel]_
```

```
# f2 and f3 of RR and LL coarticulate with the next vowel's targets
Target[av] DEC 3 / [+LL]_
Target[f2] <- .9*Target[f2] + .1*TARGET[f2, X] / [+LL]_ [+sonorant,-nasal ; X]
Coart(75), Target[f3] <- Target[f2]+250 / [+RR]_
Target[f2] <- .75*Target[f2] + .25*TARGET[f2, X] / [+RR]_ [+sonorant,-nasal ; X]
# coarticulate with schwa when the next segment is nonsonorant or nasal
```

<!-- book p.192 -->

```
Target[f2] <- .9*Target[f2] + .1*1450 / [+LL]_ { [-sonorant ; X] / [+nasal ; X] }
Target[f2] <- .75*Target[f2] + .25*TARGET[f2, X] / [+RR]_ { [-sonorant ; X] / [+nasal ; X] }
# shorter transitions into liquids and glides
Bper[f1..f3] <- 90 / [-nasal,+sonor ; S] [+lateral]_
# short transitions between retroflex and lateral
Tcf[f1..b3] <- 90 / [-liqgli]_
ELSE Tcf[f1..b3] <- 50 / [+liqgli ; S] _
ELSE Tcf[f1..b3] <- 70
Tcf[f3] <- 90 / [+retro]_
```

### C.7 Vowels

```
/[+vowel]_
```

```
# strong forward coarticulation of schwa
Target[f2] <- avg(Target[f2], TARGET[f2, X]) / [+sonor,-nasal ; X] S [+AXP]_
# no schwa offglide before a velar consonant
Diptar[f2] <- Target[f2] / [+front,+lax]_ [+velar ; S]
# front vowels velarize before +lateral
Diptar[f2] DEC 300 / { [+front]_ / [+yglide]_ } [+lateral ; S]
Diptar[f2] DEC 300 / { [+front,-diphthong]_ / [+yglide,-diphthong]_ } [+lateral ; S]
# YU fronts before an alveolar plosive
```

<!-- book p.193 -->

```
Diptar[f2] INC 200 / [+YU]_ [+alveolar,+plosive ; S]
# longer f2 time constant in a +F2back, +round transition
Tcf[f2] INC 50 / [+f2back ; S] [+round]_
# smaller Y-glide before an alveolar consonant
Diptar[f2] DEC 150 / [+yglide]_ [+alveolar ; S]
```

*Note: a short non-retroflex vowel has centralized formants. Shift Target[f1..f3] and Diptar[f1..f3] toward 490,1450,2500 Hz. The size of the shift is a decaying exponential of the segment duration, with a 60 msec time constant.*

```
# schwa takes formant targets from the neighbouring segments
Target[f3] <- average(Oldval[f3], Target[f3], Nextar[f3])/ [+schwa]_
# back-cavity resonances reach target quickly
Tcf[f2] DEC 20 / [+f2back]_
```

#### C.7.1 *Diphthong rules*

```
/[+diphthong]_
```

*Note: Tcdiph and Tdmid come from Table C-5.*

```
Tcenter[f1..f3] <- Tdmid * Segdur[f1]
Tcdips[f1..f3] <- Tcdiph * .5 * (1 + Segdur[f1] / 45)
# earlier diphthongization after HH in tense vowels (but the rule as printed tests +lax)
Tcenter[f1..f3] <- Tcenter[f1..f2] * .67 / [+HH ; S] [+lax]_     [sic?] (f1..f3 on the left, f1..f2 on the right)
Bvalf[f1..f3] <- average(Target[f1..f3], Diptar[f1..f3])
Bvalf[f1] INC 70 / [+OY]_
Bvalf[f2] INC 200 / { [+AY]_ / [+OY]_ }
Bvalf[f3] DEC 200 / { [+AY]_ / [+OY]_ }
```

<!-- book p.194 -->

```
Tcenter[f3] <- Tcenter[f3] * .3 / [+YU]_
Tcenter[f3] <- Tcenter[f3] * .6 / { [+EXR]_ / [+IXR]_ }
Bvalb[f1..f3] <- Bvalf[f1..f3]
```

*Note: for this step only, use Diptar[f1..f3] in place of Target[f1..f3] and move Cumdur[f1..f3] to Cumdur[f1..f3] + Tcenter[f1..f3]. Then draw a transition (Figure C-2).*

[Figure: a formant track against time. It is flat until Cumdur, rises through "Target" at Cumdur+Tcenter, and levels off at "Diptar" before Cumdur+Segdur. "<- Tcdips -|- Tcdips ->" marks a span either side of Cumdur+Tcenter. The axis labels are Mintime, Cumdur, Cumdur+Tcenter and Cumdur+Segdur.]

**Figure C-2: Diphthong transition smoothing**

**Table C-5: Diphthong transition parameters**

| seg | Tcdiph | Tdmid | seg | Tcdiph | Tdmid | seg | Tcdiph | Tdmid |
|---|---|---|---|---|---|---|---|---|
| AA | 0 | 80 | AE | 100 | 75 | AH | 0 | 0 |
| AO | 110 | 80 | AW | 120 | 70 | AXR | 125 | 40 |
| AY | 100 | 55 | EH | 60 | 70 | ER | 180 | 50 |
| EXR | 100 | 50 | EY | 140 | 55 | IH | 90 | 65 |
| IXR | 100 | 50 | IY | 200 | 45 | OW | 150 | 50 |
| OXR | 110 | 60 | OY | 150 | 60 | UH | 90 | 65 |
| UW | 140 | 55 | UXR | 150 | 50 | YU | 100 | 45 |

### C.8 Obstruents

```
/[-sonor,+nasal]_
```

*(The pattern is printed exactly so: one matrix holding "-sonor, +nasal". C.8.4 Nasals falls under this section, yet the class list makes nasals sonorant. [sic?])*

<!-- book p.195 -->

#### C.8.1 *Fricatives and plosives*

```
/{ [+plosive]_ / [+fricative]_ }
```

```
# devoice unless a vowel follows
Target[avc] DEC 30 / [+voiced]_ [-vowel ; S]
Target[f4] DEC 150 / [+palatal]_
Target[a2p..a6p] <- 0,67,60,65,65 / [+alveolar,+plosive]_ [-alveolar,+retro ; S]
Target[a2p..a6p] <- 0,52,60,70,73 / [+alveolar,+plosive]_ [-alveolar,-retro ; S]
BurAmp DEC 5 / [+velar]_ [-vowel ; S]
# no voicebar in a voiced plosive after an obstruent
Target[avc] <- 0 / { [-voiced ; S] / [-sonor ; S] } [+voiced,+plosive]_
```

#### C.8.2 *Fricatives only*

```
/[+fricative]_
```

```
Target[af] DEC 4 / _ SIL
# short fricatives fall short of target, so raise the target
Target[af] INC IF Segdur[af] < 80 THEN (80 - Segdur[af])*deltat/7 / [-fric,-stop ; S] _ [+vowel ; S]
# stronger voicing between vowels
Target[avc] <- 60 / [+vowel ; S] [+voiced]_ [+vowel ; S]
# palatal fricatives round before a rounded vowel
```

<!-- book p.196 -->

```
Target[f2] DEC 50, Target[f3] DEC 200 / [+palatal]_ [+vowel,+round ; S]
```

#### C.8.3 *Stops*

```
/[+stop]_
```

```
# transition into a stop is partly discontinuous (f0 excepted)
Trantype[av..f1] <- SMODIS, Trantype[f2..b3] <- SETSMO, Trantype[avc] <- SMODIS / [-stop ; S] _
Tcf[f2..b3] <- 10, Tcf[f1] <- 15 / [+high ; S] _
Tcf[f2..b3] <- Tcobst[—], Tcf[f1] <- average(Tcobst[—], 20) / [-high ; S] _
```

(`Tcobst[—]` is printed with a dash and means Tcobst of the current segment.)

**Table C-6: Duration of forward smoothing for obstruents (Tcobst)**

| seg | Tcobst | seg | Tcobst | seg | Tcobst | seg | Tcobst |
|---|---|---|---|---|---|---|---|
| BB | 60 | CH | 100 | DD | 80 | DH | 80 |
| EL | 80 | EM | 60 | EN | 80 | FF | 60 |
| GG | 100 | GP | 100 | HH | 80 | HX | 80 |
| KK | 100 | KP | 100 | MM | 60 | NG | 100 |
| NN | 80 | PP | 80 | QQ | 80 | SH | 100 |
| SIL | 80 | SS | 80 | TH | 80 | TQ | 80 |
| TT | 80 | VV | 60 | ZH | 100 | ZZ | 80 |

##### C.8.3.1 *Insert burst at expense of closure duration*

```
/[+plosive,-gltstop]_ [-stop,-SIL ; S]
```

*Note: Burdur comes from Table C-7.*

```
Burdur INC 10 / [+TT]_ [+retro ; S]
Burdur <- Segdur[af] IF Burdur > Segdur[af]
Burdur <- 5 IF Burdur = 0
```

<!-- book p.197 -->

*Note: lower af by 20 dB at Cumdur[af] and smooth that value backward over 30 msec. Draw an af segment with value zero and duration Segdur[af]-Burdur, starting at Cumdur[af]. Shift Cumdur[af] and Segdur[af] to match, so that Segdur[af] now equals Burdur.*

```
Buramp INC 6 / [-voiced]_
Buramp DEC 6 / [+nasal ; S] [-stress]_
Buramp DEC 3 / [-nasal ; S] [-stress]_
Buramp DEC 3 / _ { [+AXP ; S] / [+fricative ; S] }
Target[af] <- Buramp
Trantype[af] <- DISCON
Target[fnz] <- 450, Bper[fnz] <- 100, Trantype[af..b3] <- SMODIS, Trantype[av] <- DISCON / [+nasal]_
Trantype[av, avc] <- DISSMO / [-voiced ; S] [+nasal]_
```

**Table C-7: Default plosive burst duration**

| seg | Burdur | seg | Burdur | seg | Burdur | seg | Burdur |
|---|---|---|---|---|---|---|---|
| BB | 5 | CH | 15 | DD | 10 | DX | 10 |
| GG | 20 | GP | 20 | JJ | 10 | KK | 25 |
| KP | 25 | MM | 0 | NG | 0 | NN | 0 |
| PP | 5 | TQ | 15 | TT | 15 | | |

#### C.8.4 *Nasals*

```
/[+nasal]_
```

```
Target[fnz] <- 450
Trantype[af..b3] <- SMODIS
```

<!-- book p.198 -->

```
Bper[fnz] <- 100
Trantype[av] <- DISCON
Trantype[avc, av] <- DISSMO / [-voiced ; S] _
```

### C.9 Adjustments

```
/_
```

```
(swap current and previous segments if Manner[current] > Manner[previous])
Bper[f2] <- 75, Bper[f3] <- 75 / [+retro]_
# SH and ZH constrain the boundary strongly unless a stop is adjacent
Bper[f2] <- 20, Tcf[f2] INC 30 / [+palatal,-sonor ; S] _
```

#### C.9.1 *Boundary values for stops*

```
/[+stop ; S] [-stop]_
```

```
Bper[f2] <- 50
Bper[f2] <- 0
Bper[f3] <- 0
Bper[fnz] <- IF Mancur - Manlas > 1 THEN 100 ELSE 0, Tcf[fnz] <- 150, Bper[f1] <- 0 / [+nasal ; S] _
Bper[f2] <- 65, Bper[f3] <- 20 / [+labial ; S] [-glottal,+front]_
Bper[f2] <- 20, Bper[f3] <- 70 / [+labial ; S] [-glottal,-front]_
Oldval[f3] <- 1750, Bper[f3] <- 20 / [+labial ; S] [-glottal,+retro]_
Oldval[f2] <- 1050 / [+alveolar ; S] [+lateral]_
Oldval[f2] <- 1400 / [+NN ; S] [-lateral]_
```

<!-- book p.199 -->

```
Oldval[f2] <- 1600 / [+alveolar,-NN ; S] [-lateral]_
Oldval[f3] <- 2300 / [+alveolar ; S] [+retro]_
Oldval[f3] <- 2620 / [+alveolar ; S] [-retro]_
```

*Note: the velar f2 locus is computed from the vowel's f1 and f2 targets. f1 stands for the lip-rounding part and f2 for the fronting part of the systematic locus shift.*

```
Oldval[f2] <- Target[f2] + (Target[f1] - 300)*2 / [+velar ; S] _
Oldval[f3] <- Oldval[f2] + 800 / [+velar ; S] [+labial]_
Oldval[f3] <- Oldval[f2] + 400 / [+velar ; S] [-labial]_
Oldval[f2,f3] <- KP-target[f2,f4] / { [+front]_ / [+AA]_ }     [sic?] ("f2,f4" as printed)
# f2 lower in NG
Oldval[f2,f3] <- (Oldval[f2,f3] + Target[f2,f3])/2 / [+NG ; S] _
# KK, GG and NG articulated further forward next to IY and IH
Oldval[f2] INC 250, Oldval[f3] INC 50 / { [+IY]_ / [+IH]_ }
(end of manner class reversal)
```

#### C.9.2 *All segments*

```
/_
```

```
Oldval[f2, f3, f4] <- 1200, 2050, 2500, Bper[f4] <- 0, Tcf[f4] <- 30 / [+alveolar,+stop ; S] [+WW]_
Target[f2, f3] <- 1850, 2200, Target[f4] INC f4 / { [+rglide ; S] / [+retro ; S] } [+alveolar]_
Target[f2, f3] <- 1700, 1900 / { [+rglide ; S] / [+retro ; S] } [+velar]_
```

<!-- book p.200 -->

```
Oldval[f2, f3] <- 1100, 1500 / [+velar ; S] [+retro]_
Oldval[f3] <- 2100 / [+alveolar ; S] [+retro]_
Oldval[f3] <- 1700 / [+labial ; S] [+retro]_
Target[f2, f3] <- 900, 2000 / [+wglide ; S] [+velar]_
ELSE Target[f2] <- 1900
Oldval[f2] <- 1850 / [+alveolar,+stop ; S] [+f2back]_
```

### C.10 Modifications

```
/_
```

```
Target[f1..f3] <- average(Target[f1..f3], Oldval[f1..f3], Nextar[f1..f3]) / [+DX]_
# shorter f2 transitions when f2back segments are involved
Tcf[f2] DEC 20 / [+f2back ; S] _
```

*Note: formants must be at least 200 Hz apart. Raise Target[f2..f4] as needed to keep that separation.*

```
# set boundary values and Tcb
Tcb[av..f0] <- Tcf[av..f0]
Bvalf[av..f0] <- (Bper[av..f0] * Target[av..f0] + (100 - Bper[av..f0]) * Oldval[av..f0]) / 100
# discontinuous formant jump at a lateral release
Bvalf[f1,f2] INC 50, Bvalb[f1,f2] DEC 50 / [+lateral ; S] _
```

*Note: DISSMO inside diphthongs is turned into SMOOTH, so the boundary is adjusted instead:*

```
Bvalb[f1..f3] <- Oldval[f1..f3] / [+nasal ; S] [+diphthong]_
# special handling of the amplitude parameters
```

<!-- book p.201 -->

```
Bvalf[av..ab,avc] <- average(Target[av..ab,avc], Oldval[av..ab,avc])
# more gradual vowel amplitude offset
Tcb[av] <- 30 / [+vowel ; S] _
# more gradual voicing offset in a stop or fricative, and in the onset too
Bvalb[av] DEC 6, Bvalf[av] <- Bvalb[av] / [+voiced ; S] [-voiced,-sonor]_
Bvalb[av] DEC 4, Bvalf[av] <- Bvalb[av] / { [+voiced,+stop ; S] / [+voiced,+fric ; S] } [+sonor,+nasal]_
# glottal segments (SIL included) have no inherent "articulatory" targets
Bvalb[a2p..b3] <- Target[a2p..b3], Bvalf[a2p..b3] <- Target[a2p..b3], Oldval[a2p..b3] <- Target[a2p..b3] / [+glottal ; S] _
# stops end abruptly
Bvalf[af] <- Target[af] / [+plosive ; S] _
```

*Note: then draw every parameter track.*

<!-- book p.202 -->

*(Printed p.202 opens Appendix D, sample trials from the Modified Rhyme Test. It is outside Appendix C and is not transcribed here.)*
