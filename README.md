# Final_V1 🗣️

**VocalBridge's voice — the user's own recordings, stored on the ESP32 itself.** No SD card, no MP3 module: the audio is compiled into the sketch as C arrays and played through the MAX98357A I2S amplifier.

## Words included

| Word | File | Length | Source |
|---|---|---|---|
| **HELP** | `SpeakerSayWords_MAX98357A/help_voice.h` | 1.53 s (×2) | user's re-recording (HELP_V3 take, 26 Sep — louder, 71% peak) + **P-burst surgery v3 = v2 params on the new take** (the new take's /p/ was also unreleased — L faded to noise floor by 1.23 s with only an 8% blip at 1.35 s; surgery: gentle L fade 25 ms + 65 ms closure + broadband 12 ms pop 300–5500 Hz @45% of word peak + 70 ms aspiration 300–1500 Hz @15% — audio/help_own_v5_pfix3.wav) |
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

- `audio/preview_help_pfix3_natural.wav` — HELP from the HELP_V3 take + pop surgery (current)
- `audio/preview_help_v3take_natural.wav` — the HELP_V3 take processed, natural ending (no surgery) — for A/B comparison
- `audio/preview_water_sharp_natural.wav` — WATER, T-sharpened (current)
- `audio/master_help_device.wav` / `audio/master_water_device.wav` — exactly what the speaker plays
- `audio/help_own_v5_pfix3.wav` — the surgered HELP word (single pass, pre-header)
- older takes/surgeries kept for history (`help_own_v2/v3_pfix/v4_pfix2`, `water_own/_v2_sharp`)
