MITalk -- command-line text-to-speech
=====================================

A recreation of the MIT text-to-speech system described in Allen, Hunnicutt
& Klatt, "From Text to Speech: The MITalk System" (Cambridge University
Press, 1987), with Dennis Klatt's 1980 cascade/parallel formant synthesizer.

Usage
-----

  mitalk "The old man sat in a rocker."            speak on the default audio device
  mitalk "The old man sat in a rocker." -o out.wav write a 10 kHz WAV file instead
  mitalk -v "Hello there."                         also print the transcription and durations
  mitalk -p "F: DH IY C: 'OW LX DD ."              input in the book's phonemic notation

Options:
  -p        input is the book's phonemic notation (Table 8-1)
  -o FILE   write a WAV file instead of playing
  -v        print the PHONO2 transcription and segment durations
  -d FILE   use another lexicon.tsv (default: the one next to the program)
  --        treat the next argument as text even if it starts with '-'

Keep lexicon.tsv in the same folder as the program.

Files
-----

  mitalk(.exe)        the program
  lexicon.tsv         pronunciation dictionary: phonemes from mitalk.100k.dict
                      with stress from the CMU Pronouncing Dictionary
  cmudict.LICENSE     license of the CMU Pronouncing Dictionary
