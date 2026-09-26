/*
 * VocalBridge Final_V1 — SPEAKER SAYS WORDS (your own voice)
 * -----------------------------------------------------------
 * Two words on-chip, in YOUR voice: HELP and WATER.
 * No SD card, no DFPlayer. More words = record + make_word_header.py.
 *
 * Board:  ESP32 Dev Module        Baud: 115200
 * Library: NONE (esp32 core v2.x and v3.x auto-detected)
 *
 * WIRING (same as always):
 *   MAX98357A  VIN->VIN(5V)  GND->GND  BCLK->GPIO27  LRC->GPIO26  DIN->GPIO25
 *   Speaker -> + / -  (SOLDER the joints - twist joints garble the bass)
 *
 * COMMANDS (Serial Monitor 115200):
 *   h    say "HELP!"          w    say "WATER!"
 *   b    beep chime           l    loop HELP every 3 s (filming)
 *   + -  volume               i    info
 *
 * HOW TO ADD MORE WORDS (YES / NO / PAIN ...):
 *   1. Record the word (quiet room, phone 15-20 cm, natural calling pace)
 *   2. Convert to WAV, then: python3 make_word_header.py recording.wav YES
 *   3. Put YES_voice.h in this folder, add #include "yes_voice.h" below,
 *      copy one playWord() block, pick a key. Done.
 */

#include "help_voice.h"     // your voice: HELP  x2
#include "water_voice.h"    // your voice: WATER x2

// ---------------- WIRING CONFIG ----------------
const int PIN_BCLK = 27;
const int PIN_LRC  = 26;
const int PIN_DIN  = 25;
const int BAUD     = 115200;

int  volume   = 15;         // 0..21
bool loopMode = false;

// ----------------------------------------------------------------------
// I2S — auto-detects esp32 core v2 vs v3
// ----------------------------------------------------------------------
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #include <ESP_I2S.h>
  I2SClass i2s;
  void i2sInit() {
    i2s.setPins(PIN_BCLK, PIN_LRC, PIN_DIN);
    i2s.begin(I2S_MODE_STD, 24000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  }
  void i2sWriteSamples(int16_t *buf, int count) {
    i2s.write((uint8_t *)buf, count * sizeof(int16_t));
  }
#else
  #include <driver/i2s.h>
  void i2sInit() {
    i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 24000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = true,     // precision audio clock
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
      cfg.use_apll = false;
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
// Generic word playback (any voice header works)
// ----------------------------------------------------------------------
void playClip(const int16_t *pcm, uint32_t n, uint32_t rate) {
  const int FADE = 72;
  int16_t buf[256];
  uint32_t pos = 0;
  while (pos < n) {
    uint32_t chunk = (n - pos < 256) ? (n - pos) : 256;
    for (uint32_t i = 0; i < chunk; i++) {
      uint32_t idx = pos + i;
      int env = volume;
      if (idx < FADE)               env = volume * idx / FADE;
      if (idx >= n - FADE)          env = volume * (n - idx) / FADE;
      int32_t s = ((int32_t)pcm[idx] * env) / 21;
      buf[i] = (int16_t)s;
    }
    i2sWriteSamples(buf, chunk);
    pos += chunk;
  }
}

// 2-beep alarm intro (attention-grabber before the word)
int16_t SINE256[256];
void buildSine() {
  for (int i = 0; i < 256; i++)
    SINE256[i] = (int16_t)(32767.0 * sinf(6.28318530718 * i / 256.0));
}
void playTone(int freqHz, int ms) {
  uint32_t step = (uint32_t)(((double)freqHz * 4294967296.0) / 24000);
  uint32_t phase = 0;
  long total = 24000L * ms / 1000;
  long fade = 24000 / 50;
  if (fade * 2 > total) fade = total / 2;
  int16_t buf[64]; long sent = 0;
  while (sent < total) {
    int nn = 64;
    if (sent + nn > total) nn = (int)(total - sent);
    for (int i = 0; i < nn; i++) {
      long idx = sent + i;
      int env = volume;
      if (idx < fade)              env = volume * idx / fade;
      else if (idx >= total - fade) env = volume * (total - idx) / fade;
      int32_t s = ((int32_t)SINE256[phase >> 24] * env) / 21;
      buf[i] = (int16_t)s;
      phase += step;
    }
    i2sWriteSamples(buf, nn);
    sent += nn;
  }
}
void playAlertBeeps() {
  playTone(1760, 110); delay(90);
  playTone(1760, 110); delay(220);
}

void sayHELP()  { playAlertBeeps(); playClip(HELP_PCM,  HELP_NUM_SAMPLES,  HELP_SAMPLE_RATE); }
void sayWATER() { playAlertBeeps(); playClip(WATER_PCM, WATER_NUM_SAMPLES, WATER_SAMPLE_RATE); }

// ----------------------------------------------------------------------
void printHelp() {
  Serial.println();
  Serial.println("------------- SPEAKER SAYS WORDS — commands -------------");
  Serial.println("  h   say 'HELP!'         w   say 'WATER!'");
  Serial.println("  b   beep                l   loop HELP every 3 s");
  Serial.println("  + / -  volume           i   info");
  Serial.println("----------------------------------------------------------");
  Serial.println();
}

void handleChar(char c) {
  if (c == '\r' || c == '\n' || c == ' ') return;
  if (c == 'h' || c == 'H') { Serial.println(">> 'HELP!'");  sayHELP(); }
  else if (c == 'w' || c == 'W') { Serial.println(">> 'WATER!'"); sayWATER(); }
  else if (c == 'b' || c == 'B') { Serial.println(">> beep"); playTone(880, 150); delay(120); playTone(440, 450); }
  else if (c == 'l' || c == 'L') {
    loopMode = !loopMode;
    Serial.println(loopMode ? ">> loop ON" : ">> loop OFF");
  }
  else if (c == '+') { if (volume < 21) volume++; Serial.print(">> volume "); Serial.println(volume); }
  else if (c == '-') { if (volume > 0)  volume--; Serial.print(">> volume "); Serial.println(volume); }
  else if (c == 'i' || c == 'I') {
    Serial.print(">> HELP: ");  Serial.print(HELP_VOICE_VERSION);
    Serial.print(" | WATER: "); Serial.println(WATER_VOICE_VERSION);
  }
  else { Serial.print("?? '"); Serial.write(c); Serial.println("'"); }
}

void setup() {
  Serial.begin(BAUD);
  delay(800);
  Serial.println();
  Serial.println("VocalBridge Final_V1 — SPEAKER SAYS WORDS (your own voice)");
  Serial.println("----------------------------------------------------------");
  Serial.print("HELP  : ");  Serial.println(HELP_VOICE_VERSION);
  Serial.print("WATER : ");  Serial.println(WATER_VOICE_VERSION);
  Serial.print("COMPILED: "); Serial.print(__DATE__); Serial.print(" "); Serial.println(__TIME__);
  i2sInit();
  buildSine();
  printHelp();
  Serial.println(">> Saying 'HELP!' in 2 seconds — LISTEN...");
  delay(2000);
  sayHELP();
}

void loop() {
  static unsigned long lastLoop = 0;
  if (loopMode && millis() - lastLoop >= 3000) {
    lastLoop = millis();
    Serial.println("'HELP!' (loop mode)");
    sayHELP();
  }
  if (Serial.available()) handleChar(Serial.read());
  delay(10);
}
