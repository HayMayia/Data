/*
 * VocalBridge — HELP DETECTOR (HelpDetect_AD8232)
 * ------------------------------------------------
 * BUILT FROM YOUR OWN RECORDED DATA (Session 1, 29 Sep):
 *   you said HELP x12 -> every burst peaked at 1556-1764
 *   sitting still      -> env ~314
 *   just moving/typing -> env 400-1000
 * So: env crossing 1200 for at least 150 ms = a HELP-strength
 * speech burst.  That gap (1000 vs 1556) is the detector's home.
 *
 * WHEN IT FIRES: a big ">>> HELP! DETECTED <<<" block in the Monitor.
 * It re-arms about a second after you go quiet again.
 *
 * HONEST NOTE: this currently detects *speech bursts at your HELP
 * loudness*. If you shout WATER it will also fire - we have only
 * recorded HELP so far. After we record WATER / YES / NO we can
 * teach it the difference. One step at a time, no faking.
 *
 * Board: ESP32 Dev Module, Baud 115200. Wiring SAME as before:
 *   AD8232: 3V3->3V3  GND->GND  OUT->GPIO34  LO+->GPIO32  LO-->GPIO33
 *
 * KEYS:  + / -  raise/lower threshold by 100
 *        t  test-fire the HELP alert (for filming)
 *        i  info   r  fast/slow lines
 *        h w y n s  marks (still useful while filming)
 */

const int PIN_OUT = 34, PIN_LOP = 32, PIN_LON = 33;
const int SAMPLE_MS = 2;
int printMs = 50;

long center = 2048;
int  env = 0, lastRaw = 0;
bool firstSample = true;
const int RING = 100;
int ring[RING]; int ringIdx = 0;

// ---------- detector (numbers from Session 1, 29 Sep) ----------
int  thresh = 1200;                 // bursts 1556-1764, noise <=1000
const int MIN_ABOVE_MS = 150;       // burst must last this long
const int REARM_ENV = 500;          // env must sink below this...
const int REARM_MS  = 800;          // ...for this long, to re-arm
unsigned long aboveSince = 0, belowSince = 0;
bool armed = true;
int  burstPeak = 0, helpCount = 0;

unsigned long tS = 0, tP = 0;

void printBanner() {
  Serial.println();
  Serial.println(F("==== VocalBridge HELP DETECTOR (tuned from your Session-1 data) ===="));
  Serial.println(F("version=DetectV1  baud=115200"));
  Serial.println(F("firing rule: env >= threshold for 150 ms  ->  '>>> HELP! DETECTED <<<'"));
  Serial.print(F("threshold=")); Serial.print(thresh);
  Serial.println(F("  (your HELP bursts: 1556-1764, silence ~314)"));
  Serial.println(F("keys: +/- threshold, t=test alert, i=info, r=speed, h/w/y/n/s=marks"));
  Serial.println(F("say it at the SAME loudness as Session 1; quieter = lower threshold with '-'"));
  Serial.println(F("plotter: raw,env,peak,pads,thresh  (watch env cross the thresh line)"));
  Serial.println(F("streaming..."));
}

void mark(const __FlashStringHelper *w) {
  Serial.print(F(">>>MARK:")); Serial.print(w);
  Serial.print(F(" t=")); Serial.print(millis()/1000.0, 2); Serial.println(F("s<<<"));
}

void fireHelp(bool test) {
  Serial.println(F("############################################################"));
  if (test) {
    Serial.print(F(">>>  HELP!  (TEST - key t)"));
  } else {
    helpCount++;
    Serial.print(F(">>>  HELP!  DETECTED  #"));
    Serial.print(helpCount);
    Serial.print(F("   peak=")); Serial.print(burstPeak);
  }
  Serial.print(F("   t=")); Serial.print(millis()/1000.0, 2); Serial.println(F("s"));
  Serial.println(F("############################################################"));
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
  else if (c=='t'||c=='T') fireHelp(true);
  else if (c=='+'||c=='=') { thresh += 100; Serial.print(F(">>> threshold=")); Serial.println(thresh); }
  else if (c=='-'||c=='_') { thresh -= 100; Serial.print(F(">>> threshold=")); Serial.println(thresh); }
  else if (c=='r'||c=='R') { printMs = (printMs==50)?250:50; Serial.println(printMs==250?F(">>>rate=slow"):F(">>>rate=fast")); }
  else if (c=='i'||c=='I') printBanner();
  else Serial.println(F("keys: +/- thresh, t=test, i=info, r=speed, marks h w y n s"));
}

void loop() {
  unsigned long now = millis();
  if (now - tS >= (unsigned long)SAMPLE_MS) { tS = now; sample(); }

  // ---------- detector state machine ----------
  if (env >= thresh) {
    if (aboveSince == 0) { aboveSince = now; burstPeak = env; }
    else if (env > burstPeak) burstPeak = env;
    belowSince = 0;
    if (armed && now - aboveSince >= (unsigned long)MIN_ABOVE_MS) {
      fireHelp(false);
      armed = false;
    }
  } else {
    aboveSince = 0; burstPeak = 0;
    if (!armed) {
      if (env < REARM_ENV) {
        if (belowSince == 0) belowSince = now;
        else if (now - belowSince >= (unsigned long)REARM_MS) {
          armed = true; belowSince = 0;
          Serial.println(F(">>> detector re-armed"));
        }
      } else belowSince = 0;
    }
  }

  if (now - tP >= (unsigned long)printMs) { tP = now; printLine(); }
  while (Serial.available()) handleKey(Serial.read());
}
