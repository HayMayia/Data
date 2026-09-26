#!/usr/bin/env python3
"""
VocalBridge Final_V1 — build any word's voice header from a recording
---------------------------------------------------------------------
Usage:
    python3 make_word_header.py <recording.wav> <WORD> [stretch] [repeat]

Example:
    python3 make_word_header.py help_own_v2.wav HELP
    python3 make_word_header.py water_own.wav WATER

Chain: trim -> (optional stretch, phase vocoder) -> telephone-band clarity
(HPF 250 Hz, presence +6 dB @ 2.2 kHz, LPF 6 kHz) -> gentle loudness
(12 dB, soft knee) -> normalize -> repeat x2 with 220 ms gap.

Writes:  <WORD>_voice.h   (lowercase) + prints stats.
Standard library only. Recordings with peak < 0.25 are rejected (duds).
"""

import wave, struct, os, sys, math

PAD_SEC  = 0.12
GAIN_DB  = 12.0
KNEE     = 0.35
REPEAT   = 2
GAP_S    = 0.22

# ---------------- filters ----------------
def highpass(xs, f0, fs, Q=0.707):
    w0 = 2*math.pi*f0/fs; a = math.sin(w0)/(2*Q); c = math.cos(w0)
    b0,b1,b2 = (1+c)/2, -(1+c), (1+c)/2
    a0,a1,a2 = 1+a, -2*c, 1-a
    x1=x2=y1=y2=0; out=[]
    for x in xs:
        y = (b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2)/a0
        out.append(y); x2,x1,y2,y1 = x1,x,y1,y
    return out

def highshelf(xs, f0, fs, gain_db, Q=0.9):
    A = 10**(gain_db/40); w0 = 2*math.pi*f0/fs
    sq = math.sqrt(A); c = math.cos(w0); al = math.sin(w0)/(2*Q)
    b0 = A*((A+1) + (A-1)*c + 2*sq*al)
    b1 = -2*A*((A-1) + (A+1)*c)
    b2 = A*((A+1) + (A-1)*c - 2*sq*al)
    a0 = (A+1) - (A-1)*c + 2*sq*al
    a1 = 2*((A-1) - (A+1)*c)
    a2 = (A+1) - (A-1)*c - 2*sq*al
    x1=x2=y1=y2=0; out=[]
    for x in xs:
        y = (b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2)/a0
        out.append(y); x2,x1,y2,y1 = x1,x,y1,y
    return out

def lowpass(xs, f0, fs):
    a = 1.0 - math.exp(-2*math.pi*f0/fs)
    y = 0.0; out = []
    for x in xs:
        y += a*(x-y)
        out.append(y)
    return out

def build(in_wav, word, stretch=1.0, repeat=REPEAT, natural=False):
    w = wave.open(in_wav, "rb")
    nch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    if sw != 2: sys.exit("ERROR: WAV must be 16-bit.")
    raw = w.readframes(n); w.close()
    samples = list(struct.unpack("<%dh" % (len(raw)//2), raw))
    if nch == 2:
        samples = [(samples[i]+samples[i+1])//2 for i in range(0, len(samples)-1, 2)]
    peak = max(map(abs, samples)) or 1
    if peak/32768 < 0.25:
        sys.exit("ERROR: near-silent dud recording (peak %.2f) — re-record." % (peak/32768))
    thr = peak*0.03
    first = next((i for i,s in enumerate(samples) if abs(s) > thr), 0)
    last  = next((len(samples)-1-i for i,s in enumerate(reversed(samples)) if abs(s) > thr),
                 len(samples)-1)
    pad = int(PAD_SEC*sr)
    xs = [s/32768.0 for s in samples[max(0,first-pad):min(len(samples), last+pad)]]
    if abs(stretch-1.0) > 0.02:
        # reuse phase vocoder from make_help_header.py if available
        try:
            import importlib.util
            spec = importlib.util.spec_from_file_location(
                "mh", os.path.join(os.path.dirname(__file__), "make_help_header.py"))
            mh = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mh)
            xs = mh.pv_stretch(xs, stretch)
        except Exception as e:
            sys.exit("stretch requested but phase vocoder unavailable: %s" % e)
    xs = highpass(xs, 250.0, sr)
    xs = highshelf(xs, 2200.0, sr, +6.0)
    xs = lowpass(xs, 6000.0, sr)
    g = 10**(GAIN_DB/20); lim = []
    for x in xs:
        x *= g; a = abs(x)
        if a > KNEE: a = KNEE + (1-KNEE)*math.tanh((a-KNEE)/(1-KNEE))
        lim.append(math.copysign(a, x))
    pk = max(map(abs, lim)) or 1
    out = [max(-32767, min(32767, int(x/pk*0.99*32767))) for x in lim]
    if repeat > 1:
        gap = [0]*int(GAP_S*sr)
        out = out + gap + out
    return out, sr

def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    natural = "--natural" in sys.argv
    argv = [a for a in sys.argv[1:] if a != "--natural"]
    in_wav, word = argv[0], argv[1].upper()
    stretch = float(argv[2]) if len(argv) > 2 else 1.0
    repeat  = int(argv[3]) if len(argv) > 3 else REPEAT
    out, sr = build(in_wav, word, stretch, repeat, natural)
    hdr = "%s_voice.h" % word.lower()
    with open(hdr, "w") as f:
        f.write("// VocalBridge Final_V1 — '%s' voice (user's own recording: %s)\n" % (word, os.path.basename(in_wav)))
        f.write("// %d Hz, mono, 16-bit PCM, %d samples (%.2f s). Do not edit by hand.\n" % (sr, len(out), len(out)/sr))
        f.write("// Regenerate: python3 make_word_header.py %s %s%s\n" % (os.path.basename(in_wav), word, " --natural" if natural else ""))
        f.write("#pragma once\n#include <Arduino.h>\n\n")
        f.write('#define %s_VOICE_VERSION "Final_V1 - %s - own voice%s x%d"\n\n' % (word, word, " NATURAL" if natural else "", repeat))
        f.write("static const uint32_t %s_SAMPLE_RATE = %d;\n" % (word, sr))
        f.write("static const uint32_t %s_NUM_SAMPLES = %d;\n" % (word, len(out)))
        f.write("static const int16_t %s_PCM[] = {\n" % word)
        for i in range(0, len(out), 16):
            f.write("  " + ",".join(str(v) for v in out[i:i+16])
                    + ("," if i+16 < len(out) else "") + "\n")
        f.write("};\n")
    print("OK: %s (%d samples @ %d Hz = %.2f s, repeat x%d)" % (hdr, len(out), sr, len(out)/sr, repeat))

if __name__ == "__main__":
    main()
