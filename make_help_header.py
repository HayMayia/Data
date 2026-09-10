#!/usr/bin/env python3
"""
VocalBridge — regenerate help_sound.h from a WAV recording
----------------------------------------------------------
Use this to put YOUR OWN voice (or a family member's) into the ESP32.

How:
  1. Record "Help!" (mono, any WAV — phone voice recorder then convert to WAV)
  2. Save/replace it as:  audio/help.wav        (next to this script's folder)
  3. Run:                 python3 make_help_header.py
     (on Windows maybe:   python make_help_header.py)
  4. Re-upload the SpeakerSayHelp_MAX98357A sketch — new voice is in.

It trims silence, normalizes loudness, and writes:
  SpeakerSayHelp_MAX98357A/help_sound.h
"""

import wave, struct, os, sys

IN_WAV   = os.path.join(os.path.dirname(__file__), "audio", "help.wav")
OUT_HDR  = os.path.join(os.path.dirname(__file__), "SpeakerSayHelp_MAX98357A", "help_sound.h")
PAD_SEC  = 0.12   # silence kept around the speech
TARGET   = 0.85   # normalize peak to 85% of full scale

def main():
    if not os.path.exists(IN_WAV):
        sys.exit("ERROR: %s not found — record 'Help!' and save it there." % IN_WAV)

    w = wave.open(IN_WAV, "rb")
    nch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    if sw != 2:
        sys.exit("ERROR: WAV must be 16-bit (yours is %d-bit). Re-export as 16-bit PCM." % (sw * 8))
    raw = w.readframes(n); w.close()

    samples = list(struct.unpack("<%dh" % (len(raw) // 2), raw))

    # stereo -> mono (average channels)
    if nch == 2:
        samples = [(samples[i] + samples[i + 1]) // 2 for i in range(0, len(samples) - 1, 2)]
    elif nch != 1:
        sys.exit("ERROR: only mono/stereo supported (got %d channels)" % nch)

    peak = max(abs(s) for s in samples) or 1
    thr = peak * 0.03
    first = next((i for i, s in enumerate(samples) if abs(s) > thr), 0)
    last  = next((len(samples) - 1 - i for i, s in enumerate(reversed(samples)) if abs(s) > thr),
                 len(samples) - 1)

    pad = int(PAD_SEC * sr)
    s, e = max(0, first - pad), min(len(samples), last + pad)
    out = samples[s:e]

    g = TARGET * 32767.0 / peak
    out = [max(-32767, min(32767, int(x * g))) for x in out]

    with open(OUT_HDR, "w") as f:
        f.write("// VocalBridge — 'Help!' voice, auto-generated from audio/help.wav\n")
        f.write("// %d Hz, mono, 16-bit PCM, %d samples (%.2f s). Do not edit by hand.\n"
                % (sr, len(out), len(out) / sr))
        f.write("// Regenerate with: python3 make_help_header.py  (after replacing audio/help.wav)\n")
        f.write("#pragma once\n#include <Arduino.h>\n\n")
        f.write("static const uint32_t HELP_SAMPLE_RATE = %d;\n" % sr)
        f.write("static const uint32_t HELP_NUM_SAMPLES = %d;\n" % len(out))
        f.write("static const int16_t HELP_PCM[] = {\n")
        for i in range(0, len(out), 16):
            f.write("  " + ",".join(str(v) for v in out[i:i + 16])
                    + ("," if i + 16 < len(out) else "") + "\n")
        f.write("};\n")

    print("OK: %d samples @ %d Hz (%.2f s) -> %s" % (len(out), sr, len(out) / sr, OUT_HDR))
    print("Now re-upload the SpeakerSayHelp_MAX98357A sketch.")

if __name__ == "__main__":
    main()
