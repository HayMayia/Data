#!/usr/bin/env python3
"""
VocalBridge — regenerate help_sound.h from a WAV recording  (LOUD v2 + CRISP + SLOWER)
--------------------------------------------------------------------------------------
Use this to put ANY voice into the ESP32 (record "Help!", save as audio/help.wav, run this).

Processing chain (in order):
  1. silence trim
  2. PAUSE INSERTION    -> words spoken naturally, 550 ms silence between them
  3. high-pass 170 Hz    -> removes bass the small speaker can't play (less mud)
  4. presence +4.5 dB @ 2.6 kHz -> crisper consonants
  5. LOUDNESS v2: +14 dB gain with deep soft-knee compression + tanh saturation
     -> much louder, punchy, no hard clipping
  6. normalize to 99%

Writes:
  SpeakerSayHelp_MAX98357A/help_sound.h   (upload this into the sketch)
  audio/help_enhanced.wav                 (preview — exactly what the device says)

Standard library only.
"""

import wave, struct, os, sys, math

IN_WAV  = os.path.join(os.path.dirname(__file__), "audio", "help.wav")
OUT_HDR = os.path.join(os.path.dirname(__file__), "SpeakerSayHelp_MAX98357A", "help_voice_v9.h")
PREVIEW = os.path.join(os.path.dirname(__file__), "audio", "help_enhanced.wav")
PAD_SEC = 0.12
STRETCH = 1.0      # v9: NO stretch at all (stretch was the cackle source)
PAUSE_S = 0.85     # silence inserted between words (the v9 slowness method)

GAIN_DB = 18.0     # loudness makeup gain before the soft limiter
KNEE    = 0.30     # soft-knee threshold (0..1) — lower = more compression

# ----------------------------------------------------------------------
def highpass(xs, f0, fs, Q=0.707):
    w0 = 2*math.pi*f0/fs; a = math.sin(w0)/(2*Q); c = math.cos(w0)
    b0, b1, b2 = (1+c)/2, -(1+c), (1+c)/2
    a0, a1, a2 = 1+a, -2*c, 1-a
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

def time_stretch(xs, ratio, fs, grain_ms=30.0, search_ms=12.0):
    """WSOLA-style overlap-add. ratio>1 = longer/slower, pitch preserved.
    Big ratios (2x+) need bigger grains + wider search + energy-normalized
    correlation, or the result warbles."""
    if abs(ratio - 1.0) < 0.001:
        return xs[:]
    N  = int(grain_ms * fs / 1000)
    Ha = N // 2
    Hs = int(round(Ha * ratio))
    S  = int(search_ms * fs / 1000)
    win = [0.5 - 0.5*math.cos(2*math.pi*i/N) for i in range(N)]
    out_len = int(len(xs)*ratio) + 2*N
    out  = [0.0]*out_len
    wsum = [0.0]*out_len
    n_grains = max(0, (len(xs) - N)//Ha)
    synth = 0
    for g in range(n_grains):
        ideal = g*Ha
        if g == 0:
            ana = ideal
        else:
            best, bestscore = ideal, -1e30
            lo, hi = max(0, ideal-S), min(len(xs)-N, ideal+S)
            for cand in range(lo, hi, 2):
                sc = 0.0; en = 0.0
                for k in range(0, N, 4):
                    v = xs[cand+k]
                    sc += v*out[synth+k]
                    en += v*v
                if en > 0:
                    sc /= math.sqrt(en)   # normalize: don't chase loud grains
                if sc > bestscore:
                    bestscore, best = sc, cand
            ana = best
        for i in range(N):
            out[synth+i]  += xs[ana+i]*win[i]
            wsum[synth+i] += win[i]
        synth += Hs
    res = [ (out[i]/wsum[i]) if wsum[i] > 0.01 else 0.0 for i in range(out_len) ]
    while res and abs(res[-1]) < 1e-4: res.pop()
    return res


def insert_pauses(xs, sr, pause_s=0.55, valley_thresh=0.03):
    """v9 slowness method: split at near-silent valleys between words and insert
    clean silence. Words keep natural pronunciation; NO time-stretch is used."""
    fl = int(0.01*sr)
    env = [math.sqrt(sum(x*x for x in xs[i:i+fl])/fl) for i in range(0, max(1,len(xs)-fl), fl)]
    sm = [sum(env[max(0,i-2):i+3])/len(env[max(0,i-2):i+3]) for i in range(len(env))]
    if not sm: return xs
    mx = max(sm)
    cuts = []
    last = -999
    for i in range(2, len(sm)-2):
        t = i*0.01
        if (sm[i] <= sm[i-1] and sm[i] <= sm[i+1] and sm[i] < mx*valley_thresh
                and t > 0.10 and t < len(xs)/sr - 0.10 and (i-last)*0.01 > 0.12):
            cuts.append(i*fl); last = i
    if not cuts: return xs
    bounds = [0] + cuts + [len(xs)]
    chunks = []
    for a, b in zip(bounds, bounds[1:]):
        seg = xs[a:b]
        # trim inner silence beyond 50 ms at each end
        fl2 = int(0.005*sr)
        e = [abs(v) for v in seg]
        thr = max(e)*0.06 if e else 0
        s0 = 0
        while s0 < len(seg)-fl2 and max(e[s0:s0+fl2]) < thr: s0 += fl2
        s1 = len(seg)
        while s1 > fl2 and max(e[s1-fl2:s1]) < thr: s1 -= fl2
        seg = seg[max(0,s0-int(0.05*sr)) : min(len(seg), s1+int(0.05*sr))]
        if len(seg) < int(0.08*sr):     # too tiny -> skip
            continue
        f = int(0.008*sr)               # 8 ms fade edges (click-free)
        for i in range(min(f, len(seg))):
            g = i/f
            seg[i] *= g
            seg[-1-i] *= g
        chunks.append(seg)
    if len(chunks) < 2: return xs
    silence = [0.0]*int(pause_s*sr)
    out = []
    for i, c in enumerate(chunks):
        if i: out += silence
        out += c
    return out

def rms_db(xs):
    r = math.sqrt(sum(x*x for x in xs)/len(xs))
    return 20*math.log10(r/32768) if xs else -99

# ----------------------------------------------------------------------
def main():
    if not os.path.exists(IN_WAV):
        sys.exit("ERROR: %s not found — record 'Help!' and save it there." % IN_WAV)

    w = wave.open(IN_WAV, "rb")
    nch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    if sw != 2:
        sys.exit("ERROR: WAV must be 16-bit (yours is %d-bit)." % (sw*8))
    raw = w.readframes(n); w.close()
    samples = list(struct.unpack("<%dh" % (len(raw)//2), raw))
    if nch == 2:
        samples = [(samples[i]+samples[i+1])//2 for i in range(0, len(samples)-1, 2)]
    elif nch != 1:
        sys.exit("ERROR: only mono/stereo supported.")

    if max(map(abs, samples)) < 0.25:
        sys.exit("ERROR: this WAV is a near-silent dud (peak < 0.25). "
                 "Re-generate/re-record the take and try again.")

    before = rms_db(samples)

    # 1) trim silence
    peak = max(map(abs, samples)) or 1
    thr = peak*0.03
    first = next((i for i,s in enumerate(samples) if abs(s) > thr), 0)
    last  = next((len(samples)-1-i for i,s in enumerate(reversed(samples)) if abs(s) > thr),
                 len(samples)-1)
    pad = int(PAD_SEC*sr)
    xs = [s/32768.0 for s in samples[max(0,first-pad):min(len(samples), last+pad)]]

    # 2) slower = natural words + inserted pauses (v9: zero stretch)
    xs = time_stretch(xs, STRETCH, sr)     # STRETCH=1.0 -> no-op safety
    xs = insert_pauses(xs, sr, pause_s=PAUSE_S)

    # 3) clarity
    xs = highpass(xs, 170.0, sr)
    xs = highshelf(xs, 2600.0, sr, +4.5)

    # 4) loudness v2: strong gain + deep soft-knee compression + tanh saturation
    g = 10**(GAIN_DB/20)
    lim = []
    for x in xs:
        x *= g
        a = abs(x)
        if a > KNEE:
            a = KNEE + (1-KNEE)*math.tanh((a-KNEE)/(1-KNEE))
        lim.append(math.copysign(a, x))

    # 5) normalize to 99%
    pk = max(map(abs, lim)) or 1
    out = [max(-32767, min(32767, int(x/pk*0.99*32767))) for x in lim]

    after = rms_db(out)

    with open(OUT_HDR, "w") as f:
        f.write("// VocalBridge — 'Help!' voice (LOUD v2 + CRISP + %d%% slower), from audio/help.wav\n"
                % round((STRETCH-1)*100))
        f.write("// %d Hz, mono, 16-bit PCM, %d samples (%.2f s). Do not edit by hand.\n"
                % (sr, len(out), len(out)/sr))
        f.write("// Regenerate: python3 make_help_header.py  (after replacing audio/help.wav)\n")
        f.write("#pragma once\n#include <Arduino.h>\n\n")
        f.write('#define HELP_VOICE_VERSION "v9 - natural words + 550ms pauses - ZERO stretch (Help! Help! Help, please!)"\n\n')
        f.write("static const uint32_t HELP_SAMPLE_RATE = %d;\n" % sr)
        f.write("static const uint32_t HELP_NUM_SAMPLES = %d;\n" % len(out))
        f.write("static const int16_t HELP_PCM[] = {\n")
        for i in range(0, len(out), 16):
            f.write("  " + ",".join(str(v) for v in out[i:i+16])
                    + ("," if i+16 < len(out) else "") + "\n")
        f.write("};\n")

    pv = wave.open(PREVIEW, "wb"); pv.setnchannels(1); pv.setsampwidth(2); pv.setframerate(sr)
    pv.writeframes(struct.pack("<%dh" % len(out), *out)); pv.close()

    print("OK: %d samples @ %d Hz (%.2f s, %.0f%% slower)" % (len(out), sr, len(out)/sr, (STRETCH-1)*100))
    print("loudness: %.1f -> %.1f dBFS  (%+.1f dB)" % (before, after, after-before))
    print("preview: %s" % PREVIEW)
    print("Now re-upload the SpeakerSayHelp_MAX98357A sketch.")

if __name__ == "__main__":
    main()
