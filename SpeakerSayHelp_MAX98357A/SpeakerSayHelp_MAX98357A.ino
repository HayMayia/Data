/*
 * VocalBridge — SPEAKER SAYS "HELP!" (MAX98357A + your own voice chip)
 * --------------------------------------------------------------------
 * The "Help!" voice is INSIDE the ESP32 (help_sound.h) — no SD card,
 * no DFPlayer, nothing extra. Just this sketch + your MAX98357A board.
 *
 * What it does:
 *   - 2 s after boot, the speaker SAYS "Help!"
 *   - Serial commands (115200):
 *       h    say "Help!" now
 *       2    say it twice (emergency double-call)
 *       l    toggle loop mode — repeats every 3 s (great for filming)
 *       + -  volume up / down (0..21)
 *       b    beep chime (fallback sound, for comparing)
 *       i    info: clip length, volume, loop state
 *
 * Board:  ESP32 Dev Module        Baud: 115200
 * Library: NONE (works on esp32 core v2.x and v3.x, auto-detected)
 *
 * WIRING (same as the beep test — nothing changes):
 *
 *   MAX98357A        ESP32
 *   VIN      ----->  VIN  (5V)
 *   GND      ----->  GND
 *   BCLK     ----->  GPIO27
 *   LRC      ----->  GPIO26
 *   DIN      ----->  GPIO25
 *   SD, GAIN ----->  unconnected
 *   + / -    ----->  speaker wires (soldered)
 *
 * WANT YOUR OWN VOICE INSTEAD?
 *   Record "Help!" → save as audio/help.wav (mono) → run make_help_header.py
 *   → this sketch automatically uses the new clip. Or upload your recording
 *   to the chat and I'll convert it for you.
 */

#include "help_sound.h"     // the embedded "Help!" audio (24000 Hz mono PCM)

// ---------------- WIRING CONFIG ----------------
// Defaults = standard wiring. If SpeakerWireDoctor found a different
// mapping, just change these three numbers to match it:
//   M1 standard:            BCLK=27  LRC=26  DIN=25
//   M2 BCLK<->LRC swapped:  BCLK=26  LRC=27  DIN=25
//   M3 LRC<->DIN swapped:   BCLK=27  LRC=25  DIN=26
//   M4 BCLK<->DIN swapped:  BCLK=25  LRC=26  DIN=27
//   M5 one hole down:       BCLK=26  LRC=25  DIN=33
//   M6 one hole up:         BCLK=14  LRC=27  DIN=26
// (Or simply re-wire the jumpers to match M1 and leave these alone.)
const int PIN_BCLK = 27;
const int PIN_LRC  = 26;
const int PIN_DIN  = 25;
const int BAUD     = 115200;

int  volume   = 13;         // 0..21 (13 = clean on small speakers; raise with +)
bool loopMode = false;

// ----------------------------------------------------------------------
// I2S setup — auto-detects esp32 core v2 vs v3
// ----------------------------------------------------------------------
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // ---------- esp32 core 3.x ----------
  #include <ESP_I2S.h>
  I2SClass i2s;

  void i2sInit() {
    i2s.setPins(PIN_BCLK, PIN_LRC, PIN_DIN);
    i2s.begin(I2S_MODE_STD, HELP_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  }
  void i2sWriteSamples(int16_t *buf, int count) {
    i2s.write((uint8_t *)buf, count * sizeof(int16_t));
  }
#else
  // ---------- esp32 core 2.x ----------
  #include <driver/i2s.h>

  void i2sInit() {
    i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = HELP_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,   // mono; MAX98357A averages L+R
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = true,     // precision audio clock = less rasp (auto-fallback below)
      .tx_desc_auto_clear = true
    };
    i2s_pin_config_t pins = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = PIN_BCLK,
      .ws_io_num = PIN_LRC,
      .data_out_num = PIN_DIN,
      .data_in_num = I2S_PIN_NO_CHANGE
    };
    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
      cfg.use_apll = false;                    // this core can't do APLL -> plain clock
      i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    }
    i2s_set_pin(I2S_NUM_0, &pins);
  }
  void i2sWriteSamples(int16_t *buf, int count) {
    size_t written = 0;
    i2s_write(I2S_NUM_0, buf, count * sizeof(int16_t), &written, portMAX_DELAY);
  }
#endif

// ----------------------------------------------------------------------
// Voice playback: stream HELP_PCM with volume + click-free fades
// ----------------------------------------------------------------------
void playHelp() {
  const int FADE = 72;                       // ~3 ms at 24 kHz
  int16_t buf[256];
  uint32_t pos = 0;
  while (pos < HELP_NUM_SAMPLES) {
    int n = (HELP_NUM_SAMPLES - pos < 256) ? (HELP_NUM_SAMPLES - pos) : 256;
    for (int i = 0; i < n; i++) {
      uint32_t idx = pos + i;
      int env = volume;
      if (idx < FADE)                   env = volume * idx / FADE;
      if (idx >= HELP_NUM_SAMPLES - FADE) env = volume * (HELP_NUM_SAMPLES - idx) / FADE;
      int32_t s = ((int32_t)HELP_PCM[idx] * env) / 21;
      buf[i] = (int16_t)s;
    }
    i2sWriteSamples(buf, n);
    pos += n;
  }
}

void playHelpTwice() {
  playHelp();
  delay(400);
  playHelp();
}

// ----------------------------------------------------------------------
// Beep fallback (same chime as SpeakerTest_MAX98357A)
// Clean 256-step sine — the old 16-step version sounded raspy even on
// perfect wiring. The "Help!" voice above is a real recording and was
// never affected by this.
// ----------------------------------------------------------------------
int16_t SINE256[256];

void buildSine() {
  for (int i = 0; i < 256; i++)
    SINE256[i] = (int16_t)(32767.0 * sinf(6.28318530718 * i / 256.0));
}

void playTone(int freqHz, int ms) {
  uint32_t step  = (uint32_t)(((double)freqHz * 4294967296.0) / HELP_SAMPLE_RATE);
  uint32_t phase = 0;
  long total = (long)HELP_SAMPLE_RATE * ms / 1000;
  long fade  = HELP_SAMPLE_RATE / 50;
  if (fade * 2 > total) fade = total / 2;
  int16_t buf[64];
  long sent = 0;
  while (sent < total) {
    int n = 64;
    if (sent + n > total) n = (int)(total - sent);
    for (int i = 0; i < n; i++) {
      long idx = sent + i;
      int env = volume;
      if (idx < fade)                env = volume * idx / fade;
      else if (idx >= total - fade)  env = volume * (total - idx) / fade;
      int32_t s = ((int32_t)SINE256[phase >> 24] * env) / 21;
      buf[i] = (int16_t)s;
      phase += step;
    }
    i2sWriteSamples(buf, n);
    sent += n;
  }
}

void playChime() {
  playTone(880, 150);  delay(120);
  playTone(880, 150);  delay(120);
  playTone(440, 450);
}

// ----------------------------------------------------------------------
// Serial commands
// ----------------------------------------------------------------------
void printHelp() {
  Serial.println();
  Serial.println("----------------- SPEAKER SAYS HELP — commands -----------------");
  Serial.println("  h   say 'Help!'          2    say it twice");
  Serial.println("  l   toggle loop mode (every 3 s — good for filming)");
  Serial.println("  + / -   volume up / down       b    beep chime");
  Serial.println("  i   info");
  Serial.println("------------------------------------------------------------------");
  Serial.println();
}

void volumeMsg() {
  Serial.print(">> volume = "); Serial.print(volume); Serial.println(" / 21");
}

void handleChar(char c) {
  if (c == '\r' || c == '\n' || c == ' ') return;
  if (c == 'h' || c == 'H') { Serial.println(">> 'Help!'"); playHelp(); }
  else if (c == '2')        { Serial.println(">> 'Help! Help!'"); playHelpTwice(); }
  else if (c == 'l' || c == 'L') {
    loopMode = !loopMode;
    Serial.println(loopMode ? ">> loop ON — 'Help!' every 3 s (l = off)" : ">> loop OFF");
  }
  else if (c == '+') { if (volume < 21) volume++; volumeMsg(); }
  else if (c == '-') { if (volume > 0)  volume--; volumeMsg(); }
  else if (c == 'b' || c == 'B') { Serial.println(">> beep"); playChime(); }
  else if (c == 'i' || c == 'I') {
    Serial.print(">> clip: "); Serial.print(HELP_NUM_SAMPLES);
    Serial.print(" samples @ "); Serial.print(HELP_SAMPLE_RATE);
    Serial.print(" Hz = "); Serial.print((float)HELP_NUM_SAMPLES / HELP_SAMPLE_RATE, 2);
    Serial.println(" s | volume:"); Serial.print(volume);
    Serial.print("/21 | loop:"); Serial.println(loopMode ? "ON" : "OFF");
  }
  else if (c == '?' ) printHelp();
  else { Serial.print("?? '"); Serial.write(c); Serial.println("' — send ? for help"); }
}

// ----------------------------------------------------------------------
void setup() {
  Serial.begin(BAUD);
  delay(800);
  Serial.println();
  Serial.println("VocalBridge — SPEAKER SAYS 'HELP!' (MAX98357A, voice on-chip)");
  Serial.println("--------------------------------------------------------------");
  i2sInit();
  buildSine();
  printHelp();
  volumeMsg();
  Serial.println(">> Saying 'Help!' in 2 seconds — LISTEN...");
  Serial.println();
  delay(2000);
  playHelp();
}

void loop() {
  static unsigned long lastLoop = 0;
  if (loopMode && millis() - lastLoop >= 3000) {
    lastLoop = millis();
    Serial.println("'Help!' (loop mode)");
    playHelp();
  }
  if (Serial.available()) handleChar(Serial.read());
  delay(10);
}
