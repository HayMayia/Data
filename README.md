# Final_V1 🗣️

**VocalBridge's voice — the user's own recordings, stored on the ESP32 itself.** No SD card, no MP3 module: the audio is compiled into the sketch as C arrays and played through the MAX98357A I2S amplifier.

## Words included

| Word | File | Length | Source |
|---|---|---|---|
| **HELP** | `SpeakerSayWords_MAX98357A/help_voice.h` | 1.62 s (×2) | user's recording + **P-burst surgery v2** (final /p/ was unreleased → "HELL"; v1 graft was too quiet/narrow; v2 = gentle L fade + 65 ms closure + broadband 12 ms pop @45% + 70 ms aspiration — audio/help_own_v4_pfix2.wav) |
| **WATER** | `SpeakerSayWords_MAX98357A/water_voice.h` | 1.65 s (×2) | user's recording + **T-sharpening** (the take had a ~95 ms dark retroflex-T gap + 155 ms rolled-R tail = muffled sound; surgery: 45 ms closure + bright 16 ms alveolar click @30% + R tail trimmed 80 ms — audio/water_own_v2_sharp.wav) |

Both are processed with the same chain: silence trim → telephone-band clarity (250 Hz HPF, +6 dB @ 2.2 kHz presence, 6 kHz LPF) → gentle loudness (12 dB, soft knee) → said twice (220 ms gap).

## Use

1. Download/clone, open `SpeakerSayWords_MAX98357A/SpeakerSayWords_MAX98357A.ino` in Arduino IDE
2. Board: ESP32 Dev Module → Upload (no libraries needed)
3. Serial Monitor @ 115200: `h` = HELP · `w` = WATER · `b` = beep · `l` = loop · `+`/`-` = volume

Wiring: MAX98357A VIN→VIN(5V), GND→GND, BCLK→GPIO27, LRC→GPIO26, DIN→GPIO25; speaker on +/− (soldered!).

## Add more words (YES / NO / PAIN ...)

```bash
python3 make_word_header.py your_recording.wav YES
# put YES_voice.h in the sketch folder, add #include + one sayYES() block
```

## Audio previews

- `audio/preview_help_own_natural.wav` / `audio/preview_water_own_natural.wav` — natural, for computer listening
- `audio/master_help_device.wav` / `audio/master_water_device.wav` — exactly what the speaker plays
- `audio/help_own_v2.wav` / `audio/water_own.wav` — the original takes (keep as masters)
