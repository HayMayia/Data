/*
 * VocalBridge — CLENCH DETECTOR v12 (v11 + SPEAKER = full demo)
 * --------------------------------------------------------------
 * This is the whole Phase-1 demo in ONE sketch:
 *
 *   squeeze -> ">>> CLENCH! <<<" on screen  AND  speaker says "Help"
 *
 * v12 = v11's weak-signal detection (0.3 s windows, fires on 2 quick
 * lines OR one big spike) + DFPlayer Mini wired to Serial2.
 * If the DFPlayer is missing/broken it still runs as a silent v11 —
 * nothing is lost.
 *
 * Board:  ESP32 Dev Module
 * Baud:   115200 (Serial Monitor)
 * Library: "DFRobotDFPlayerMini" (Arduino Library Manager)
 *
 * WIRING (both devices at once):
 *
 *   AD8232 (same as always):
 *     3.3V -> 3V3          GND -> GND          OUTPUT -> GPIO34
 *     (LO+ -> GPIO32, LO- -> GPIO33 optional, same as MuscleTest v4)
 *
 *   DFPlayer Mini:
 *     VCC -> VIN (5V)      GND -> GND
 *     RX  -> GPIO26        TX  -> GPIO27       (they cross — check twice!)
 *     Speaker -> SPK_1 and SPK_2 (never to GND)
 *     microSD -> FAT32, folder "mp3" in root, 0001.mp3 = "Help"
 *               (if you add more: 0002 Yes, 0003 No, 0004 Water, 0005 Pain)
 *
 * KEEP THE SPEAKER WIRES AWAY FROM THE ELECTRODE WIRES — the speaker
 * current can inject noise into the AD8232. Short speaker wires, no
 * crossing over the pads.
 *
 * DEMO PROTOCOL (same trick as always):
 *   1) Arm FLAT and still on the table. Sit still 5 s while it learns.
 *   2) It prints ">>> ARMED". NOW: squeeze a soft ball (or hard fist)
 *      and HOLD 2-3 s, then FULLY relax and keep still.
 *   3) Screen: CLENCH! + speaker: "Help". Wait for ">>> ARMED" before
 *      the next squeeze.
 */

#include <DFRobotDFPlayerMini.h>

const int SENSOR_PIN = 34;
const int BAUD = 115200;

// DFPlayer on Serial2: ESP32 RX=27 <- DFPlayer TX, ESP32 TX=26 -> DFPlayer RX
#define DF_RX_PIN 27
#define DF_TX_PIN 26

const int N = 30;               // 30 samples * 10ms = 0.3 s per line
const int LEARN_MS = 5000;      // 5 s calibration window
const int MAX_LEARN = 24;       // stored learning samples (0.3 s each)
const int FIRE_STREAK = 2;      // 2 lines (0.6 s) above threshold -> fire
const int BIG_SPIKE_MULT = 3;   // dev > threshold*this -> fire instantly (1 line)
const int SETTLE_LINES = 3;     // ~0.9 s of calm required before re-arming
const int REPLAY_GAP_MS = 2000; // never say "Help" more than once per 2 s

int learnSamples[MAX_LEARN];
int learnCount = 0;
unsigned long learnStart = 0;

float baseline = 0;
int THRESHOLD = 200;
bool learned = false;

enum State { ST_ARMED, ST_RISING, ST_SETTLE };
State state = ST_ARMED;
int streak = 0;
int quietStreak = 0;
int sessionMaxDev = 0;

// ---- speaker ----
DFRobotDFPlayerMini player;
bool speakerOnline = false;
unsigned long lastPlayMs = 0;

int readAvg() {
  long sum = 0;
  for (int i = 0; i < N; i++) {
    sum += analogRead(SENSOR_PIN);
    delay(10);
  }
  return (int)(sum / N);
}

void sortArr(int *a, int n) {
  for (int i = 1; i < n; i++) {
    int v = a[i];
    int j = i - 1;
    while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; }
    a[j + 1] = v;
  }
}

// one place for the CLENCH event: print + speak
void clenchFired() {
  Serial.println(">>> CLENCH! <<<");
  if (speakerOnline && (millis() - lastPlayMs) > REPLAY_GAP_MS) {
    player.playMp3Folder(1);            // mp3/0001.mp3 = "Help"
    lastPlayMs = millis();
    Serial.println(">>> SPEAKER: 'Help'");
  }
}

void setup() {
  Serial.begin(BAUD);
  delay(800);
  analogReadResolution(12);
  analogSetPinAttenuation(SENSOR_PIN, ADC_11db);
  pinMode(SENSOR_PIN, INPUT);

  Serial.println("VocalBridge_ClenchDetector_v12 (clench -> speaker says HELP)");

  // ---- speaker init (silent if missing) ----
  Serial2.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);
  for (int i = 1; i <= 3 && !speakerOnline; i++) {
    speakerOnline = player.begin(Serial2, /*isACK*/ true, /*doReset*/ true);
    if (!speakerOnline) delay(400);
  }
  if (speakerOnline) {
    player.volume(20);
    delay(150);
    Serial.println(">> SPEAKER ONLINE — squeeze will say 'Help'.");
  } else {
    Serial.println(">> SPEAKER OFFLINE (wiring/SD?) — running silent as v11.");
  }

  Serial.println();
  Serial.println("SIT STILL 5 s (arm FLAT). Learning...");
  Serial.println();
  learnStart = millis();          // learn window starts AFTER speaker init
}

void loop() {
  int avg = readAvg();

  // ---------------- CALIBRATION ----------------
  if (!learned) {
    if (learnCount < MAX_LEARN) learnSamples[learnCount++] = avg;

    Serial.print("learning ");
    Serial.print((millis() - learnStart) / 1000);
    Serial.print("s  avg=");
    Serial.println(avg);

    if (millis() - learnStart >= LEARN_MS && learnCount >= 8) {
      int sorted[MAX_LEARN];
      for (int i = 0; i < learnCount; i++) sorted[i] = learnSamples[i];
      sortArr(sorted, learnCount);

      int med = sorted[learnCount / 2];
      baseline = med;

      int devs[MAX_LEARN];
      for (int i = 0; i < learnCount; i++) {
        int d = sorted[i] - med;
        if (d < 0) d = -d;
        devs[i] = d;
      }
      sortArr(devs, learnCount);
      int mad = devs[learnCount / 2];

      THRESHOLD = mad * 4;
      if (THRESHOLD < 150) THRESHOLD = 150;
      if (THRESHOLD > 2000) THRESHOLD = 2000;

      learned = true;
      state = ST_ARMED;
      streak = 0;
      quietStreak = 0;

      Serial.println();
      Serial.print(">>> LEARNED: baseline = "); Serial.print(med);
      Serial.print(", rest noise = "); Serial.print(mad);
      Serial.print(", threshold = "); Serial.println(THRESHOLD);
      Serial.println(">>> ARMED — squeeze and hold 2-3 s, then relax fully.");
      Serial.println();
    }
    return;
  }

  // ---------------- RUNNING ----------------
  int dev = avg - (int)baseline;
  int absDev = dev < 0 ? -dev : dev;

  if (absDev > sessionMaxDev) sessionMaxDev = absDev;

  // slow drift tracking only while relaxed
  if (streak == 0 && quietStreak == 0) {
    baseline = baseline * 0.998 + avg * 0.002;
  }

  bool above = absDev > THRESHOLD;
  bool big = absDev > THRESHOLD * BIG_SPIKE_MULT;   // one big spike = definite clench

  switch (state) {
    case ST_ARMED:
      if (big) {
        clenchFired();
        state = ST_SETTLE;
        quietStreak = 0;
        streak = 0;
      } else if (above) {
        streak++;
        if (streak >= FIRE_STREAK) {
          clenchFired();
          state = ST_SETTLE;
          quietStreak = 0;
          streak = 0;
        } else {
          state = ST_RISING;
        }
      }
      break;

    case ST_RISING:
      if (big) {
        clenchFired();
        state = ST_SETTLE;
        quietStreak = 0;
        streak = 0;
      } else if (above) {
        streak++;
        if (streak >= FIRE_STREAK) {
          clenchFired();
          state = ST_SETTLE;
          quietStreak = 0;
          streak = 0;
        }
      } else {
        // too short / dropped -> back to armed, not a real squeeze
        state = ST_ARMED;
        streak = 0;
      }
      break;

    case ST_SETTLE:
      if (!above) {
        quietStreak++;
        if (quietStreak >= SETTLE_LINES) {
          state = ST_ARMED;
          quietStreak = 0;
          Serial.println(">>> ARMED — squeeze now.");
        }
      } else {
        quietStreak = 0;   // still moving -> restart the calm count
      }
      break;
  }

  // one diagnostic line
  const char *st = state == ST_ARMED ? "ARMED" : (state == ST_RISING ? "RISING" : "SETTLE");
  Serial.print("avg="); Serial.print(avg);
  Serial.print("  dev="); Serial.print(dev);
  Serial.print("  thr="); Serial.print(THRESHOLD);
  Serial.print("  state="); Serial.print(st);
  Serial.print("  MAXdev="); Serial.print(sessionMaxDev);
  Serial.println(above ? "  [above]" : "");
}
