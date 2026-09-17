# VocalBridge — Progress Log (continuation)

Updated: night of 5 Sep 2026 (speaker session, last one before exam week).

## 🔧 CURRENT DEBUGGING STATE (speaker wiring)
- LRC path FIXED (tone changes audible = BCLK/LRC/DIN all decoding). Which fix worked
  (fresh wire vs GPIO33) — still to be confirmed by user.
- Constant raspy/garbled sound **unchanged by the 256-step sine upgrade** → garble is
  PHYSICAL (marginal joint), not code. Suspect #1: amp header pins (possibly unsoldered
  from factory, like their AD8232 was).
- **NOW: user re-soldered the driver board — NO CHANGE.** Suspects remaining:
  ① speaker itself (battery pop test: wires on a 1.5V AA, clean pop = OK)
  ② amp clipping (volume `-`×5 test; fix = volume ≤8 or GAIN→VIN = 0dB)
  ③ dud amp clone (if ① ② pass → replace board)
- **✅ SOLVED (5 Sep, late): EXCHANGED BOARD WORKS.** Clean chime on SpeakerTest.
  The original unit was defective (lemon) — diagnosis chain (LRC isolation → re-solder
  → code fixes → exchange) validated. **"Help!" voice CONFIRMED WORKING too.**
- **Audio tuning (5 Sep, night):** user wanted louder + crisper →
  ① help_sound.h rebuilt as LOUD+CRISP edition (+4.7 dB loudness, HPF 170 Hz,
  presence +4.5 dB @ 2.6 kHz, soft limiter; same voice take). make_help_header.py
  now applies this chain automatically. Preview: audio/help_enhanced.wav.
  ② advised GAIN→GND wire (+3 dB) + VIN-on-5V check + speaker-in-a-box acoustic tip.
- ⚠️ IMPORTANT: the code pasted in chat (Sep 5) predates the APLL patch. For the new
  board, download the latest sketches from the `beep` branch on GitHub.
- Older context: garble with LRC dangling + all 6 Wire Doctor mappings failing →
  buzz splitter test isolated the fault to the LRC path (power/amp/speaker/BCLK/DIN
  were all confirmed good).
- When GPIO33 is taken by LRC, the muscle sensor's LO− moves to GPIO35 later.
- **Audio v5 (mid-Sep):** user said v4 "still very very fast" → STRETCH 2.3→3.0
  (2.64 s, 63,338 samples, RMS −10.9 dBFS). ⚠️ Clarified to user: speed is baked into
  help_sound.h, NOT a sketch setting — if device still sounds fast after this, the old
  header is still in the sketch folder (verify: open help_sound.h in Notepad, check
  the duration on line 2; delete old copies; re-upload).
- **Status (mid-Sep):** audio system DONE (loud+crisp "Help!" confirmed). Exams nearly
  over (only Computers left). Next session: fresh pads → v13 (clench → "Help!") → film.


## 📍 Where the project stands

### ✅ DONE (previous sessions)
- **4095 saga solved** — root cause was floating OUTPUT wire / loose cable; soldered
  headers fixed it. Healthy AD8232 @3.3V reads ~2047 at rest (our "green light").
- **Electrodes** — green/RL on elbow bone (required), red+yellow on the same forearm
  muscle belly. LO+/LO- leads-off detection wired to GPIO32/33 and confirmed.
- **Muscle detection works** — clench shows up as slow baseline shift, dev up to 682
  (ball squeeze) / 1890 (max) vs rest noise ~39.
- **ClenchDetector v10** (ARMED→CLENCH!→SETTLE→re-ARM state machine) — works.
- **ClenchDetector v11** (weak-signal edition: 0.3 s windows, 2-quick-lines OR
  big-spike ≥3× threshold instant fire) — ready for dried-pad sessions.
- Full documentation set exists from the earlier session (SFVIP-format report, BOM,
  build guide, poster, video script, teammate guide, academy quiz site).

### 🆕 THIS SESSION (5 Sep, night)
User request (was rejected by the AI service last time, answered now):
> "give me a code to test if my speaker works using the provided speaker module and the driver"

**Hardware discovery:** the user's actual audio parts (from their own Amazon links in an
earlier chat) are a **MAX98357A I2S amplifier** (B0H99FVF3V) + **2″ 4Ω 5W speaker**
(Electronic Spices, B0BN44HPF9). NOT a DFPlayer — no microSD, no MP3 player, and
**digital input only** (a plain GPIO tone test gives silence on it).

Created `vocalbridge-speaker/`:
| File | Purpose |
|---|---|
| **`SpeakerTest_MAX98357A.ino`** | **The speaker test for THEIR board.** I2S beeps (880/440 Hz chime with click-free fades), no library, auto-detects esp32 core v2/v3. BCLK=27, LRC=26, DIN=25 (no clash with sensor on 34). Commands: b/+/-/h. |
| **`SpeakerWireDoctor.ino`** | **Wiring self-diagnosis.** For the "garbled sound that dies when LRC is plugged in" case: cycles 6 BCLK/LRC/DIN pin arrangements in software (swaps + one-hole shifts), chimes after each. When the chime is clean, the printed mapping = the actual wiring. No rewiring needed to diagnose. |
| **`SpeakerSayHelp_MAX98357A.ino`** | **Speaker SPEAKS "Help!"** — real voice clip embedded in the ESP32 flash (`help_sound.h`, AI voice chosen by the user in chat, 24 kHz mono 0.47 s, silence-trimmed + normalized). No SD card, no library. Commands: h play / 2 double / l loop / +− volume / b beep / i info. |
| `make_help_header.py` | Regenerates `help_sound.h` from `audio/help.wav` — lets the user put **their own voice** into the device. Tested working. |
| `SpeakerTest_DFPlayer.ino` | Plan B if a DFPlayer Mini is ever bought: status report, auto-play, serial commands, decoded errors. |
| `SpeakerTest_Tone_Amp.ino` | Plan C for plain analog amps (PAM8403 etc.): beep test. Does NOT work on MAX98357A (digital-only). |
| `ClenchDetector_v12_SPEAKER.ino` | v11 + DFPlayer = full Phase-1 demo **for the DFPlayer path**. Speaker optional at runtime. |
| `SPEAKER_TEST_GUIDE.md` | Start here: identify module (MAX98357A = Plan A = theirs), wiring, troubleshooting, EMG noise rules, next-week plan. |

### Key wiring standardized this session
```
MAX98357A (their board):  VIN->VIN(5V)  GND->GND  BCLK->GPIO27  LRC->GPIO26  DIN->GPIO25
                          SD & GAIN unconnected; speaker -> + / - output
DFPlayer (if ever bought): VCC->VIN(5V) GND->GND RX->GPIO26 TX->GPIO27 (crossed!)
                          Speaker -> SPK_1 + SPK_2 (never GND); SD: FAT32 /mp3/0001.mp3…
```

## ⏭️ NEXT STEPS (after exams)
1. Wire MAX98357A (5 wires + speaker) → run `SpeakerTest_MAX98357A.ino` → hear chime.
2. Run `SpeakerSayHelp_MAX98357A.ino` → hear the device SAY "Help!" ✅ (voice is on-chip)
3. **Fresh pads**, clench test again with v10/v11 → confirm CLENCH! still fires.
4. **Build v13** = clench detector + spoken "Help" through the MAX98357A
   (merge v11 state machine + the playHelp() function from SpeakerSayHelp).
   Then: squeeze → CLENCH! + "Help!". Film it (Phase-1 demo complete).
   Optional: generate Yes/No/Water/Pain voice clips the same way.
5. Then the real VocalBridge goal: jaw clench / swallow / tongue press with pads
   under the chin — check if those give a usable dev too.
6. Package updates: BOM, build guide, poster, report, video script (all exist;
   update with the MAX98357A speaker section).

**Note:** every sketch now lives in its own subfolder (Arduino IDE merges all .ino
files in one folder into one sketch — that would break the build).

## 🔍 HARDWARE AUTHENTICITY AUDIT (5 Sep, via Amazon listings)
- **MAX98357A board (B0H99FVF3V)**: Brand "**Generic**", ₹269 (fake MRP ₹399). Generic
  Chinese breakout (Adafruit-design copy — legal/normal). Chip authenticity lottery;
  theirs distorts constantly despite soldering + code fixes → **likely defective unit /
  reject-grade chip. ACTION: claim Amazon 10-day defective replacement, or pivot to
  DFPlayer Mini (code ready) / branded board (ROBODUINO, Robocraze SmartElex).**
- **Electronic Spices 2″ 4Ω 5W speaker (B0BN44HPF9)**: genuine Indian budget brand,
  ₹143, sold via Clicktech/Amazon — not fake, just cheap. Battery pop test = final check.
- **AD8232 (red SparkFun-style board)**: design clone (normal in India) but PROVEN
  working — rest ~2047, clench dev 682, leads-off OK. Fine.
- **ESP32 DevKit**: proven working across all sessions. Fine.
- **Spandan (Sunfox) ECG electrodes**: genuine medical brand. Fine (drying = normal wear).

## 📁 File history (for archaeology)
- `workspace-01a06757…/` (earlier session): docs, poster, report, academy, BOM+speaker plan
- `workspace-01a06d50…/` (4 Sep session): ADC_SelfTest, MuscleTest v2–v4,
  ClenchDetector v5–v11, VocalBridge_4095_FIX.md
- `vocalbridge-speaker/` (this session): speaker tests + v12 + guide
