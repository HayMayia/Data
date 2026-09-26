# VocalBridge — Vocabulary & Update Roadmap

## Phase 1 (current): factory vocabulary — 4 words, own voice, NATURAL (as recorded)
- [x] HELP  — HELP_V3 take, natural build done (help_voice.h, 1.54 s x2)
- [x] WATER — original take, natural build done (water_voice.h, 2.08 s x2)
- [ ] YES   — waiting for recording (YES.m4a)
- [ ] NO    — waiting for recording (NO.m4a)

Recording protocol: quiet room, phone 15–20 cm, natural calling pace,
~1 s silence around the word, one word per file -> Drive.

Build: python3 make_word_header.py <rec>.wav <WORD> --natural
(no surgery, no EQ — user directive 26 Sep: "don't alter a thing")

## Phase 2 (AFTER all 4 above are done): the "Tesla update" demo — add PAIN over the air
REMINDER TRIGGER: when YES + NO are built and the 4-word sketch is verified on device,
remind the user to start Phase 2.

- Method: ESP32-to-ESP32 firmware OTA over ESP-NOW (fact-checked 26 Sep vs Espressif docs):
  sender chunks a compiled .bin (<=250 B/packet, ACK each), receiver writes via Update.h
  into the inactive OTA partition, verifies, swaps boot flag, reboots into new code.
  Old 4-word firmware genuinely has no PAIN — bytes arrive over the air. No hidden inputs.
- PAIN recording lives ONLY on the sender ESP32 (never preloaded on the device) — keeps the demo honest.
- No SD card needed: sender stores the update in its own flash (SPIFFS/LittleFS partition).
- Fallback (only if ESP-NOW proves unreliable): Wi-Fi OTA via HTTPUpdate.
- Needs: 2nd ESP32, OTA partition scheme (Tools > Partition Scheme: "Minimal SPIFFS (Large APPS with OTA)"
  or similar 2-slot scheme) — check before first flash.

## Standing rules
- Zero-context intelligibility (mother test) is the bar for every word.
- Never ship unverified zips; every build ships natural preview + device master.
- User tests every build on the real speaker before it counts.
