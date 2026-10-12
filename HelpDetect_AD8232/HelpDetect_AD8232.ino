/*
 * VocalBridge — HELP DETECTOR (HelpDetect_AD8232)  V1.6
 * ------------------------------------------------------
 * V1.6 = THE RANGE CLASSIFIER (user's own design, 29 Sep):
 *   "it should be a RANGE of values... if the values are in range for
 *    HELP it will say HELP, otherwise it will not."
 * Implemented exactly like that. Every burst is measured on TWO features
 * and must land inside HELP's measured box (BOTH, like the user's AND rule):
 *
 *      HELP  =  peak in [800 .. 1800]  AND  dur in [150 .. 1200] ms
 *
 * Validated by replay on every recorded session (29 Sep):
 *   current placement : 2/2 words -> HELP, 0 false
 *   old placement     : 12/12 words hit, 0 false
 *   silent bad-pads   : 0 false (all 16 spikes rejected: 50-100 ms = too short)
 * A word that splits into two bursts alerts ONCE (1.5 s echo rule).
 *
 * The box comes ONLY from recorded HELP data (WATER/YES/NO deliberately
 * NOT used - they have no box yet):
 *   - session 1 (RA/LA chin + RL collarbone): 12 HELPs, peak 1121-1710, dur 350-1150 ms
 *   - session 4 (RA below apple + RL neck - CURRENT): 2 HELPs, peak 904 & 1624, dur 150 & 400 ms
 *   - excluded: session 3 (loose RL pad - proven contact fault) and 2 marks with
 *     no burst above noise (word not confirmable)
 *   - what the box rejects: pad noise (peak <= 715), motion spikes (50-100 ms long),
 *     long fused noise (guard cuts bursts at 3 s -> "too long")
 *
 * HONEST NOTE (unchanged since V1.0): this is a ONE-box classifier. WATER, YES
 * and NO are not measured yet - if one of them happens to land inside HELP's
 * box, it will still print HELP. Their boxes get added after the word session.
 *
 * Calibrate (key c) works like V1.5: measures 5 s of silence, sets the burst
 * level, then verifies 3 s more and auto-raises if you were not still.
 * Classification happens when the burst ENDS (~0.2 s after you finish the word).
 *
 * Board: ESP32 Dev Module, Baud 115200. Wiring same as always:
 *   AD8232: 3V3->3V3  GND->GND  OUT->GPIO34  LO+->GPIO32  LO-->GPIO33
 *
 * KEYS:  c calibrate (SILENT, hands off pads!)   +/- HELP box lower edge x100
 *        t test alert   i info   r fast/slow   h/w/y/n/s marks
 */

const int PIN_OUT = 34, PIN_LOP = 32, PIN_LON = 33;
const int SAMPLE_MS = 2;
int printMs = 50;

long center = 2048;
int  env = 0, lastRaw = 0;
bool firstSample = true;
const int RING = 100;
int ring[RING]; int ringIdx = 0;

// ---------- detector V1.6 (range classifier) ----------
int  burstLvl = 511;               // burst starts here (calibrated default)
int  helpPeakMin = 800;            // HELP box: lower peak edge (weakest HELP 904, noise 715)
const int HELP_PEAK_MAX = 1800;    // HELP box: upper peak edge (strongest HELP 1710)
const unsigned long HELP_DUR_MIN = 150;   // HELP box: shortest word 150 ms (spikes 50-100)
const unsigned long HELP_DUR_MAX = 1200;  // HELP box: longest single word 1150 ms
const unsigned long BURST_GRACE_MS = 100; // dips <100 ms forgiven; spike PAIRS split apart
const unsigned long REFRACTORY_MS = 1500; // a word that splits in two alerts only once
const unsigned long BURST_MAX_MS   = 3000;// longer than this = not a word (fusion guard)

bool inBurst = false;
unsigned long burstStart = 0, lastHigh = 0, lastFire = 0;
int  burstPeak = 0, maxEnvEver = 0, helpCount = 0;

unsigned long tS = 0, tP = 0;

void printBanner() {
  Serial.println();
  Serial.println(F("==== VocalBridge HELP DETECTOR V1.6 (range classifier) ===="));
  Serial.println(F("baud=115200"));
  Serial.print(F("HELP box: peak "));   Serial.print(helpPeakMin);
  Serial.print(F("-"));                 Serial.print(HELP_PEAK_MAX);
  Serial.print(F("  AND  dur "));       Serial.print(HELP_DUR_MIN);
  Serial.print(F("-"));                 Serial.print(HELP_DUR_MAX);
  Serial.println(F(" ms   (from 14 recorded HELPs)"));
  Serial.print(F("burst level="));      Serial.println(burstLvl);
  Serial.println(F("burst inside the box -> HELP; outside -> BURST + reason"));
  Serial.println(F("WATER/YES/NO: no box yet - unmeasured words may still read HELP"));
  Serial.println(F("after moving ANY pad: sit normally (no pressing pads), press c"));
  Serial.println(F("keys: c=calibrate, +/- box edge, t=test, i=info, r=speed, marks h w y n s"));
  Serial.println(F("STATUE TEST: sit silent+still 20 s - HELP alerts during it = wire/pad problem"));
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

void classifyBurst(unsigned long durMs) {
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
  const int N = 100;                     // 100 x 50 ms = 5 s
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
  const int V = 60;                      // 60 x 50 ms = 3 s verify
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
  inBurst = false; burstPeak = 0;
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

  // ---------- burst profiler + range classifier (V1.6) ----------
  if (env >= burstLvl) {
    if (!inBurst) { inBurst = true; burstStart = now; burstPeak = env; }
    if (env > burstPeak) burstPeak = env;
    lastHigh = now;
    if (now - burstStart > BURST_MAX_MS) {      // fusion guard
      classifyBurst(now - burstStart);          // will read "too long"
      Serial.println(F(">>> burst >3 s = pads moved or gates too low -> press c while silent"));
      inBurst = false; burstPeak = 0;           // restart fresh next sample
    }
  } else if (inBurst && now - lastHigh >= BURST_GRACE_MS) {
    classifyBurst(lastHigh - burstStart);       // true time above burst level
    inBurst = false; burstPeak = 0;
  }

  if (now - tP >= (unsigned long)printMs) { tP = now; printLine(); }
  while (Serial.available()) handleKey(Serial.read());
}
