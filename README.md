# BEEP 🔊

ESP32 + MAX98357A I2S amplifier + speaker — from a simple **beep test** to a device that **speaks "Help!"** with a real voice stored on the ESP32 itself (no SD card, no MP3 module).

Built for the **VocalBridge** project (EMG muscle-sensor wearable that lets a user call for help with a muscle squeeze → the device speaks).

## Quick start

1. **Wire it** (5 wires + speaker):

   | MAX98357A | ESP32 |
   |---|---|
   | VIN | VIN (5V) |
   | GND | GND |
   | BCLK | GPIO27 |
   | LRC | GPIO26 |
   | DIN | GPIO25 |
   | + / − | speaker wires |

2. Open a sketch **from inside its folder** (Arduino IDE rule) → Board: **ESP32 Dev Module** → Upload.
3. No libraries needed. Works on esp32 core v2.x and v3.x (auto-detected).

## The sketches (in the order you'd use them)

| Folder | What it does |
|---|---|
| `SpeakerTest_MAX98357A/` | **Start here.** Clean 256-step sine chime (beep-beep-beeeep) every 3 s — proves the wiring. Commands: `b` beep, `+`/`-` volume |
| `SpeakerWireDoctor/` | Wiring broken? It cycles 6 BCLK/LRC/DIN pin arrangements **in software** (press `m`, listen) — names your actual wiring so you don't have to rewire to diagnose |
| `SpeakerSayHelp_MAX98357A/` | **The goal.** Says **"Help!"** 2 s after boot — the voice is a real audio clip compiled into the ESP32 (`help_sound.h`). Commands: `h` say it, `2` twice, `l` loop (great for filming), `+`/`-` volume, `b` beep |
| `SpeakerTest_DFPlayer/` | Same tests for a DFPlayer Mini + microSD setup (alternate hardware plan) |
| `ClenchDetector_v12_SPEAKER/` | Clench detector (v11 state machine) + DFPlayer "Help" — full demo for the DFPlayer path |
| `SpeakerTest_Tone_Amp/` | Beep test for plain analog amps (PAM8403/LM386). ⚠️ NOT for MAX98357A (digital-only input) |

## Put your own voice in it

```bash
# record "Help!", save as audio/help.wav (mono WAV), then:
python3 make_help_header.py      # regenerates help_sound.h
# re-upload the SpeakerSayHelp sketch
```

The script trims silence, normalizes loudness, and writes the C array. `audio/help_clean.wav` is a preview of exactly what the device says.

## Docs

- **`SPEAKER_TEST_GUIDE.md`** — full wiring diagrams, troubleshooting table (including the "buzz splitter test" for finding dead wires), and the EMG noise rules
- **`PROGRESS_LOG.md`** — project state and next steps

## Notes learned the hard way

- **Volume:** keep it ~10–13 on the default 9 dB gain — higher clips into fuzz on a small speaker
- **I2S needs all 3 signal wires at once** — testing one wire at a time just amplifies noise (sounds like a broken buzzer; that's normal)
- **Solder the joints** for the final build — a loose speaker twist-joint sounds like a wiring fault
- Speaker wires stay **away from EMG electrode wires** — speaker current can inject noise into the sensor
