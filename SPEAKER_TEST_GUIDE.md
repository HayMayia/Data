# VocalBridge — SPEAKER TEST (tonight's mission)

**Where we are:** 4095 fixed ✅ · clench detection works (v10/v11) ✅ · exams next week, so
this is the last hardware session for a few days. Tonight = **make the speaker talk**.
Target time: **10 minutes.**

---

## ⭐ YOUR hardware (from your own Amazon order)

You bought (confirmed in our earlier chat):
- **Driver: MAX98357A I2S amplifier breakout** — a ~2 cm board with pins
  **VIN GND BCLK LRC DIN** (+ maybe SD, GAIN). **No microSD slot.**
- **Speaker: 2″ 4Ω 5W** — bare metal speaker with two wires.

→ **Your test file is `SpeakerTest_MAX98357A.ino`. Skip to PLAN A below.**

The other plans stay in this guide just in case (e.g. if you later buy a DFPlayer Mini).

---

## Step 0 — Not sure which board you have? (10 seconds)

| The board… | It is a… | Test file |
|---|---|---|
| Pins say **BCLK / LRC / DIN**, no SD slot, chip marked **MAX98357A** | **I2S amplifier** ← yours | **`SpeakerTest_MAX98357A.ino`** |
| Has a **microSD slot**, pins VCC GND RX TX SPK_1 SPK_2 | DFPlayer Mini (MP3 player) | `SpeakerTest_DFPlayer.ino` |
| No SD slot, just an amp chip + **analog input** (IN+/IN−) | Plain amp (PAM8403 / LM386) | `SpeakerTest_Tone_Amp.ino` |

⚠️ The MAX98357A and a PAM8403 are **NOT interchangeable** — the MAX98357A has a
**digital input only**. A plain GPIO beep test gives silence; it needs the I2S code in Plan A.

---

# PLAN A — MAX98357A + speaker (YOUR board) — 10 min

## Step 1 — Wiring (3 min)

**Unplug USB while wiring.**

| MAX98357A pin | Goes to | Note |
|---|---|---|
| VIN | **VIN (5V)** on ESP32 | louder than 3V3 |
| GND | GND | common ground |
| BCLK | **GPIO27** | bit clock |
| LRC | **GPIO26** | left/right clock |
| DIN | **GPIO25** | the actual audio data |
| SD | leave unconnected | enabled by default; if total silence, try SD → 3V3 |
| GAIN | leave unconnected | default gain is fine |
| **+ / −** output | **speaker wires** | solder the two speaker wires to the + and − holes |

```
 ESP32                        MAX98357A
 ─────                        ─────────
 VIN 5V ────────────────────► VIN
 GND ────────────────────────► GND
 GPIO27 ────────────────────► BCLK
 GPIO26 ────────────────────► LRC
 GPIO25 ────────────────────► DIN
                                 │
                              +  ┴  −  ◄── speaker wires (either way round,
                                            but + to + if marked)
```

These pins **don't clash with the muscle sensor** (AD8232 on GPIO34, LO+ 32, LO− 33),
so you can leave the sensor wired.

## Step 2 — Upload & listen

1. Open `SpeakerTest_MAX98357A.ino` → Board: **ESP32 Dev Module** → Upload.
   - No library needed. Works on esp32 package v2.x and v3.x.
2. Open Serial Monitor at **115200** (optional).
3. **2 seconds after boot you should hear: beep-beep-beeeep**, repeating every 3 s.
4. Commands: `b` = beep now · `+` / `−` = volume · `h` = help.

**Hear the chime? Your speaker + driver work. Done for tonight. 📚**

---

# ⭐ PLAN A+ — make it SPEAK "Help!" (the real thing)

Once the chime works, open **`SpeakerSayHelp_MAX98357A/SpeakerSayHelp_MAX98357A.ino`**
(same wiring, nothing changes) and upload it.

- The "Help!" **voice is stored inside the ESP32 itself** (`help_sound.h`) —
  no SD card, no DFPlayer, fully standalone. This is why your MAX98357A was a good buy.
- 2 s after boot it **says "Help!"** out loud.
- Serial commands (115200):

| Send | Does | Send | Does |
|---|---|---|---|
| `h` | say "Help!" | `2` | say it twice |
| `l` | loop every 3 s (great for filming) | `b` | beep chime |
| `+` / `−` | volume | `i` | clip info |

### Put YOUR OWN voice in it (optional, 5 min)
The built-in clip is an AI voice (you picked it in chat). To use your own:
1. Record "Help!" on your phone, get it as a **mono WAV** file.
2. Replace `audio/help.wav` with your recording.
3. Run `python3 make_help_header.py` (included) → re-upload the sketch.
   Or upload your recording to our chat and I'll convert it for you.

---

# 🔊 Make it LOUDER and CLEARER

**Software (v3 — current):** `help_sound.h` on the `beep` branch = voice-02 saying
**"Help! Help, please!"** — the most human take of 3 candidates (highest measured
energy + intonation motion; serious but not monotone). 15% slower (pitch-preserved),
clarity EQ (170 Hz HPF + presence), RMS **−8.8 dBFS** (v2 was −9.7). 1.03 s long.
Preview: `audio/help_enhanced.wav`. Alternative serious single-word take saved at
`audio/help_alt_serious.wav` — swap it in as `audio/help.wav` and re-run the script.
To re-tune: `STRETCH` / `GAIN_DB` / `KNEE` at the top of `make_help_header.py`.

**Hardware GAIN pin (free +3 dB):** one extra wire on the MAX98357A —

| GAIN pin | Amp gain | vs default |
|---|---|---|
| not connected (default) | 9 dB | — |
| **→ GND** | **12 dB** | **+3 dB (louder!)** |
| → VIN | 6 dB | −3 dB (quieter) |

Wire **GAIN → GND** for maximum loudness. If the voice then sounds harsh at volume 21,
drop to 18 — that's the amp's honest limit, not a fault.

**Power check:** the amp's VIN must be on the ESP32's **VIN (5V)** pin — on 3V3 it's
permanently ~3.5 dB quieter.

**Acoustic trick (free, big):** a bare speaker cancels itself — front wave meets back
wave. Mount it in ANY small box/cup with a speaker-sized hole (even a matchbox or
plastic bottle cap ring) and it gets dramatically louder AND clearer. Best free
upgrade there is.

---

## Troubleshooting (Plan A)

| Symptom | Likely cause | Fix |
|---|---|---|
| **Raspy buzzy noise** instead of sound | **I2S wires partially connected** (e.g. only BCLK in, LRC/DIN floating), or DIN on the wrong GPIO | I2S needs **all 3 signal wires at once**: BCLK→27, LRC→26, DIN→25. Never test one wire at a time — the amp just amplifies noise from floating pins |
| **Re-soldered the amp — still garbled** | Suspects left: the speaker itself, amp overdrive (clipping), or a dud amp board | ① **battery pop test**: speaker wires on a 1.5V AA, on/off — clean pop = speaker OK, scratchy/silent = speaker is the fault ② **volume test**: send `-`×5 — garble shrinks = clipping → volume ≤8 and/or **GAIN→VIN** (0 dB) ③ re-upload sketch (now uses the precision audio clock/APLL with auto-fallback) ④ still bad with speaker OK → replace the amp board |
| **Re-uploading the 256-step sine changed nothing** | The garble is **physical**, not code — a marginal joint/wire corrupts any waveform the same way. | Run the **wiggle test**: while the chime plays, wiggle each of the 5 wires + pinch the speaker twist, ONE at a time. The one that changes the sound is the fault. Then solder it. |
| **Tune recognizable but raspy/fuzzy, all the time** | ① old sketch's crude 16-step chime (fixed → re-upload the updated sketch, now a clean 256-step tone) ② clipping — volume too high for the amp's default 9dB gain (send `-` a few times, target ~10-13) ③ speaker twist-joint crackle (pinch test) | Re-upload sketch → volume down → pinch test, in that order |
| **Wire Doctor: all 6 mappings silent/garbled** | A **physical** fault — all mappings share the same 3 wires, so one dead jumper/bad contact kills all 6. Suspects in order: LRC jumper (history!), speaker twist-joint, power jumper, unsoldered amp header | Run the **buzz splitter test** (below) |

**Buzz splitter test** (when all Wire Doctor mappings fail): with only BCLK + DIN connected
(LRC jumper pulled off the ESP32), power on. **Buzz/garble = power, amp, speaker, BCLK,
DIN all fine — the LRC path is the fault.** Fix order: ① swap the LRC jumper for a fresh
wire (into GPIO26, reboot — boots on M1). ② still garbling? Move it to **GPIO33** and set
`PIN_LRC = 33` in the sketch (GPIO26 may be damaged). ③ still garbling? Re-solder the
amp's LRC header pin (cold joint). Silence (no buzz) on M1 with LRC dangling = power or
speaker joint moved instead — re-twist speaker, re-seat VIN/GND.
| Buzz appears only sometimes | floating pin picking up noise / loose jumper | Press all jumpers firm; wire everything before powering on |
| Total silence, no errors | BCLK/LRC swapped | Swap GPIO26/GPIO27 wires — #1 mistake |
| | DIN wrong pin | DIN must be GPIO25 |
| | amp disabled | Jumper **SD → 3V3** (some clone boards default to off) |
| | speaker not attached | Solder speaker wires to + / − (loose jumpers don't carry audio well) |
| | volume at 0 | Send `+` a few times |
| | **wrong sketch running!** | `SpeakerSayHelp` plays ONCE 2 s after boot, then waits for `h` — sounds like silence! Use `SpeakerTest_MAX98357A` (chime every 3 s) while debugging wiring |
| Very faint | powered from 3V3 | Move VIN to the ESP32's **VIN (5V)** pin |
| | quiet by default | Send `+` up to 21; keep it moderate — it's a small speaker |
| Compile error mentioning `driver/i2s.h` or `ESP_I2S.h` | old/new esp32 package | Boards Manager → update **esp32 by Espressif** to the latest |
| Crackly / buzzy with all wires in | DIN on wrong GPIO (amplifying garbage data) | Check DIN = GPIO25 first |
| ESP32 reboots on loud beeps | power spike | Lower volume; use a good USB port/cable |

**Rules while testing:** unplug USB before changing wires → connect **all 5** (VIN, GND, BCLK, LRC, DIN) → re-plug → listen. Hot-plugging I2S wires while powered confuses the amp.

**EMG noise rule:** keep speaker wires away from the electrode wires, never let them
cross the pads. Speaker current can inject noise into the AD8232.

---

# PLAN B — DFPlayer Mini + speaker (only if you buy one later)

1. microSD ≤ 32 GB, **FAT32**, folder `mp3`, file `0001.mp3` = someone saying "Help"
   (record with an MP3 recorder app — renaming `.m4a` does **not** work).
2. Wiring: VCC→VIN(5V), GND→GND, DFPlayer **RX→GPIO26**, **TX→GPIO27** (crossed!),
   speaker→**SPK_1 + SPK_2** (never to GND).
3. Install library **DFRobotDFPlayerMini**, upload `SpeakerTest_DFPlayer.ino`,
   Serial Monitor 115200 → it plays "Help" 2 s after boot, then takes commands
   (`1`–`9` play, `+`/`−` volume, `n`/`b` track, `s` stop, `x` root-file fallback).
4. `ClenchDetector_v12_SPEAKER.ino` = clench detector + DFPlayer "Help" — the
   full demo **for this plan**.

---

# PLAN C — plain amplifier board (PAM8403 / LM386, analog input)

1. Amp VCC→VIN(5V), GND→GND, **IN+→GPIO25**, IN−→GND, speaker→amp output.
2. Upload `SpeakerTest_Tone_Amp.ino` → hear **beep-beep-beeeep** forever.
3. ⚠️ Do **not** use this plan for a MAX98357A — it has no analog input.

---

# Next week, when you're back

Your board is the MAX98357A, so the "speaker says Help" demo uses the ESP32's
digital audio (no SD card, no DFPlayer needed — actually cleaner!):

1. **Fresh electrode pads** (the old ones dried out), arm flat, sit still 5 s → `>>> ARMED`.
2. Squeeze hard 2–3 s → screen says `>>> CLENCH! <<<` **and the speaker speaks**.
3. The speaking half is DONE — `SpeakerSayHelp_MAX98357A` already says "Help!".
   What's left is **v13** = clench detector + that same voice in one sketch
   (squeeze → "Help!"). Say the word and it's ready.
4. Film it — that's the complete standalone Phase-1 demo.

Good luck with SST — the sensor isn't going anywhere. 🍀

---

## Folder layout (why each sketch has its own folder)

Arduino IDE merges **all** `.ino` files in one folder into a single sketch, so every
sketch lives in its own subfolder — always open the `.ino` **from inside its folder**:

```
vocalbridge-speaker/
├── SPEAKER_TEST_GUIDE.md            ← start here
├── PROGRESS_LOG.md
├── make_help_header.py              ← put your own voice in the ESP32
├── SpeakerTest_MAX98357A/           ← Plan A: beep test (wiring check)
├── SpeakerWireDoctor/               ← finds wiring mistakes WITHOUT rewiring (press m)
├── SpeakerSayHelp_MAX98357A/        ← Plan A+: SPEAKS "Help!" (voice on-chip)
│   └── help_sound.h                 ← the embedded audio (auto-generated)
├── SpeakerTest_DFPlayer/            ← Plan B (only if you buy a DFPlayer)
├── ClenchDetector_v12_SPEAKER/      ← Plan B demo (clench + DFPlayer)
├── SpeakerTest_Tone_Amp/            ← Plan C (plain analog amps only)
└── audio/                           ← help.wav source + cleaned clip
```
