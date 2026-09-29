/*
 * VocalBridge — HELP DETECTOR (HelpDetect_AD8232)  V1.5
 * ------------------------------------------------------
 * V1.4 lesson (29 Sep, session 4): user pressed the pad while calibrating
 * (silence read 92), then let go -> resting baseline rose to ~350-480, which
 * is ABOVE the alert level 392 -> the detector fired on baseline noise, and
 * bursts fused into multi-second "mega-bursts" that swallowed the real words.
 *
 * V1.5 = CALIBRATION THAT CHECKS ITSELF:
 *   - press 'c' while SILENT and STILL (do NOT hold the pads - sit exactly
 *     how you will sit when speaking). It measures 5 s of silence and sets:
 *         burst level = p90 + 60    (p90 = the level noise rarely exceeds)
 *         alert level = p90 + 250
 *   - then it VERIFIES for 3 more seconds: if the signal is still above the
 *     burst level, it auto-raises the gates and tells you to calibrate again.
 *   - LONG-BURST GUARD: a burst older than 3 s is cut off with a message -
 *     real words last 0.3-1 s; a 3 s "burst" means pads moved or gates are
 *     too low. This stops mega-burst fusion.
 *   - alert fires ONLY if: peak >= alert level AND burst lasted >= 250 ms.
 *
 * MEASURED 29 Sep (same speaker, same word):
 *   RA below Adam's apple (current): words 904-1624 env, strongest yet
 *   RA on the side + RL neck        : words 564-854
 *   RA/LA chin + RL collarbone      : words 1556-1764
 *
 * Board: ESP32 Dev Module, Baud 115200. Wiring same as always:
 *   AD8232: 3V3->3V3  GND->GND  OUT->GPIO34  LO+->GPIO32  LO-->GPIO33
 *
 * KEYS:  c calibrate (SILENT, no pressing pads!)   +/- alert level x100
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

// ---------- detector V1.5 (self-checking calibration + shape gate) ----------
int  thresh   = 701;   // alert level (default: p90 451 + 250, session-4 baseline)
int  burstLvl = 511;   // burst starts here (default: p90 451 + 60)
const unsigned long BURST_GRACE_MS = 150;  // dips shorter than this stay in the burst
const unsigned long SPEECH_MIN_MS  = 250;  // burst must LAST this long to alert
const unsigned long REFRACTORY_MS  = 1500;
const unsigned long BURST_MAX_MS   = 3000; // longer than this = not a word

bool inBurst = false, firedThisBurst = false;
unsigned long burstStart = 0, lastHigh = 0, lastFire = 0;
int  burstPeak = 0, maxEnvEver = 0, helpCount = 0;

unsigned long tS = 0, tP = 0;

void printBanner() {
  Serial.println();
  Serial.println(F("==== VocalBridge HELP DETECTOR V1.5 (self-checking calibration) ===="));
  Serial.println(F("baud=115200"));
  Serial.println(F("alert = peak >= alert level AND burst lasts >= 250 ms"));
  Serial.print(F("burst level=")); Serial.print(burstLvl);
  Serial.print(F("  alert level=")); Serial.print(thresh);
  Serial.println(F("  (defaults: RA below apple + RL neck)"));
  Serial.println(F("after moving ANY pad: sit normally (no pressing pads), press c"));
  Serial.println(F("measured: below-apple words 904-1624 / side words 564-854"));
  Serial.println(F("keys: c=calibrate, +/- alert, t=test, i=info, r=speed, marks h w y n s"));
  Serial.println(F("STATUE TEST: sit silent+still 20 s - alerts during it = wire/pad problem"));
  Serial.println(F("plotter: raw,env,peak,pads,thresh"));
  Serial.println(F("streaming..."));
}

void mark(const __FlashStringHelper *w) {
  Serial.print(F(">>>MARK:")); Serial.print(w);
  Serial.print(F(" t=")); Serial.print(millis()/1000.0, 2); Serial.println(F("s<<<"));
}

void fireHelp(unsigned long durMs) {
  helpCount++;
  Serial.println(F("############################################################"));
  Serial.print(F(">>>  HELP!  DETECTED  #")); Serial.print(helpCount);
  Serial.print(F("   peak=")); Serial.print(burstPeak);
  Serial.print(F("   dur=")); Serial.print(durMs);
  Serial.print(F("ms   max=")); Serial.print(maxEnvEver);
  Serial.print(F("   t=")); Serial.print(millis()/1000.0, 2); Serial.println(F("s"));
  Serial.println(F("############################################################"));
}

void reportBurst(unsigned long durMs) {
  Serial.print(F(">>> BURST peak=")); Serial.print(burstPeak);
  Serial.print(F(" dur=")); Serial.print(durMs);
  Serial.println(F("ms alert=no (too short or too weak - logged for study)"));
}

void sortBuf(int *b, int n) {
  for (int i = 0; i < n - 1; i++)
    for (int j = i + 1; j < n; j++)
      if (b[j] < b[i]) { int tmp = b[i]; b[i] = b[j]; b[j] = tmp; }
}

void collect(int n, int *buf) {          // n samples, one every 50 ms
  for (int i = 0; i < n; i++) {
    unsigned long st = millis();
    while (millis() - st < 50) { sample(); delay(2); }
    buf[i] = env;
  }
}

void runCalibrate() {
  Serial.println(F(">>> CALIBRATE: sit SILENT + STILL, hands off pads... (5 s)"));
  const int N = 100;                     // 100 x 50 ms = 5 s
  int buf[N];
  collect(N, buf);
  sortBuf(buf, N);
  int med = (buf[N/2 - 1] + buf[N/2]) / 2;
  int p90 = (buf[N - 11] + buf[N - 10]) / 2;   // 90th percentile
  burstLvl = p90 + 60;
  thresh   = p90 + 250;
  Serial.print(F(">>> CALIBRATE: silence med=")); Serial.print(med);
  Serial.print(F("  p90=")); Serial.print(p90);
  Serial.print(F("  burst=")); Serial.print(burstLvl);
  Serial.print(F("  alert=")); Serial.println(thresh);
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
  if (above > V / 5) {                   // more than 20% above burst = too noisy
    sortBuf(vb, V);
    int p90v = (vb[V - 7] + vb[V - 6]) / 2;
    if (p90v + 60 > burstLvl) {
      burstLvl = p90v + 60;
      thresh   = p90v + 250;
      Serial.print(F(">>> STILL NOISY - gates auto-raised: burst=")); Serial.print(burstLvl);
      Serial.print(F(" alert=")); Serial.println(thresh);
      Serial.println(F(">>> pads loose or you moved: fix, then press c again"));
    }
  } else {
    Serial.println(F(">>> CALIBRATE OK"));
  }
  inBurst = false; burstPeak = 0; firedThisBurst = false;
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
  Serial.print(F(",thresh:")); Serial.print(thresh);
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
  else if (c=='+'||c=='=') { thresh += 100; Serial.print(F(">>> threshold=")); Serial.println(thresh); }
  else if (c=='-'||c=='_') { thresh -= 100; Serial.print(F(">>> threshold=")); Serial.println(thresh); }
  else if (c=='r'||c=='R') { printMs = (printMs==50)?250:50; Serial.println(printMs==250?F(">>>rate=slow"):F(">>>rate=fast")); }
  else if (c=='i'||c=='I') printBanner();
  else Serial.println(F("keys: c=calibrate, +/- thresh, t=test, i=info, r=speed, marks h w y n s"));
}

void loop() {
  unsigned long now = millis();
  if (now - tS >= (unsigned long)SAMPLE_MS) { tS = now; sample(); }

  // ---------- burst profiler + shape gate (V1.5) ----------
  if (env >= burstLvl) {
    if (!inBurst) { inBurst = true; burstStart = now; burstPeak = env; firedThisBurst = false; }
    if (env > burstPeak) burstPeak = env;
    lastHigh = now;
    unsigned long age = now - burstStart;
    if (age > BURST_MAX_MS) {             // mega-burst guard
      reportBurst(age);
      Serial.println(F(">>> burst >3 s = pads moved or gates too low -> press c while silent"));
      inBurst = false; burstPeak = 0;     // restart fresh next sample
    } else {
      if (!firedThisBurst && env >= thresh &&
          age >= SPEECH_MIN_MS && now - lastFire >= REFRACTORY_MS) {
        fireHelp(age);
        lastFire = now;
        firedThisBurst = true;
      }
    }
  } else if (inBurst && now - lastHigh >= BURST_GRACE_MS) {
    reportBurst(now - burstStart);
    inBurst = false; burstPeak = 0;
  }

  if (now - tP >= (unsigned long)printMs) { tP = now; printLine(); }
  while (Serial.available()) handleKey(Serial.read());
}
