#!/usr/bin/env python3
"""Generates the ESPuino system sounds (startup, shutdown, volume, volume-limit) as small WAV files.

Only the standard library is needed:  python3 generate_sounds.py

Edit the MELODIES table below for your own tunes. Notes are (frequency in Hz, length in ms); 0 Hz is a
rest. Every note gets a soft attack and a decaying tail, and each file gets a little silence in front
and behind: short mono clips otherwise click when the amplifier switches on and off.
"""
import math
import struct
import wave

RATE = 22050
PAD_MS = 150  # silence before and after, hides the click of the I2S amplifier and keeps the first note audible
LEVEL = 0.6  # peak level, 1.0 = full scale; the box volume does the rest

# Do-mi-sol-do, the simplest tune there is: easy to recognise and easy to sing along to.
C4, E4, G4, C5, E5 = 261.63, 329.63, 392.00, 523.25, 659.25

MELODIES = {
    # do-mi-sol-do upwards, even and bouncy: "hello, I am awake"
    "startup.wav": [(C4, 170), (E4, 170), (G4, 170), (C5, 600)],
    # the same notes downwards, a little calmer: "good night"
    "shutdown.wav": [(C5, 200), (G4, 200), (E4, 200), (C4, 700)],
    # one soft tick per volume step
    "volume.wav": [(G4, 55)],
    # two lower ticks: "that is as far as it goes"
    "volume-limit.wav": [(C4, 55), (0, 40), (C4, 55)],
}


def note(freq, ms):
    n = int(RATE * ms / 1000)
    attack = max(1, int(RATE * 0.006))
    out = []
    for i in range(n):
        if freq == 0:
            out.append(0.0)
            continue
        t = i / RATE
        env = min(1.0, i / attack) * math.exp(-1.6 * i / n) * min(1.0, (n - i) / attack)
        # a sine plus a short-lived bright partial gives a music-box tone
        bell = math.exp(-6.0 * i / n)  # the bright partial fades first, like a xylophone or music box
        out.append(env * (math.sin(2 * math.pi * freq * t) + 0.45 * bell * math.sin(6 * math.pi * freq * t)) / 1.45)
    return out


def render(notes):
    pad = [0.0] * int(RATE * PAD_MS / 1000)
    samples = list(pad)
    for freq, ms in notes:
        samples += note(freq, ms)
    samples += pad
    return struct.pack("<%dh" % len(samples), *(int(max(-1.0, min(1.0, s * LEVEL)) * 32767) for s in samples))


for name, notes in MELODIES.items():
    with wave.open(name, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(render(notes))
    print("wrote", name)
