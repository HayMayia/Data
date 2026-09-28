# Pad placement — speech muscle recording (AD8232)

Match the label on your sensor cable: **RA**, **LA**, **RL**.
(See `pad_placement.png` for the picture.)

## Primary placement: UNDER THE CHIN — start here

| Label | Where exactly |
|---|---|
| **RA** | Under the chin, **left of center**, about two finger-widths below the jaw line |
| **LA** | Under the chin, **right of center**, **2–3 cm away from RA** (side by side) |
| **RL** (reference) | On the flat **chest bone at the center**, in the little dip just below the collarbones |

**Why here:** these muscles move the tongue and the floor of the mouth — the busiest
muscles during speech, and different words move them differently (WATER = wide jaw +
tongue tip, NO = small mouth, YES = tongue high, HELP = breathy start + lip pop).
This spot also works for *silent mouthing* — a later test.

## Skin prep (do this first — oil kills the signal)

1. Wipe the skin with water (or an alcohol wipe), let it dry.
2. Press each pad firmly for 10 seconds.
3. Let the cables hang loose / tape them down — a cable getting tugged makes fake spikes.

## If the signal is flat, try these (2 minutes each)

- **JAW:** RA on the cheek over the biting muscle (clench your teeth — the bump that
  pops out below the back teeth), LA 3 cm in front of it along the jaw line. RL stays on the chest.
- **THROAT:** RA and LA on either side of the Adam's apple, 1–2 cm away.
  (Picks up voicing — will NOT work for silent mouthing.)

## During recording

- Sit still, hands off the table and cables.
- Say each word **5–10 times**, normal calling volume, ~2 s pause between words.
- In the Serial Monitor, press the word's key right after saying it:
  `h` `w` `y` `n` — press `s` if a spike happened but you said nothing.
- `PADS:1` must stay 1. If it shows 0 + "CHECK-PADS!", a pad fell off or a cable unsnapped.

## Honest expectations

The AD8232 is an ECG chip (0.5–40 Hz). Real muscle signal (EMG) is 20–450 Hz, so we see
the **slow shape of muscle bursts** — when they happened and how strong — not the fine
signal. That is exactly what we need for the video study: whether the four words look
different enough to tell apart. This sketch records only; it never decides.

## Wiring (same as the arm demo — no speaker needed)

AD8232: 3V3→3V3, GND→GND, OUT→GPIO34, LO+→GPIO32, LO−→GPIO33. USB cable to the computer.
