# System sounds

Short feedback sounds for switching the box on and off and for turning the volume.

| File | When |
| --- | --- |
| `startup.wav` | End of boot, rising arpeggio |
| `shutdown.wav` | Before the box goes to sleep (button press, sleep timer, MQTT), falling arpeggio |
| `volume.wav` | One tick per volume step, **only while nothing is playing** |
| `volume-limit.wav` | Volume hit the minimum or maximum (optional; falls back to `volume.wav`) |

## Switching on

Copy the four files to `/ESPuino/sounds/` on the SD card. **A sound is active exactly when its file
exists** — delete a file to silence just that one. Nothing else has to be configured.

A master switch under **General → Options → Play system sounds** (NVS key `systemSounds`, default on)
mutes everything without touching the card.

## Behaviour worth knowing

- With a story running, turning the volume makes no tick. The music itself getting louder or quieter is
  the feedback, and interrupting a track for a click would cost position accuracy.
- On shutdown a running story is paused first. That is the regular pause, so the play position is stored
  exactly as if you had pressed pause before switching off. The box stays powered until the sound ends.
- Volume ticks are rate limited to one per 250 ms, so spinning the knob does not stutter.
- A sound is cut off after 3 seconds, so a broken file can never hold up boot or shutdown.
- Sounds play at the current volume. At volume 0 they are silent.

## Your own sounds

Any short `.wav` or `.mp3` works (keep the names, or change the paths at the top of
`src/SystemSounds.cpp`). Mono, 22.05 kHz, 16 bit is plenty. Leave 50–100 ms of silence at the start and
end of the clip: without it short clips click when the amplifier switches.

`generate_sounds.py` produced the files in this folder; edit its `MELODIES` table to compose your own and
run `python3 generate_sounds.py`.
