# Pad placement — speech muscle recording (AD8232)

Match the label on your sensor cable: **RA**, **LA**, **RL** (RL is usually the **green** cable).
Pictures: `pad_placement_shoulderRL.png` (RL on the shoulder — recommended for everyone)
and `pad_placement.png` (RL on the chest bone — alternative, only if preferred).

## Placement — SAME spots for everyone in the group

| Label | Where exactly |
|---|---|
| **RA** | Under the chin, **left of center**, about two finger-widths below the jaw line |
| **LA** | Under the chin, **right of center**, **2–3 cm away from RA** (side by side) |
| **RL** (green, reference) | On the **bony top of the shoulder** — the hard bump where a t-shirt seam sits. Fully clothed, nothing on or near the chest. |

**Why under the chin:** these muscles move the tongue and the floor of the mouth — the busiest
muscles during speech, and different words move them differently (WATER = wide jaw +
tongue tip, NO = small mouth, YES = tongue high, HELP = breathy start + lip pop).
This spot also works for *silent mouthing* — a later test.

**Why the shoulder for RL:** the reference only needs a quiet, bony spot away from the
speaking muscles — the shoulder bone is exactly that, and everyone can use the identical
spot, so all three members' recordings are comparable.

## Measured 29 Sep: the green pad's spot changes the signal by ~2x

Same speaker, same word (HELP), same RA/LA spots — only RL moved:

| RL (green) spot | Silence | Word peaks | Word/silence |
|---|---|---|---|
| Below collarbone, on skin | ~314 | 1556–1764 | ~5x (strongest) |
| Back of neck | ~275 | 564–854 | ~2.3x |

Rules from this measurement:
- **Pick ONE RL spot and keep it for the whole 4-word study** — recordings from
  different placements cannot be compared.
- A green pad that is "a little off" is NOT a little change — it halved the signal.
- If you use the detector sketch (HelpDetect **V1.4+**), press **c** after moving any
  pad: it measures your silence for 5 s and re-tunes its gates automatically.

**Backups if the shoulder is noisy:**
- **Behind the ear** — the hard bone bump behind the earlobe (tie hair back).
- **Back of neck** — measured 29 Sep, works (about half the collarbone signal; press c to calibrate).
- **Chest bone** — the dip just below the collarbones (only if preferred; strongest measured signal).

## Skin prep (do this first — oil kills the signal)

1. Wipe the skin with water (or an alcohol wipe), let it dry.
2. Press each pad firmly for 10 seconds.
3. Let the cables hang loose / tape them down — a cable getting tugged makes fake spikes.

## If the signal is flat, try these (2 minutes each)

- **JAW:** RA on the cheek over the biting muscle (clench your teeth — the bump that
  pops out below the back teeth), LA 3 cm in front of it along the jaw line. RL stays on the chest.
- **THROAT:** RA and LA on either side of the Adam's apple, 1–2 cm away.
  (Picks up voicing — will NOT work for silent mouthing. Skip if anyone is not comfortable —
  the cheek spots above are usually enough.)

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
