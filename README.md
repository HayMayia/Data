# Final_V1 🗣️

**VocalBridge's voice — the user's own recordings, stored on the ESP32 itself.** No SD card, no MP3 module: the audio is compiled into the sketch as C arrays and played through the MAX98357A I2S amplifier.

## Words included

| Word | File | Length | Source |
|---|---|---|---|
| **HELP** | `SpeakerSayWords_MAX98357A/help_voice.h` | 0.66 s | user's re-recording (HELP_V3 take, 26 Sep) — **natural, as recorded** (no surgery: user directive 26 Sep — "don't alter a thing") |
| **WATER** | `SpeakerSayWords_MAX98357A/water_voice.h` | 0.93 s | user's original take (WATER.m4a) — **natural, as recorded** (T-sharpening removed per user directive 26 Sep) |
| **YES** | `SpeakerSayWords_MAX98357A/yes_voice.h` | 0.90 s | user's recording (YES.m4a, 28 Sep) — **natural, as recorded** |
| **NO** | `SpeakerSayWords_MAX98357A/no_voice.h` | 0.83 s | user's recording (NO.m4a, 28 Sep) — **natural, as recorded** |

PAIN is **not** on the device — it is reserved for the Phase-2 over-the-air update demo (see ROADMAP.md).

All four are built **natural** (`--natural` mode, added 26 Sep): the recording stays as spoken — silence trimmed, volume matched, nothing re-synthesized. Each word is said **once** per key press (the ×2 repeat was removed 28 Sep on request; the audio samples themselves are unchanged).

## Use

1. Download/clone, open `SpeakerSayWords_MAX98357A/SpeakerSayWords_MAX98357A.ino` in Arduino IDE
2. Board: ESP32 Dev Module → Upload (no libraries needed)
3. Serial Monitor @ 115200: `h` = HELP · `w` = WATER · `y` = YES · `n` = NO · `b` = beep · `l` = loop · `+`/`-` = volume

Wiring: MAX98357A VIN→VIN(5V), GND→GND, BCLK→GPIO27, LRC→GPIO26, DIN→GPIO25; speaker on +/− (soldered!).

## Add more words (YES / NO / PAIN ...)

```bash
python3 make_word_header.py your_recording.wav YES --natural
# put YES_voice.h in the sketch folder, add #include + one sayYES() block
```

## Audio previews

- `audio/preview_help_natural.wav` / `audio/preview_water_natural.wav` / `audio/preview_yes_natural.wav` / `audio/preview_no_natural.wav` — current builds: as recorded, natural (computer listening)
- `audio/master_help_device.wav` / `master_water_device.wav` / `master_yes_device.wav` / `master_no_device.wav` — exactly what the speaker plays (word only, no beep intro)
- earlier surgery experiments kept for history only, not in the build (`help_own_v4_pfix2`, `help_own_v5_pfix3`, `water_own_v2_sharp`, previews)
