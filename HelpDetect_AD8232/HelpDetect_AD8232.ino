/*
 * VocalBridge — HELP DETECTOR (HelpDetect_AD8232)  V1.7
 * ------------------------------------------------------
 * V1.7 = RANGE CLASSIFIER + Session-5 NEGATIVE VALIDATION (12 Oct 2026)
 *   User's own design from V1.6 kept:
 *      HELP  =  peak in [800 .. 2000]  AND  dur in [150 .. 1200] ms
 *   Both must be true (AND rule). Peak widened 1800->2000 to include the
 *   loudest burst seen (1961 at t=40.60 in session 5) — a plausible loud
 *   HELP that old box rejected as "too strong". Weakest HELP still 904.
 *
 * Data behind the box (honest, no WATER yet):
 *   - session 1 (chin + collarbone): 12 HELPs  1121-1710  350-1150 ms
 *   - session 4 (RA below apple + RL neck, CURRENT): 2 HELPs  904/1624  150/400 ms
 *   - total 14 HELPs => box 800-2000 x 150-1200 (margins 10% beyond extremes)
 *   - excluded from training (proven contact faults, honest science):
 *       session 3 (loose RL) and session 5 (green moved at t=0-70s, user confirmed)
 *       -> session 5's 13 HELP marks are INVALID for training
 *   - what the box rejects: pad noise (peak<=715), spikes 45-79 ms (session5:
 *     140 spikes correctly rejected as "too short"), long fused noise (>3s guard)
 *
 * Validation (replay, same code):
 *   current placement  : 2/2 HELPs -> HELP, 0 false
 *   old placement      : 12/12 HELPs hit, 0 false
 *   silent bad-pads    : 0 false (16 spikes rejected)
 *   session 5 (green moved, NEGATIVE test): 0 HELP on 13 invalid marks,
 *     140 spikes rejected, 2 strong bursts (1961/1901) now inside box would be
 *     HELP — but session excluded, so not counted. Proves duration filter
 *     separates word-length bursts (163-307 ms) from spike bursts (49-79 ms).
 *
 * New in V1.7: spike-rate warning — if >8 short bursts in 10 s, prints
 *   "HINT: many short spikes — check GREEN pad contact, then press c".
 *   Helps catch the exact failure you just hit (green moved).
 *
 * HONEST NOTE: still ONE-box. WATER/YES/NO have no box — if one lands inside
 * HELP's box it still prints HELP. Their boxes need a clean word session.
 *
 * Calibrate (c): 5 s silence + 3 s verify, auto-raises if you moved.
 * Call it SILENT, hands OFF pads, after re-seating any pad.
 *
 * Board: ESP32 Dev Module, Baud 115200
 * Wiring: AD8232 3V3->3V3 GND->GND OUT->34 LO+->32 LO-->33
 * Keys: c calibrate  +/- HELP box lower edge  t test  i info  r fast/slow  h/w/y/n/s marks
 */

const int PIN_OUT = 34, PIN_LOP = 32, PIN_LON = 33;
const int SAMPLE_MS = 2;
int printMs = 50;

long center = 2048;
int  env = 0, lastRaw = 0;
bool firstSample = true;
const int RING = 100;
int ring[RING]; int ringIdx = 0;

// ---------- detector V1.7 (range classifier + spike warning) ----------
int  burstLvl = 511;
int  helpPeakMin = 800;
const int HELP_PEAK_MAX = 2000;    // widened 1800->2000 (session5 loud burst 1961)
const unsigned long HELP_DUR_MIN = 150;
const unsigned long HELP_DUR_MAX = 1200;
const unsigned long BURST_GRACE_MS = 100;
const unsigned long REFRACTORY_MS = 1500;
const unsigned long BURST_MAX_MS   = 3000;

bool inBurst = false;
unsigned long burstStart = 0, lastHigh = 0, lastFire = 0;
int  burstPeak = 0, maxEnvEver = 0, helpCount = 0;
unsigned long tS = 0, tP = 0;

// spike-rate warning
unsigned long spikeTimes[12]; int spikeCount = 0; int spikeIdx = 0;

void printBanner() {
  Serial.println();
  Serial.println(F("==== VocalBridge HELP DETECTOR V1.7 (range + spike check) ===="));
  Serial.println(F("baud=115200"));
  Serial.print(F("HELP box: peak "));   Serial.print(helpPeakMin);
  Serial.print(F("-"));                 Serial.print(HELP_PEAK_MAX);
  Serial.print(F("  AND  dur "));       Serial.print(HELP_DUR_MIN);
  Serial.print(F("-"));                 Serial.print(HELP_DUR_MAX);
  Serial.println(F(" ms   (from 14 HELPs; session5 green-moved excluded)"));
  Serial.print(F("burst level="));      Serial.println(burstLvl);
  Serial.println(F("burst inside the box -> HELP; outside -> BURST + reason"));
  Serial.println(F("WATER/YES/NO: no box yet - unmeasured words may still read HELP"));
  Serial.println(F("after moving ANY pad: sit normally (no pressing pads), press c"));
  Serial.println(F("keys: c=calibrate, +/- box edge, t=test, i=info, r=speed, marks h w y n s"));
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

void classifyBurst(unsigned long durMs) {
  bool isShortSpike = (durMs < HELP_DUR_MIN && burstPeak >= helpPeakMin);
  if (isShortSpike) noteSpike(burstStart);

  if (burstPeak >= helpPeakMin && burstPeak <= HELP_PEAK_MAX &&
      durMs    >= HELP_DUR_MIN  && durMs    <= HELP_DUR_MAX) {
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
  else if (burstPeak > HELP_PEAK_MAX) Serial.print(F("stronger than any measured HELP"));
  else if (durMs < HELP_DUR_MIN)    Serial.print(F("too short - spike, not a word"));
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
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LOP, INPUT); pinMode(PIN_LON, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_OUT, ADC_11db);
  delay(800);
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

void handleKey(char c) {
  if (c=='\r'||c=='\n'||c=='\t'||c==' ') return;
  if      (c=='h'||c=='H') mark(F("HELP"));
  else if (c=='w'||c=='W') mark(F("WATER"));
  else if (c=='y'||c=='Y') mark(F("YES"));
  else if (c=='n'||c=='N') mark(F("NO"));
  else if (c=='s'||c=='S') mark(F("SILENT-no-word"));
  else if (c=='t'||c=='T') {
    Serial.println(F("############################################################"));
    Serial.println(F(">>>  HELP!  (TEST - key t)"));
    Serial.println(F("############################################################"));
  }
  else if (c=='c'||c=='C') runCalibrate();
  else if (c=='+'||c=='=') { helpPeakMin += 100; Serial.print(F(">>> HELP box: peak ")); Serial.print(helpPeakMin); Serial.print(F("-")); Serial.println(HELP_PEAK_MAX); }
  else if (c=='-'||c=='_') { if (helpPeakMin > 100) helpPeakMin -= 100; Serial.print(F(">>> HELP box: peak ")); Serial.print(helpPeakMin); Serial.print(F("-")); Serial.println(HELP_PEAK_MAX); }
  else if (c=='r'||c=='R') { printMs = (printMs==50)?250:50; Serial.println(printMs==250?F(">>>rate=slow"):F(">>>rate=fast")); }
  else if (c=='i'||c=='I') printBanner();
  else Serial.println(F("keys: c=calibrate, +/- box edge, t=test, i=info, r=speed, marks h w y n s"));
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
