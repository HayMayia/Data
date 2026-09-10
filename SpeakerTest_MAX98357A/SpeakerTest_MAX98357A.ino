/*
 * VocalBridge — SPEAKER TEST (MAX98357A I2S amp + speaker)  ← YOUR hardware
 * -------------------------------------------------------------------------
 * You bought: MAX98357A I2S amplifier + 2-inch 4-ohm speaker.
 * This sketch makes the ESP32 send beeps over I2S. If you hear
 * beep-beep-beeeep every 3 seconds, the speaker + driver WORK.
 *
 * No library needed. Works on esp32 board package v2.x AND v3.x
 * (it auto-detects the version).
 *
 * Board:  ESP32 Dev Module
 * Baud:   115200 (Serial Monitor — optional, for volume commands)
 *
 * WIRING (5 wires + speaker):
 *
 *   MAX98357A        ESP32
 *   --------         -----
 *   VIN      ----->  VIN  (5V)
 *   GND      ----->  GND
 *   BCLK     ----->  GPIO27
 *   LRC      ----->  GPIO26
 *   DIN      ----->  GPIO25
 *   SD       ----->  (leave unconnected — enabled by default;
 *                    if you ever hear NOTHING, try SD -> 3V3)
 *   GAIN     ----->  (leave unconnected — default gain is fine)
 *
 *   Speaker + and -  ----->  the driver's + / - output holes
 *                            (solder the two speaker wires there)
 *
 * These pins do NOT clash with the muscle sensor (AD8232 on GPIO34,
 * LO+ on 32, LO- on 33) — both can be wired at the same time.
 *
 * SERIAL COMMANDS (115200):
 *   b    play the beep pattern now
 *   +    volume up            -    volume down
 *   h    help
 */

const int PIN_BCLK = 27;          // bit clock
const int PIN_LRC  = 26;          // left/right clock (word select)
const int PIN_DIN  = 25;          // data into the amp

const int BAUD       = 115200;
const int SAMPLE_RATE = 16000;

int volume = 10;                  // 0..21 — starts at half, safe for a small speaker

// ----------------------------------------------------------------------
// I2S setup — handled differently on core v2 vs v3, auto-detected here
// ----------------------------------------------------------------------
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // ---------- esp32 core 3.x ----------
  #include <ESP_I2S.h>
  I2SClass i2s;

  void i2sInit() {
    i2s.setPins(PIN_BCLK, PIN_LRC, PIN_DIN);
    i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
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
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,     // mono; the MAX98357A averages L+R
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
      .tx_desc_auto_clear = true
    };
    i2s_pin_config_t pins = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = PIN_BCLK,
      .ws_io_num = PIN_LRC,
      .data_out_num = PIN_DIN,
      .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
  }

  void i2sWriteSamples(int16_t *buf, int count) {
    size_t written = 0;
    i2s_write(I2S_NUM_0, buf, count * sizeof(int16_t), &written, portMAX_DELAY);
  }
#endif

// ----------------------------------------------------------------------
// Tiny synthesizer: one 16-entry sine table + a phase accumulator
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// Clean 256-step sine wave (the old 16-step version sounded raspy even
// with perfect wiring — this one sounds like a real instrument)
// ----------------------------------------------------------------------
int16_t SINE256[256];

void buildSine() {
  for (int i = 0; i < 256; i++)
    SINE256[i] = (int16_t)(32767.0 * sinf(6.28318530718 * i / 256.0));
}

// play a pure tone for `ms` milliseconds at `freqHz` (with click-free fades)
void playTone(int freqHz, int ms) {
  uint32_t step  = (uint32_t)(((double)freqHz * 4294967296.0) / SAMPLE_RATE);
  uint32_t phase = 0;
  long total     = (long)SAMPLE_RATE * ms / 1000;     // total samples to send
  long fade      = SAMPLE_RATE / 50;                  // 20 ms fade in/out (no clicks)
  if (fade * 2 > total) fade = total / 2;

  int16_t buf[64];
  long sent = 0;
  while (sent < total) {
    int n = 64;
    if (sent + n > total) n = (int)(total - sent);
    for (int i = 0; i < n; i++) {
      long idx = sent + i;
      int env = volume;                               // flat middle part
      if (idx < fade)          env = (int)(volume * idx / fade);          // fade in
      else if (idx >= total - fade) env = (int)(volume * (total - idx) / fade); // fade out
      int32_t s = ((int32_t)SINE256[phase >> 24] * env) / 21;
      buf[i] = (int16_t)s;
      phase += step;
    }
    i2sWriteSamples(buf, n);
    sent += n;
  }
}

void playChime() {          // the pattern you should learn to recognize
  playTone(880, 150);  delay(120);
  playTone(880, 150);  delay(120);
  playTone(440, 450);       // looong low beep
}

void volumeMsg() {
  Serial.print(">> volume = ");
  Serial.print(volume);
  Serial.println(" / 21");
}

void handleChar(char c) {
  if (c == '\r' || c == '\n' || c == ' ') return;
  if (c == 'b' || c == 'B') { Serial.println(">> beep!"); playChime(); }
  else if (c == '+') { if (volume < 21) volume++; volumeMsg(); }
  else if (c == '-') { if (volume > 0)  volume--; volumeMsg(); }
  else if (c == 'h' || c == '?') printHelp();
  else { Serial.print("?? '"); Serial.write(c); Serial.println("' — send h for help"); }
}

void printHelp() {
  Serial.println();
  Serial.println("--------------- SPEAKER TEST — commands ---------------");
  Serial.println("  b   beep now        + / -   volume up / down");
  Serial.println("  h   this help");
  Serial.println("--------------------------------------------------------");
  Serial.println();
}

// ----------------------------------------------------------------------
void setup() {
  Serial.begin(BAUD);
  delay(800);

  Serial.println();
  Serial.println("VocalBridge SpeakerTest — MAX98357A (I2S) + speaker");
  Serial.println("----------------------------------------------------");

  i2sInit();
  buildSine();

  Serial.println(">> I2S started. Playing a chime in 2 seconds — LISTEN...");
  Serial.println(">> You should hear: beep-beep-beeeep, repeating every 3 s.");
  volumeMsg();
  delay(2000);
}

void loop() {
  playChime();
  Serial.println("chime! (b = beep now, + / - = volume)");
  for (int t = 0; t < 300; t++) {          // 3 s pause, still reading commands
    if (Serial.available()) handleChar(Serial.read());
    delay(10);
  }
}
