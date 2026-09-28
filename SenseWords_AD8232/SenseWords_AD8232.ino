/*
 * VocalBridge — SENSE WORDS (SenseWords_AD8232)
 * ------------------------------------------------
 * DATA RECORDING ONLY. This sketch NEVER decides what word was said.
 * It only shows the electricity your speech muscles make, so we can
 * study the readings together (you film the Serial Monitor + Plotter).
 *
 * Board: ESP32 Dev Module        Baud: 115200
 * No speaker, no amp needed — sensor only.
 *
 * WIRING (AD8232 — same as the arm demo):
 *   3V3 -> 3V3      GND -> GND
 *   OUT  -> GPIO34  LO+ -> GPIO32  LO- -> GPIO33
 *
 * PLOTTER (Arduino IDE 2.x): 4 labelled curves — raw, env, peak, pads.
 * MONITOR: same numbers + a timestamp on every line.
 * KEYS (type in Serial Monitor):
 *   h = mark "I just said HELP"     w = mark WATER
 *   y = mark YES                    n = mark NO
 *   s = mark "silence (said nothing)"
 *   r = fast/slow lines             i = reprint this info
 *
 * Pads: see pad_placement.png / PAD_PLACEMENT.md in this folder.
 */

const int PIN_OUT = 34;   // AD8232 OUT  (ADC1_CH6, input-only pin)
const int PIN_LOP = 32;   // LO+ (HIGH = pad detached)
const int PIN_LON = 33;   // LO- (HIGH = pad detached)

const int SAMPLE_MS = 2;  // internal sampling ~500 Hz
int printMs = 50;         // stream 20 lines/s (press r for slow, 4 lines/s)

long center = 2048;       // slow baseline, follows drift by itself
int  env    = 0;          // peak-hold envelope of the muscle burst
int  lastRaw = 0;
bool firstSample = true;

const int RING = 100;     // ~200 ms of envelope history -> "peak"
int ring[RING];
int ringIdx = 0;

unsigned long tS = 0, tP = 0;

// ---------- boot banner (written so the Plotter ignores every line) ----------
void printBanner() {
  Serial.println();
  Serial.println(F("==== VocalBridge SENSE-WORDS (record-only, no deciding) ===="));
  Serial.println(F("version=SenseV1   baud=115200"));
  Serial.println(F("wiring: OUT=GPIO34,LO+=GPIO32,LO-=GPIO33,VIN=3V3,GND=GND"));
  Serial.println(F("pads: RA and LA under the chin (two finger-widths apart), RL on the chest bone"));
  Serial.println(F("plotter curves: raw,env,peak,pads   (PADS must stay one)"));
  Serial.println(F("mark keys: h=HELP,w=WATER,y=YES,n=NO,s=silent"));
  Serial.println(F("keys: r=fast/slow lines, i=this info"));
  Serial.println(F("keep face relaxed the first seconds -> baseline settles"));
  Serial.println(F("streaming..."));
}

void mark(const __FlashStringHelper *w) {
  Serial.print(F(">>>MARK:"));
  Serial.print(w);
  Serial.print(F(" t="));
  Serial.print(millis() / 1000.0, 2);
  Serial.println(F("s<<<"));
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LOP, INPUT);
  pinMode(PIN_LON, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_OUT, ADC_11db);
  delay(800);
  printBanner();
}

// ---------- one ADC sample: baseline + envelope (display maths only) ----------
void sample() {
  int raw = analogRead(PIN_OUT);
  lastRaw = raw;
  if (firstSample) { center = raw; firstSample = false; }
  center += (raw - center) / 1024;       // ~2 s time constant at 500 Hz
  int dev = abs(raw - center);
  if (dev > env) env = dev;              // rise instantly
  else       env = (env * 97) / 100;     // fall in ~130 ms
  ring[ringIdx] = env;
  ringIdx = (ringIdx + 1) % RING;
}

// ---------- one stream line: works in Monitor AND Plotter ----------
void printLine() {
  int peak = 0;
  for (int i = 0; i < RING; i++) if (ring[i] > peak) peak = ring[i];
  bool padsOn = (digitalRead(PIN_LOP) == LOW && digitalRead(PIN_LON) == LOW);

  Serial.print(F("raw:"));    Serial.print(lastRaw);
  Serial.print(F(",env:"));   Serial.print(env);
  Serial.print(F(",peak:"));  Serial.print(peak);
  Serial.print(F(",pads:"));  Serial.print(padsOn ? 1 : 0);
  Serial.print(F("  [t="));   Serial.print(millis() / 1000.0, 2); Serial.print(F("s]"));
  if (!padsOn) Serial.print(F("  CHECK-PADS!"));
  Serial.println();
}

void handleKey(char c) {
  if (c == '\r' || c == '\n' || c == '\t' || c == ' ') return;
  if      (c == 'h' || c == 'H') mark(F("HELP"));
  else if (c == 'w' || c == 'W') mark(F("WATER"));
  else if (c == 'y' || c == 'Y') mark(F("YES"));
  else if (c == 'n' || c == 'N') mark(F("NO"));
  else if (c == 's' || c == 'S') mark(F("SILENT-no-word"));
  else if (c == 'r' || c == 'R') {
    printMs = (printMs == 50) ? 250 : 50;
    Serial.println(printMs == 250 ? F(">>>rate=slow") : F(">>>rate=fast"));
  }
  else if (c == 'i' || c == 'I') printBanner();
  else Serial.println(F("keys: h w y n s = mark a word, r = speed, i = info"));
}

void loop() {
  unsigned long now = millis();
  if (now - tS >= (unsigned long)SAMPLE_MS) { tS = now; sample(); }
  if (now - tP >= (unsigned long)printMs)   { tP = now; printLine(); }
  while (Serial.available()) handleKey(Serial.read());
}
