# VocalBridge — Vocabulary & Update Roadmap

## Phase 1 (SHIPPED 28 Sep as Voice_Final-V5): factory vocabulary — 4 words, own voice, NATURAL (as recorded)
- [x] HELP  — HELP_V3 take, natural build done (help_voice.h, 0.66 s, said once)
- [x] WATER — original take, natural build done (water_voice.h, 0.93 s, said once)
- [x] YES   — built 28 Sep from YES.m4a (natural, 0.90 s, said once)
- [x] NO    — built 28 Sep from NO.m4a (natural, 0.83 s, said once)
- [x] 28 Sep — words now play ONCE per key press (user request); accepted audio samples unchanged

Recording protocol: quiet room, phone 15–20 cm, natural calling pace,
~1 s silence around the word, one word per file -> Drive.

Build: python3 make_word_header.py <rec>.wav <WORD> --natural
(no surgery, no EQ — user directive 26 Sep: "don't alter a thing")

## Phase 1.5 (SHIPPED 12 Oct as HelpDetect V2.0): the loop is CLOSED — sense -> speak
Before V2.0 the project had two halves that never met:

| | senses the word | speaks it aloud | triggered by |
|---|---|---|---|
| `HelpDetect_AD8232` V1.9 | YES (tuned box + auto-learn) | NO — serial print only | the muscle signal |
| `SpeakerSayWords_MAX98357A` V5 | NO | YES (own voice, on-chip) | a keypress on a laptop |

Neither alone is VocalBridge. **V2.0 merges them**: a sensed HELP now plays the
2-beep alert and says "HELP!" in the user's own voice, with no laptop, no Serial
Monitor and no SD card. Audio path + the four voice headers are lifted VERBATIM
from the V5 build the user already verified on the real speaker (28 Sep), so
nothing unproven was introduced.

- [x] 12 Oct — V2.0 built: V1.9 detector + V5 own-voice playback
- [x] 12 Oct — detection logic provably untouched: diff vs V1.9 = 233 added /
      10 removed lines, and **all 10 removed lines are comment or banner text**
- [x] 12 Oct — compile+link verified on BOTH esp32 core paths (v2 `driver/i2s.h`
      and v3 `ESP_I2S.h`), 0 errors / 0 warnings; `setup()`+`loop()` executed clean
- [x] 12 Oct — log line now carries `burst:` (the calibrated gate) and `box:`,
      so a pasted log can be replayed offline. Older logs lack `burstLvl`, which
      is why sessions 1-6 could never be re-analysed without guessing.
- [ ] **USER MUST DO:** flash V2.0 and confirm the speaker actually fires on a
      sensed HELP. Standing rule: *user tests every build on the real speaker
      before it counts.* This build is NOT yet device-verified.

New keys in V2.0 (V1.9 keys all unchanged, including `+/-` = box edge):
`1/2/3/4` = say HELP/WATER/YES/NO by hand (speaker self-test + demo fallback),
`b` = beep, `m` = mute/unmute (use for tuning and the statue test),
`[` `]` = volume down/up. `t` = test now prints AND speaks (true end-to-end).

## STILL OPEN — WATER / YES / NO have no sensing data
They remain COLLECT-ONLY: V2.0 prints `COLLECTED` and does not alert, because
**there is nothing to build a box from**. Measured across all archived logs,
`Values.txt` (session 6) contains 18 `MARK:HELP` and **0** `MARK:WATER`,
`MARK:YES`, `MARK:NO`. Sessions 1-6 give HELP data only.

Protocol to close this (one ~10 min session, same rules as always):
1. Re-seat Green (+Red/Yellow) firmly on bare skin for 10 s, then do not move it.
2. Sit silent, hands OFF the pads, press `c` -> expect `CALIBRATE OK`.
3. Statue test 20 s — any HELP during it is a wire/pad fault, not a word.
4. Say each word 8x, pressing its key right after each one:
   `w` for WATER, `y` for YES, `n` for NO. Keep `h`/HELP for last.
5. Paste the log. Because V2.0 logs `burst:` and `box:`, the three new boxes can
   be derived offline for the first time — no guessing the gate.

Only after that can the detector tell WATER from YES from NO, which is what stops
"everything is HELP".

## Phase 2 (AFTER all 4 above are done): the "Tesla update" demo — add PAIN over the air
REMINDER TRIGGER: when YES + NO are built and the 4-word sketch is verified on device,
remind the user to start Phase 2.

- Method: ESP32-to-ESP32 firmware OTA over ESP-NOW (fact-checked 26 Sep vs Espressif docs):
  sender chunks a compiled .bin (<=250 B/packet, ACK each), receiver writes via Update.h
  into the inactive OTA partition, verifies, swaps boot flag, reboots into new code.
  Old 4-word firmware genuinely has no PAIN — bytes arrive over the air. No hidden inputs.
- PAIN source downloaded + converted 28 Sep (audio/pain_own.wav, 0.61 s word, 81% peak) — ready for the sender build; NOT in the device zip/sketch
- PAIN recording lives ONLY on the sender ESP32 (never preloaded on the device) — keeps the demo honest.
- No SD card needed: sender stores the update in its own flash (SPIFFS/LittleFS partition).
- Fallback (only if ESP-NOW proves unreliable): Wi-Fi OTA via HTTPUpdate.
- The PAIN update ships as **Voice_Final-V6** — boot screen shows V5 before, V6 after: visible proof the update is real.
- Needs: 2nd ESP32, OTA partition scheme (Tools > Partition Scheme: "Minimal SPIFFS (Large APPS with OTA)"
  or similar 2-slot scheme) — check before first flash.

## Standing rules
- Zero-context intelligibility (mother test) is the bar for every word.
- Never ship unverified zips; every build ships natural preview + device master.
- User tests every build on the real speaker before it counts.
- Firmware versions in sequence (user directive 28 Sep): Voice_Final-V5 (current) → V6 → V7 … Every build prints its version at boot.
