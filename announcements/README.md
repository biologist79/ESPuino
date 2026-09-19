# Announcements

Short spoken messages ESPuino can play while music is running — currently only the low-battery
warning. Playback is interrupted for the length of the announcement and then resumes at exactly the
same spot; the web interface and MQTT never see the interruption.

The feature is off by default. Switch it on under **General → Power**, where you also point it at the
file to play.

## Using the files here

1. Copy the file for your language onto the SD card, e.g. to `/ESPuino/announcements/`.
2. In the file browser, right-click it and choose **Use as battery warning** — that fills in the path
   and jumps to the setting. Alternatively type the path into the field yourself.
3. Tick the checkbox and save.

If the file is missing or cannot be played, playback simply carries on untouched and an error is
written to the log.

| File | Voice | Spoken text |
| --- | --- | --- |
| `battery-low_de.mp3` | Anna | „Der Akku ist fast leer. Bitte laden." |
| `battery-low_en.mp3` | Samantha | "The battery is almost empty. Please charge." |
| `battery-low_fr.mp3` | Thomas | « La batterie est presque vide. Merci de la recharger. » |

MP3, mono, 22.05 kHz, 64 kbit/s, around 20 KB each. `.wav`, `.flac`, `.m4a`, `.aac`, `.ogg` and
`.opus` are played as well, so any format you already have is fine — MP3 is simply the cheapest one
for the ESP32 to decode.

## Recording your own

Nothing here is special — any short audio file does. A different voice, your own wording, or a
parent's voice recorded on a phone all work just as well, and for children the last one is usually
the nicest.

These files were generated on macOS with the built-in `say` plus ffmpeg:

```bash
say -v Anna -o /tmp/announcement.aiff "Der Akku ist fast leer. Bitte laden."
ffmpeg -i /tmp/announcement.aiff -codec:a libmp3lame -b:a 64k -ac 1 -ar 22050 battery-low_de.mp3
```

`say -v '?'` lists the available voices. Besides the neutral system voices there are friendlier ones
such as `Grandma` and `Grandpa`, which tend to suit a children's room better.

The second step is worth it: `say` writes stereo at a high bitrate, and mono at 64 kbit/s cuts the
file to roughly a fifth without any audible difference for speech. Without ffmpeg at hand, macOS can
also produce a playable file on its own — `say -o battery-low_de.m4a "..."` writes AAC directly — but
it cannot encode MP3.
