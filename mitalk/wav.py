"""16-bit mono WAV output: to a file, to bytes, or straight to the speakers."""

import io
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np


def wav_bytes(samples, rate):
    peak = np.max(np.abs(samples)) if len(samples) else 0.0
    if peak > 0:
        samples = samples / peak * 0.9
    pcm = (samples * 32767).astype("<i2")
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())
    return buf.getvalue()


def write_wav(path, samples, rate):
    with open(path, "wb") as f:
        f.write(wav_bytes(samples, rate))


def play(samples, rate):
    """Play on the default audio device and wait until done.  Uses winsound
    on Windows, else a temporary file handed to aplay / paplay / afplay."""
    data = wav_bytes(samples, rate)
    if sys.platform == "win32":
        import winsound
        winsound.PlaySound(data, winsound.SND_MEMORY)
        return
    fd, path = tempfile.mkstemp(suffix=".wav")
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        for player in (["aplay", "-q"], ["paplay"], ["afplay"]):
            try:
                if subprocess.run(player + [path], capture_output=True).returncode == 0:
                    return
            except FileNotFoundError:
                continue
        raise RuntimeError("no audio player found (aplay, paplay or afplay); use -o file.wav")
    finally:
        os.remove(path)
