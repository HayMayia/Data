/*
 * VocalBridge — HELP DETECTOR (HelpDetect_AD8232)  V1.4
 * ------------------------------------------------------
 * V1.3 lesson (29 Sep, session 3): the green (RL) electrode moved from
 * below the collarbone to the BACK OF THE NECK. Same voice, same word,
 * but the signal HALVED (words 1556-1764 -> 564-854). Fixed thresholds
 * cannot survive a placement change.
 *
 * V1.4 = CALIBRATION + V1.3 shape gate:
 *   - press 'c' while SILENT and STILL: the board measures your silence
 *     level for 5 s and sets both gate levels from it:
 *         burst level = silence + 200   (a burst starts here)
 *         alert level = silence + 300   (peak needed to alert)
 *   - alert fires ONLY if the burst is speech-shaped:
 *        peak >= alert level  AND  burst has lasted >= 250 ms
 *   - short spikes still get a BURST report but NO alert
 *
 * MEASURED 29 Sep (same speaker, same word):
 *   RL below collarbone: silence ~314, words 1556-1764 (5x)   <- stronger
 *   RL back of neck    : silence ~275, words 564-854  (2.3x)  <- current
 *
 * Board: ESP32 Dev Module, Baud 115200. Wiring same as always:
 *   AD8232: 3V3->3V3  GND->GND  OUT->GPIO34  LO+->GPIO32  LO-->GPIO33
 *
 * KEYS:  c calibrate (SILENT!)   +/- alert level x100   t test alert
 *        i info   r fast/slow   h/w/y/n/s marks
 */

const int PIN_OUT = 34, PIN_LOP = 32, PIN_LON = 33;
const int SAMPLE_MS = 2;
int printMs = 50;

long center = 2048;
int  env = 0, lastRaw = 0;
bool firstSample = true;
const int RING = 100;
int ring[RING]; int ringIdx = 0;

// ---------- detector V1.4 (shape gate + calibration) ----------
int  thresh   = 575;   // alert level: peak needed (default: RL back of neck)
int  burstLvl = 475;   // burst starts here  (default: RL back of neck)
const unsigned long BURST_GRACE_MS = 150;  // dips shorter than this stay in the burst
const unsigned long SPEECH_MIN_MS  = 250;  // burst must LAST this long to alert
const unsigned long REFRACTORY_MS  = 1500;

bool inBurst = false, firedThisBurst = false;
unsigned long burstStart = 0, lastHigh = 0, lastFire = 0;
int  burstPeak = 0, maxEnvEver = 0, helpCount = 0;

unsigned long tS = 0, tP = 0;

void printBanner() {
  Serial.println();
  Serial.println(F("==== VocalBridge HELP DETECTOR V1.4 (calibrated shape gate) ===="));
  Serial.println(F("baud=115200"));
  Serial.println(F("alert = peak >= alert level AND burst lasts >= 250 ms"));
  Serial.print(F("burst level=")); Serial.print(burstLvl);
  Serial.print(F("  alert level=")); Serial.print(thresh);
  Serial.println(F("  (defaults: RL on BACK OF NECK)"));
  Serial.println(F("green pad moved? press c while SILENT+STILL -> auto re-tune (5 s)"));
  Serial.println(F("measured: neck words 564-854 / collarbone words 1556-1764"));
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

void runCalibrate() {
  Serial.println(F(">>> CALIBRATE: sit SILENT + STILL, do not talk... (5 s)"));
  const int N = 100;                     // 100 x 50 ms = 5 s
  int buf[N];
  for (int i = 0; i < N; i++) {
    unsigned long st = millis();
    while (millis() - st < 50) { sample(); delay(2); }
    buf[i] = env;
  }
  for (int i = 0; i < N - 1; i++)        // sort small array
    for (int j = i + 1; j < N; j++)
      if (buf[j] < buf[i]) { int tmp = buf[i]; buf[i] = buf[j]; buf[j] = tmp; }
  int med = (buf[N/2 - 1] + buf[N/2]) / 2;
  burstLvl = med + 200;
  thresh   = med + 300;
  inBurst = false; burstPeak = 0; firedThisBurst = false;
  Serial.print(F(">>> CALIBRATE done. silence=")); Serial.print(med);
  Serial.print(F("  burst="));  Serial.print(burstLvl);
  Serial.print(F("  alert="));  Serial.println(thresh);
  Serial.println(F(">>> if you talked during it, press c again"));
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

  // ---------- burst profiler + shape gate (V1.4) ----------
  if (env >= burstLvl) {
    if (!inBurst) { inBurst = true; burstStart = now; burstPeak = env; firedThisBurst = false; }
    if (env > burstPeak) burstPeak = env;
    lastHigh = now;
    unsigned long age = now - burstStart;
    if (!firedThisBurst && env >= thresh &&
        age >= SPEECH_MIN_MS && now - lastFire >= REFRACTORY_MS) {
      fireHelp(age);
      lastFire = now;
      firedThisBurst = true;
    }
  } else if (inBurst && now - lastHigh >= BURST_GRACE_MS) {
    reportBurst(now - burstStart);
    inBurst = false; burstPeak = 0;
  }

  if (now - tP >= (unsigned long)printMs) { tP = now; printLine(); }
  while (Serial.available()) handleKey(Serial.read());
}
