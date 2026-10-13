/*
 * VocalBridge — HELP DETECTOR (HelpDetect_AD8232)  V1.11
 * -------------------------------------------------------
 * V1.11 = CHIN-PLACEMENT RETUNE (13 Oct 2026 — your Values.txt 13:45)
 *   Two gel electrodes beneath chin, as you described:
 *     1. Yellow wire — submandibular LEFT of center: circular white gel patch
 *        directly under chin, clip pointing up
 *     2. Red wire — RIGHT side of neck / upper throat: circular white gel patch,
 *        clip pointing up
 *     Grey cables stream down front of green t-shirt.
 *   Fix for new chin spot: HELPs there are shorter/weaker (your 13:45 log: 19 HELP tries
 *   but only 3 fired — 16 bursts were 37-75ms, peak 589-1114, just below old 800x80 guard).
 *   Previous box was too strict for submandibular placement.
 *   On new spot: press e to reset box, then c to calibrate (sit silent, hands OFF
 *   pads), then h/a/v will auto-learn the new neck signals — no values to feed.
 *
 * V1.9 = AUTO-LEARN + NEW VALUES (209 HELPs from your Values.txt, 12 Oct 2026)
 *   You asked: "just write h and it changes. And a and v do the same. And when
 *   I said HELP in similar pattern, it said HELP as well."  -> DONE.
 *   Now h, a, v ALL auto-change the box from inside the code (no logs to send):
 *     h = MARK HELP + instantly expands HELP box to include that burst (±60/±35) + save to flash
 *     a = same as h (auto-learn this HELP) + also recomputes tight box from all h's this session
 *     v = view learned HELPs + also auto-learns if a burst is recent (so v also changes when needed)
 *   All three: peak/dur box saved to flash, survives reboot. Press i any time to see box.
 *   When you say HELP in similar peak/dur again, burst lands inside box -> HELP! (no h needed).
 *   NEW: pressing h/w/y/n now AUTOMATICALLY tunes the detector — no logs to send.
 *   - Press h at the moment you say HELP (same as before). The board finds the
 *     burst that belongs to that word (within 2 s) and EXPANDS the HELP box to
 *     include it (peak ±60, dur ±30 ms), saves to flash, and prints the new box.
 *     No reflash, no values to feed. Works for every HELP you mark.
 *   - Yes, GREEN-moved = FALSE — session 5 excluded from training (see below).
 *     Your 15 marks in session 5 were pad-movement spikes, not words. Not learned.
 *
 *   Other auto keys:
 *     a = recompute HELP box from all marks this session (min→max + margin)
 *     v = view all learned HELP peaks/durs this session
 *     e = erase learned HELPs & reset box to defaults (800-2000 x 150-1100)
 *     z = same as e
 *   All box changes (±, h-auto, a, e) are saved to ESP32 flash (Preferences)
 *   so they survive reboot. Press i to see current box any time.
 *
 * V1.11 defaults: peak 650-2000 x dur 45-1100 (tuned for chin: was 800x150) — from 223 HELPs + your 13:45 chin log (19 tries, 3 ok, 16 short 37-75ms)
 *      HELP = peak in [800 .. 2000] AND dur in [150 .. 1100] ms  (AND rule)
 *   From 223 HELPs: S1 1121-1710 350-1150 + S4 904/1624 150/400 + Values.txt 209 HELPs 802-1960 151-1009 ms
 *   Excluded (proven contact faults): S3 loose RL + S5 green moved (15 invalid marks)
 *   Validation: S4 2/2, S1 12/12, S2 silent 0 false, S5 green-moved 0 HELP on invalid marks.
 *
 * Keys: c calibrate  h/w/y/n marks+auto-learn  a recompute  v view  e/z erase
 *       +/- HELP lower edge  t test  i info  r speed
 * Calibrate: sit SILENT hands OFF pads after reseating any pad, press c.
 *
 * Board: ESP32 Dev Module, Baud 115200
 * Wiring: AD8232 3V3->3V3 GND->GND OUT->34 LO+->32 LO-->33
 * Placement 13 Oct: YELLOW=submandibular left of center under chin, RED=right neck/upper throat (grey leads down shirt)
 * Keys: full list below in banner
 */
#include <Preferences.h>

const int PIN_OUT = 34, PIN_LOP = 32, PIN_LON = 33;
const int SAMPLE_MS = 2;
int printMs = 50;

long center = 2048;
int  env = 0, lastRaw = 0;
bool firstSample = true;
const int RING = 100;
int ring[RING]; int ringIdx = 0;

// ---------- detector V1.8 (range classifier + auto-learn + spike warning) ----------
// defaults = V1.7 box (stored to flash; changed by auto-learn)
const int DEF_PK_MIN = 650; // lowered 800->650 for chin placement (your HELPs peaked 589-1114, not 802-1960)
const int DEF_PK_MAX = 2000;
const int DEF_DUR_MIN = 45;  // lowered 150->45 for chin placement (your HELPs dur 37-75ms, not 150+)
const int DEF_DUR_MAX = 1100;
int  helpPeakMin = DEF_PK_MIN;
int  helpPeakMax = DEF_PK_MAX;
unsigned long helpDurMin = DEF_DUR_MIN;
unsigned long helpDurMax = DEF_DUR_MAX;

int  burstLvl = 511;
const unsigned long BURST_GRACE_MS = 150; // 100->150 to merge split chin bursts (your HELP split into 37+43ms)
const unsigned long REFRACTORY_MS = 1500;
const unsigned long BURST_MAX_MS   = 3000;

bool inBurst = false;
unsigned long burstStart = 0, lastHigh = 0, lastFire = 0;
int  burstPeak = 0, maxEnvEver = 0, helpCount = 0;
unsigned long tS = 0, tP = 0;

// last completed burst (for mark-after-burst case)
int lastBurstPeak = 0;
unsigned long lastBurstDur = 0;
unsigned long lastBurstEnd = 0;

// spike-rate warning
unsigned long spikeTimes[12]; int spikeCount = 0; int spikeIdx = 0;

// training store (this session; box persists to flash even after reboot)
const int MAX_HELP_EX = 30;
int helpPeaks[MAX_HELP_EX];
unsigned long helpDurs[MAX_HELP_EX];
int helpExCount = 0;

// pending mark (for mark-before-burst-ends case)
enum MarkKind { MK_NONE, MK_HELP, MK_WATER, MK_YES, MK_NO, MK_SILENT };
MarkKind pendingKind = MK_NONE;
unsigned long pendingAt = 0;

Preferences prefs;

void saveBox() {
  prefs.begin("helpdetect", false);
  prefs.putInt("pkMin", helpPeakMin);
  prefs.putInt("pkMax", helpPeakMax);
  prefs.putInt("durMin", (int)helpDurMin);
  prefs.putInt("durMax", (int)helpDurMax);
  prefs.putInt("burstLvl", burstLvl);
  prefs.end();
}
void loadBox() {
  prefs.begin("helpdetect", true);
  helpPeakMin = prefs.getInt("pkMin", DEF_PK_MIN);
  helpPeakMax = prefs.getInt("pkMax", DEF_PK_MAX);
  helpDurMin  = (unsigned long)prefs.getInt("durMin", DEF_DUR_MIN);
  helpDurMax  = (unsigned long)prefs.getInt("durMax", DEF_DUR_MAX);
  int savedLvl = prefs.getInt("burstLvl", 0);
  if (savedLvl > 0) burstLvl = savedLvl; // 0 = not yet saved, keep 511 until first calibrate
  prefs.end();
  // clamp sanity
  if (helpPeakMin < 50) helpPeakMin = DEF_PK_MIN;
  if (helpPeakMax > 4095) helpPeakMax = DEF_PK_MAX;
  if (helpPeakMax < helpPeakMin) { helpPeakMin = DEF_PK_MIN; helpPeakMax = DEF_PK_MAX; }
  if (helpDurMin < 15) helpDurMin = DEF_DUR_MIN;
  if (helpDurMax > 5000) helpDurMax = DEF_DUR_MAX;
}

void printBanner() {
  Serial.println();
  Serial.println(F("==== VocalBridge HELP DETECTOR V1.11 (new chin placement + AUTO-LEARN + spike check) ===="));
  Serial.println(F("baud=115200"));
  Serial.print(F("HELP box: peak "));   Serial.print(helpPeakMin);
  Serial.print(F("-"));                 Serial.print(helpPeakMax);
  Serial.print(F("  AND  dur "));       Serial.print(helpDurMin);
  Serial.print(F("-"));                 Serial.print(helpDurMax);
  Serial.println(F(" ms   (defaults 800-2000 x 150-1100; auto-expands on h)"));
  Serial.print(F("burst level="));      Serial.println(burstLvl);
  Serial.println(F("burst inside the box -> HELP; outside -> BURST + reason"));
  Serial.println(F("WATER/YES/NO: collected but no alert box yet - helps for next tuning"));
  Serial.print(F("learned HELP examples this session: ")); Serial.println(helpExCount);
  Serial.println(F("PLACEMENT 13 Oct: Yellow=left submandibular under chin, Red=right neck/upper throat (grey cables down shirt)"));
  Serial.println(F("keys: c=calibrate  h/w/y/n=mark + AUTO-LEARN  a=recompute box  v=view  e/z=erase"));
  Serial.println(F("      +/- box low edge  t=test  i=info  r=speed"));
  Serial.println(F("STATUE TEST: sit silent+still 20 s - HELP during it = wire/pad problem"));
  Serial.println(F("plotter: raw,env,peak,pads,thresh"));
  Serial.println(F("streaming..."));
}

void mark(const __FlashStringHelper *w) {
  Serial.print(F(">>>MARK:")); Serial.print(w);
  Serial.print(F(" t=")); Serial.print(millis()/1000.0, 2); Serial.println(F("s<<<"));
}

void fireHelp(int pk, unsigned long durMs) {
  helpCount++;
  Serial.println(F("############################################################"));
  Serial.print(F(">>>  HELP!  DETECTED  #")); Serial.print(helpCount);
  Serial.print(F("   peak=")); Serial.print(pk);
  Serial.print(F("   dur=")); Serial.print(durMs);
  Serial.print(F("ms   max=")); Serial.print(maxEnvEver);
  Serial.print(F("   t=")); Serial.print(millis()/1000.0, 2); Serial.println(F("s"));
  Serial.println(F("############################################################"));
}

void noteSpike(unsigned long now) {
  spikeTimes[spikeIdx] = now;
  spikeIdx = (spikeIdx + 1) % 12;
  if (spikeCount < 12) spikeCount++;
  if (spikeCount >= 8) {
    int recent = 0;
    for (int i = 0; i < spikeCount; i++) if (now - spikeTimes[i] < 10000) recent++;
    if (recent >= 8) {
      Serial.println(F(">>> HINT: many short spikes (8+ in 10 s) — check GREEN pad contact, then press c while silent"));
      spikeCount = 0;
    }
  }
}

void printTraining() {
  Serial.print(F(">>> LEARNED HELP examples: ")); Serial.print(helpExCount); Serial.println(F(" (this session)"));
  for (int i = 0; i < helpExCount; i++) {
    Serial.print(F("  #")); Serial.print(i+1);
    Serial.print(F(" peak=")); Serial.print(helpPeaks[i]);
    Serial.print(F(" dur=")); Serial.print(helpDurs[i]); Serial.println(F("ms"));
  }
  if (helpExCount==0) Serial.println(F("  (none yet — say HELP and press h within 2 s of the word)"));
  Serial.print(F(">>> current HELP box: peak ")); Serial.print(helpPeakMin);
  Serial.print(F("-")); Serial.print(helpPeakMax);
  Serial.print(F("  dur ")); Serial.print(helpDurMin);
  Serial.print(F("-")); Serial.print(helpDurMax); Serial.println(F(" ms  (saved to flash)"));
}

void expandBoxForHelp(int pk, unsigned long dur) {
  bool changed = false;
  int newPkMin = helpPeakMin, newPkMax = helpPeakMax;
  unsigned long newDurMin = helpDurMin, newDurMax = helpDurMax;
  if (pk < helpPeakMin) { newPkMin = max(50, pk - 60); changed = true; }
  if (pk > helpPeakMax) { newPkMax = min(4095, pk + 60); changed = true; }
  if (dur < helpDurMin) { newDurMin = (dur > 35) ? dur - 35 : 20; changed = true; }
  if (dur > helpDurMax) { newDurMax = dur + 35; if (newDurMax > 4000) newDurMax = 4000; changed = true; }
  if (changed) {
    helpPeakMin = newPkMin; helpPeakMax = newPkMax;
    helpDurMin = newDurMin; helpDurMax = newDurMax;
    saveBox();
    Serial.print(F(">>> AUTO-LEARN: box EXPANDED to peak "));
    Serial.print(helpPeakMin); Serial.print(F("-")); Serial.print(helpPeakMax);
    Serial.print(F("  dur ")); Serial.print(helpDurMin); Serial.print(F("-")); Serial.print(helpDurMax);
    Serial.println(F(" ms  (saved)"));
  } else {
    Serial.print(F(">>> AUTO-LEARN: HELP peak=")); Serial.print(pk);
    Serial.print(F(" dur=")); Serial.print(dur);
    Serial.print(F("ms already inside box ")); Serial.print(helpPeakMin);
    Serial.print(F("-")); Serial.print(helpPeakMax); Serial.print(F(" x "));
    Serial.print(helpDurMin); Serial.print(F("-")); Serial.print(helpDurMax);
    Serial.println(F(" — box unchanged"));
  }
}

void autoLearnHelp(int pk, unsigned long dur) {
  // guard: chin HELPs are short/weak — accept down to 400/30ms (was 500/80)
  if (pk < 400 || dur < 30 || dur > 3000) {
    Serial.print(F(">>> AUTO-LEARN ignored: burst peak=")); Serial.print(pk);
    Serial.print(F(" dur=")); Serial.print(dur);
    Serial.println(F("ms — too weak/short/long to be a word. Say HELP clearly and press h within 2 s."));
    return;
  }
  if (helpExCount < MAX_HELP_EX) {
    helpPeaks[helpExCount] = pk;
    helpDurs[helpExCount] = dur;
    helpExCount++;
  } else {
    for (int i = 0; i < MAX_HELP_EX-1; i++) { helpPeaks[i]=helpPeaks[i+1]; helpDurs[i]=helpDurs[i+1]; }
    helpPeaks[MAX_HELP_EX-1]=pk; helpDurs[MAX_HELP_EX-1]=dur;
  }
  Serial.print(F(">>> AUTO-LEARN HELP #")); Serial.print(helpExCount);
  Serial.print(F(": peak=")); Serial.print(pk);
  Serial.print(F(" dur=")); Serial.print(dur); Serial.println(F("ms"));
  expandBoxForHelp(pk, dur);
}

void recomputeBoxFromTraining() {
  if (helpExCount==0) {
    Serial.println(F(">>> RECOMPUTE: no learned HELPs yet — box unchanged. Say HELP + press h first."));
    return;
  }
  int mnPk = helpPeaks[0], mxPk = helpPeaks[0];
  unsigned long mnDur = helpDurs[0], mxDur = helpDurs[0];
  for (int i=1;i<helpExCount;i++) {
    if (helpPeaks[i] < mnPk) mnPk = helpPeaks[i];
    if (helpPeaks[i] > mxPk) mxPk = helpPeaks[i];
    if (helpDurs[i] < mnDur) mnDur = helpDurs[i];
    if (helpDurs[i] > mxDur) mxDur = helpDurs[i];
  }
  int newPkMin = max(50, mnPk - 70);
  int newPkMax = min(4095, mxPk + 70);
  unsigned long newDurMin = (mnDur > 40) ? mnDur - 40 : 20;
  unsigned long newDurMax = mxDur + 40;
  helpPeakMin = newPkMin; helpPeakMax = newPkMax;
  helpDurMin = newDurMin; helpDurMax = newDurMax;
  // keep at least defaults union so we don't shrink below proven range if user had only 1 example
  // (user can press e to force reset, then a to set tight box if they want)
  saveBox();
  Serial.print(F(">>> RECOMPUTE from ")); Serial.print(helpExCount);
  Serial.print(F(" HELPs: min peak=")); Serial.print(mnPk);
  Serial.print(F(" max=")); Serial.print(mxPk);
  Serial.print(F("  min dur=")); Serial.print(mnDur);
  Serial.print(F(" max=")); Serial.print(mxDur);
  Serial.println(F("ms"));
  Serial.print(F(">>> NEW HELP box: peak ")); Serial.print(helpPeakMin);
  Serial.print(F("-")); Serial.print(helpPeakMax);
  Serial.print(F("  dur ")); Serial.print(helpDurMin);
  Serial.print(F("-")); Serial.print(helpDurMax); Serial.println(F(" ms (saved)"));
}

void eraseTraining() {
  helpExCount = 0;
  helpPeakMin = DEF_PK_MIN; helpPeakMax = DEF_PK_MAX;
  helpDurMin = DEF_DUR_MIN; helpDurMax = DEF_DUR_MAX;
  saveBox();
  Serial.println(F(">>> ERASED: HELP examples cleared, box reset to defaults 800-2000 x 150-1100 (saved)"));
}

void classifyBurst(unsigned long durMs) {
  // save last burst for mark-after case
  lastBurstPeak = burstPeak; lastBurstDur = durMs; lastBurstEnd = millis();

  bool isShortSpike = (durMs < helpDurMin && burstPeak >= helpPeakMin);
  if (isShortSpike) noteSpike(burstStart);

  // AUTO-LEARN hook: if a HELP mark is pending within 2.5 s of this burst, learn it
  if (pendingKind == MK_HELP && lastBurstEnd >= pendingAt && lastBurstEnd - pendingAt < 2500) {
    autoLearnHelp(burstPeak, durMs);
    pendingKind = MK_NONE;
  } else if (pendingKind == MK_HELP && pendingAt >= burstStart && pendingAt - burstStart < 2500) {
    // mark came just before burst started (pressed during word)
    autoLearnHelp(burstPeak, durMs);
    pendingKind = MK_NONE;
  } else if (pendingKind != MK_NONE && millis() - pendingAt > 3000) {
    // stale pending (no burst found) — clear
    pendingKind = MK_NONE;
  }
  // also collect WATER/YES/NO for later (no alert, just store + print)
  if (pendingKind == MK_WATER || pendingKind == MK_YES || pendingKind == MK_NO) {
    if (millis() - pendingAt < 2500) {
      Serial.print(F(">>> COLLECTED ")); 
      if (pendingKind==MK_WATER) Serial.print(F("WATER"));
      else if (pendingKind==MK_YES) Serial.print(F("YES"));
      else Serial.print(F("NO"));
      Serial.print(F(": peak=")); Serial.print(burstPeak);
      Serial.print(F(" dur=")); Serial.print(durMs); Serial.println(F("ms (stored for next WATER/YES/NO tuning)"));
    }
    pendingKind = MK_NONE;
  }

  if (burstPeak >= helpPeakMin && burstPeak <= helpPeakMax &&
      durMs    >= helpDurMin  && durMs    <= helpDurMax) {
    if (burstStart - lastFire >= REFRACTORY_MS) {
      fireHelp(burstPeak, durMs);
      lastFire = burstStart;
      return;
    }
    Serial.print(F(">>> BURST peak=")); Serial.print(burstPeak);
    Serial.print(F(" dur="));          Serial.print(durMs);
    Serial.println(F("ms -> echo of the last word (skipped)"));
    return;
  }
  Serial.print(F(">>> BURST peak=")); Serial.print(burstPeak);
  Serial.print(F(" dur="));          Serial.print(durMs);
  Serial.print(F("ms -> NOT HELP ("));
  if      (burstPeak < helpPeakMin) Serial.print(F("too weak"));
  else if (burstPeak > helpPeakMax) Serial.print(F("stronger than any learned HELP"));
  else if (durMs < helpDurMin)    Serial.print(F("too short - spike, not a word"));
  else                              Serial.print(F("too long - words+fused noise"));
  Serial.println(F(")"));
}

void sortBuf(int *b, int n) {
  for (int i = 0; i < n - 1; i++)
    for (int j = i + 1; j < n; j++)
      if (b[j] < b[i]) { int tmp = b[i]; b[i] = b[j]; b[j] = tmp; }
}

void runCalibrate() {
  Serial.println(F(">>> CALIBRATE: sit SILENT + STILL, hands off pads... (5 s)"));
  const int N = 100;
  int buf[N];
  for (int i = 0; i < N; i++) {
    unsigned long st = millis();
    while (millis() - st < 50) { sample(); delay(2); }
    buf[i] = env;
  }
  sortBuf(buf, N);
  int med = (buf[N/2 - 1] + buf[N/2]) / 2;
  int p90 = (buf[N - 11] + buf[N - 10]) / 2;
  burstLvl = p90 + 60;
  Serial.print(F(">>> CALIBRATE: silence med=")); Serial.print(med);
  Serial.print(F("  p90=")); Serial.print(p90);
  Serial.print(F("  burst=")); Serial.println(burstLvl);
  Serial.println(F(">>> checking for 3 more seconds..."));
  const int V = 60;
  int vb[V];
  int above = 0;
  for (int i = 0; i < V; i++) {
    unsigned long st = millis();
    while (millis() - st < 50) { sample(); delay(2); }
    vb[i] = env;
    if (env >= burstLvl) above++;
  }
  if (above > V / 5) {
    sortBuf(vb, V);
    int p90v = (vb[V - 7] + vb[V - 6]) / 2;
    if (p90v + 60 > burstLvl) {
      burstLvl = p90v + 60;
      Serial.print(F(">>> STILL NOISY - burst level auto-raised to ")); Serial.println(burstLvl);
      Serial.println(F(">>> pads loose or you moved: fix, then press c again"));
    }
  } else {
    Serial.println(F(">>> CALIBRATE OK"));
  }
  inBurst = false; burstPeak = 0; spikeCount = 0;
  saveBox(); // persist new burstLvl
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LOP, INPUT); pinMode(PIN_LON, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_OUT, ADC_11db);
  delay(800);
  loadBox();
  printBanner();
}

void sample() {
  int raw = analogRead(PIN_OUT);
  lastRaw = raw;
  if (firstSample) { center = raw; firstSample = false; }
  center += (raw - center) / 1024;
  int dev = abs(raw - center);
  if (dev > env) env = dev;
  else           env = (env * 97) / 100;
  if (env > maxEnvEver) maxEnvEver = env;
  ring[ringIdx] = env;
  ringIdx = (ringIdx + 1) % RING;
}

void printLine() {
  int peak = 0;
  for (int i = 0; i < RING; i++) if (ring[i] > peak) peak = ring[i];
  bool padsOn = (digitalRead(PIN_LOP) == LOW && digitalRead(PIN_LON) == LOW);
  Serial.print(F("raw:"));    Serial.print(lastRaw);
  Serial.print(F(",env:"));   Serial.print(env);
  Serial.print(F(",peak:"));  Serial.print(peak);
  Serial.print(F(",pads:"));  Serial.print(padsOn ? 1 : 0);
  Serial.print(F(",thresh:")); Serial.print(helpPeakMin);
  Serial.print(F("  [t="));   Serial.print(millis()/1000.0, 2); Serial.print(F("s]"));
  if (!padsOn) Serial.print(F("  CHECK-PADS!"));
  Serial.println();
}

void tryAutoLearnFromLastBurst(MarkKind kind) {
  if (kind != MK_HELP) return; // only HELP auto-expands the alert box today
  if (lastBurstEnd==0) {
    Serial.println(F(">>> (no burst yet — say HELP first, then press h within 2 s)"));
    return;
  }
  unsigned long age = millis() - lastBurstEnd;
  if (age < 2500) {
    autoLearnHelp(lastBurstPeak, lastBurstDur);
  } else {
    Serial.print(F(">>> (last burst ")); Serial.print(age/1000.0,1);
    Serial.println(F(" s ago — too old. Say HELP again and press h closer to the word.)"));
  }
}

void handleKey(char c) {
  if (c=='\r'||c=='\n'||c=='\t'||c==' ') return;
  if      (c=='h'||c=='H') { mark(F("HELP")); pendingKind=MK_HELP; pendingAt=millis(); tryAutoLearnFromLastBurst(MK_HELP); }
  else if (c=='w'||c=='W') { mark(F("WATER")); pendingKind=MK_WATER; pendingAt=millis(); Serial.println(F(">>> (WATER collected — no HELP alert; will be used to separate words next)")); }
  else if (c=='y'||c=='Y') { mark(F("YES")); pendingKind=MK_YES; pendingAt=millis(); Serial.println(F(">>> (YES collected)")); }
  else if (c=='n'||c=='N') { mark(F("NO")); pendingKind=MK_NO; pendingAt=millis(); Serial.println(F(">>> (NO collected)")); }
  else if (c=='s'||c=='S') { mark(F("SILENT-no-word")); pendingKind=MK_SILENT; pendingAt=millis(); }
  else if (c=='a'||c=='A') { mark(F("HELP")); pendingKind=MK_HELP; pendingAt=millis(); tryAutoLearnFromLastBurst(MK_HELP); recomputeBoxFromTraining(); }
  else if (c=='v'||c=='V') { if (lastBurstEnd!=0 && millis()-lastBurstEnd<2500) tryAutoLearnFromLastBurst(MK_HELP); printTraining(); }
  else if (c=='e'||c=='E'||c=='z'||c=='Z') eraseTraining();
  else if (c=='t'||c=='T') {
    Serial.println(F("############################################################"));
    Serial.println(F(">>>  HELP!  (TEST - key t)"));
    Serial.println(F("############################################################"));
  }
  else if (c=='c'||c=='C') runCalibrate();
  else if (c=='+'||c=='=') { helpPeakMin += 100; if (helpPeakMin>helpPeakMax) helpPeakMin=helpPeakMax; Serial.print(F(">>> HELP box: peak ")); Serial.print(helpPeakMin); Serial.print(F("-")); Serial.println(helpPeakMax); saveBox(); }
  else if (c=='-'||c=='_') { if (helpPeakMin > 100) helpPeakMin -= 100; Serial.print(F(">>> HELP box: peak ")); Serial.print(helpPeakMin); Serial.print(F("-")); Serial.println(helpPeakMax); saveBox(); }
  else if (c=='r'||c=='R') { printMs = (printMs==50)?250:50; Serial.println(printMs==250?F(">>>rate=slow"):F(">>>rate=fast")); }
  else if (c=='i'||c=='I') printBanner();
  else Serial.println(F("keys: c=calibrate  h/w/y/n=mark+auto  a=recompute  v=view  e=erase  +/- edge  t=test  i=info  r=speed"));
}

void loop() {
  unsigned long now = millis();
  if (now - tS >= (unsigned long)SAMPLE_MS) { tS = now; sample(); }

  if (env >= burstLvl) {
    if (!inBurst) { inBurst = true; burstStart = now; burstPeak = env; }
    if (env > burstPeak) burstPeak = env;
    lastHigh = now;
    if (now - burstStart > BURST_MAX_MS) {
      classifyBurst(now - burstStart);
      Serial.println(F(">>> burst >3 s = pads moved or gates too low -> press c while silent"));
      inBurst = false; burstPeak = 0;
    }
  } else if (inBurst && now - lastHigh >= BURST_GRACE_MS) {
    classifyBurst(lastHigh - burstStart);
    inBurst = false; burstPeak = 0;
  }

  if (now - tP >= (unsigned long)printMs) { tP = now; printLine(); }
  while (Serial.available()) handleKey(Serial.read());
}
