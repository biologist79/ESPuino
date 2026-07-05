# RC522 constant-presence detection: root cause & fix

Context: In `pauseIfRfidRemoved` mode, playback would flap (pause/resume) while a
card sat perfectly still on the reader, and card-removal detection was unreliable
on RC522 clones. During debugging we first tried papering over this with a growing
"consecutive misses" debounce (3 → 8 cycles ≈ 400 ms). That treats the symptom.
This note records the actual root cause and the fix.

## Root cause: REQA never reliably re-detects a stationary card

The old presence loop polled with `PICC_IsNewCardPresent()`. Under the hood that
sends **REQA (`0x26`)**. From the MFRC522 library's own header:

| Command | Opcode | Wakes cards in state… |
|---------|--------|-----------------------|
| `REQA`  | `0x26` | **IDLE only** ("Sleeping cards in state HALT are ignored") |
| `WUPA`  | `0x52` | **IDLE and HALT** |
| `HLTA`  | `0x50` | moves an ACTIVE card → HALT |

The ISO-14443A state machine is: `IDLE → (REQA/WUPA) → READY → (Select) → ACTIVE → (HLTA) → HALT`.

In `pauseIfRfidRemoved` mode the old code deliberately did **not** `HaltA()` after
reading the card (`RfidMfrc522.cpp`, the `if (!gPlayProperties.pauseIfRfidRemoved)`
guard around `PICC_HaltA()`). So the card was left in the **ACTIVE** state — and an
ACTIVE card does not answer REQA. The loop then relied on a block of "voodoo"
(from miguelbalboa/rfid issue #188) that re-ran `PICC_ReadCardSerial()`
(anticollision + Select) on retries and checked for a magic `control == 13 || 14`.

Whether that re-Select succeeds on any given poll depends on which state the card
(and a sloppy clone's state machine) happens to be in at that instant. That is the
coin-flip we were seeing. Requiring N consecutive misses only hides dropped polls,
at the cost of N × ~50 ms of removal latency — it can never make the underlying
poll deterministic.

## The fix: keep the card in HALT, poll with WUPA

Park the card in a **known** state (HALT) after every read, and poll with the one
command that wakes a HALTed card. Each poll becomes a clean yes/no.

```cpp
template <typename Reader>
static bool RfidMfrc522_CardStillPresent(Reader &reader) {
    byte bufferATQA[2];
    byte bufferSize = sizeof(bufferATQA);
    // mirror PICC_IsNewCardPresent()'s register reset; some readers need it
    reader.PCD_WriteRegister(Reader::TxModeReg, 0x00);
    reader.PCD_WriteRegister(Reader::RxModeReg, 0x00);
    reader.PCD_WriteRegister(Reader::ModWidthReg, 0x26);
    auto result = reader.PICC_WakeupA(bufferATQA, &bufferSize); // WUPA wakes HALT
    reader.PICC_HaltA();                                        // straight back to HALT
    return (result == Reader::STATUS_OK || result == Reader::STATUS_COLLISION);
}
```

Companion change: in `pauseIfRfidRemoved` mode we now `PICC_HaltA()` immediately
after the initial read, so the card enters the poll loop already in HALT. The
`control`/voodoo block and the 8-cycle debounce are gone; the loop is just:

```cpp
while (RfidMfrc522_CardStillPresent(reader) || ++misses < removalDebounceCycles) { … }
```

`removalDebounceCycles` is kept at **2** (≈100 ms) purely to swallow the rare
genuinely-dropped poll from RF noise. Because each WUPA poll is now deterministic,
this can be dropped to **1** to test raw reliability with zero miss tolerance.

### Template note (both reader types)

`RfidMfrc522_TaskImpl` is instantiated for both the SPI `MFRC522` class and the
I2C `MFRC522_I2C` class. Those two libraries differ in ways that matter here:

- Register addresses: SPI values are `<< 1` shifted (`TxModeReg = 0x24`), I2C are
  raw (`TxModeReg = 0x12`).
- `StatusCode`: SPI `STATUS_OK = 0`, I2C `STATUS_OK = 1`.
- `PICC_WakeupA` returns `MFRC522::StatusCode` (SPI) vs `byte` (I2C).

Using `Reader::TxModeReg` / `Reader::STATUS_OK` and `auto result` lets the compiler
pick the correct constants and types per instantiation, so one helper serves both.

## Verified

`pio run -e esp32-s3-devkitc-1` — recompiles `RfidMfrc522.cpp` and links the full
firmware **SUCCESS**. That env defines `RFID_READER_TYPE_RUNTIME` and
`I2C_2_ENABLE`, so both the SPI and I2C template instantiations are exercised.

(Note: `-e lolin_d32_pro_sdmmc_pe`, the repo default env, fails to build on the
current machine for **unrelated, pre-existing** reasons — a FastLED
`addLeds<CHIPSET, LED_PIN, COLOR_ORDER>` version mismatch in `Led.cpp` and
undeclared `I2S_BCLK/LRC/DOUT` pins in `AudioPlayer.cpp`, i.e. that env's per-user
pin config isn't present here. Neither involves `RfidMfrc522.cpp`.)

## If it's still unreliable after this — secondary avenues

These are hardware/tuning, in rough order of effort-to-payoff:

1. **Antenna gain is at max (`mfrc522Gain` default `0x07`, ~48 dB).** For a card
   sitting *directly* on the coil, max gain can saturate the receiver and garble
   the ATQA. Counterintuitively, try `0x04`–`0x05`. Quick test via the `mfrc522Gain`
   NVS pref.
2. **Power decoupling.** RC522 clones brown out on the RF transmit burst. Add a
   10–100 µF cap across the module's 3V3/GND and make sure the 3.3 V rail is solid.
   Many "random" dropped polls are actually supply dips.
3. **Hardware swap — PN532.** If the clone is simply bad, no firmware saves it. The
   PN532 has proper in-field target detection (vs. repeated REQA/WUPA polling) and
   is the reliable choice for continuous-presence use; several Tonuino/Toniebox-style
   builds moved to it for exactly this reason.

## Live test results (2026-07-05, ESP32-S3 + RC522 clone, SPI, version reg 0x82)

Bench-tested for ~2 h on real hardware in `pauseIfRfidRemoved` mode with an
audiobook playlist and multiple Mifare cards:

- **Protocol fix confirmed.** No pause/resume flapping while a card sits
  stationary on the reader; verified stretches of 38+ s continuous presence
  with playback progressing. Genuine removals are detected in 100–400 ms
  (2-cycle debounce) — down from the 400 ms+ the old 8-cycle debounce needed
  just to stay stable.
- **Secondary avenue #1 (antenna gain) also confirmed.** With the default
  `mfrc522Gain` (7 = 48 dB max), a card lying *directly on* the reader coil
  got a phantom removal 100–400 ms after every detect (receiver saturation) —
  log signature: `RFID-tag removed` → immediate pause at position 0. The same
  card at ~3 cm was rock solid. Setting `mfrc522Gain = 5` fixed contact
  distance completely. Reminder: the pref is only read in `RfidMfrc522_Init`,
  so it needs a reboot to apply.
- Side effect worth knowing: with max gain those phantom removals paused
  playback instantly after a card was applied, which presents as "the box
  doesn't start playing" — an audio-looking symptom whose actual cause is RF.

## Optional hardening (not yet done)

`WUPA` confirms *a* card is present, not that it's the *same* UID — a card swapped
during playback would read as "still present". If that matters, change the poll to
`WUPA → PICC_Select → compare UID → HaltA`. Slightly slower, still deterministic.
