"""Front-end checks against transcriptions printed in the book."""

from mitalk import phono
from mitalk.__main__ import text_to_phono2


def test_figure_8_1_phono2():
    # Figure 8-1 PHONO2 output, minus the "* -" morph/syllable boundaries
    # in "rocker" (the dictionary does not record morphemes).
    want = ("F: DH IY C: 'OW LX DD C: MM 'AE NN )N C: SS 'AE DX F: IH NN F: AX "
            "C: RR 'AA KK ER .")
    assert text_to_phono2("The old man sat in a rocker.") == [want]


def test_section_8_7_corrections():
    """Words from the 8.7 sample, as the authors corrected them."""
    def word(w, content=True):
        return " ".join(s for s, _ in phono.word_phones(w, content))
    assert word("messages") == "MM EH SS AX JJ IX ZZ"
    assert word("prerecorded").endswith("IX DD")
    assert word("english").endswith("IX SH")
    assert word("recording").startswith("RR AX KK")
    assert word("recording").endswith("IH NG")
    assert word("automatic") == "AO TT AX MM AE TT IH KK"   # flaps come at 8.5


def test_function_word_forms_from_8_7():
    out = text_to_phono2("The butter was better than milk and bread.")[0]
    assert "F: WW AH ZZ" in out
    assert "F: AE NN C:" in out


def test_flaps_and_word_initial_stops():
    out = text_to_phono2("This recording is a demonstration.")[0]
    assert "AX C: DD \"EH MM" in out          # no flap word-initially
    assert "\"AO DX AX MM 'AE DX IH KK" in text_to_phono2("automatic text")[0]


def test_velar_fronting_and_r_colored_vowels():
    out = text_to_phono2("Give the keen cat a gift by the fire and the bar.")[0]
    for piece in ("GP 'IH VV", "KP 'IY NN", "KP 'AE", "FF 'AY RX", "BB 'AXR"):
        assert piece in out, piece


def test_number_normalization():
    from mitalk.text import normalize
    def say(t):
        return " | ".join(" ".join(s) for s in normalize(t))
    assert say("1,000th") == "one thousandth ."
    assert say("In 1999, he left.") == "In nineteen ninety nine , he left ."
    assert say("$2.01") == "two dollars and one cent ."
    assert say("$1.5 million") == "one point five million dollars ."
    assert say("1990s") == "nineteen nineties ."
    assert say("Is the answer B? Yes.") == "Is the answer B ? | Yes ."
    assert say("9" * 30).count("nine") == 30   # no crash on huge numbers


def test_review_fixes_in_phonology():
    assert "TQ C: RR" in text_to_phono2("Mat ran.")[0]
    assert "TQ EN ZZ" in text_to_phono2("kittens")[0]
    assert "IX ZZ" in text_to_phono2("The judge's dog.")[0]
    assert "AXP" not in text_to_phono2("Judge.")[0]
    assert text_to_phono2("Something happened.")[0].startswith("F:")
