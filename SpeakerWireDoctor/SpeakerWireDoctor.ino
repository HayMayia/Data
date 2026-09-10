/*
 * VocalBridge — SPEAKER WIRE DOCTOR (MAX98357A)
 * ---------------------------------------------
 * Finds your wiring mistake WITHOUT rewiring. You keep the three signal
 * wires plugged in wherever they are, upload this, and press 'm'.
 *
 * It tries 6 different pin mappings in software. When you hear a CLEAN
 * beep-beep-beeeep, the screen tells you exactly which mapping matched —
 * that's how your wires are actually arranged. Report it back (or rewire
 * to the standard M1) and you're done.
 *
 * WHY THIS WORKS: maybe your wires are swapped or one hole off. Instead of
 * guessing with jumpers, the sketch just changes WHICH GPIOs it outputs
 * BCLK / LRC / DIN on, until the sound is clean.
 *
 * Board:  ESP32 Dev Module        Baud: 115200
 * Library: NONE (esp32 core v2.x and v3.x both OK)
 *
 * Keep these plugged in (any arrangement — that's the point):
 *   VIN -> VIN(5V), GND -> GND, and three wires into BCLK / LRC / DIN
 *
 * COMMANDS:
 *   m    next mapping (listen after every press!)
 *   1-6  jump to a mapping
 *   b    chime right now
 *   + -  volume
 *   p    print current mapping
 *   h    help
 *
 * Note: mapping M5 briefly uses GPIO33 (which the muscle sensor's LO-
 * also uses). That's fine during this test — the sensor isn't read here.
 */

const int BAUD        = 115200;
const int SAMPLE_RATE = 16000;

struct MapDef {
  const char *name;
  int bclk, lrc, din;
};

// BCLK / LRC / DIN candidate arrangements
const MapDef MAPS[] = {
  { "M1 standard (BCLK=27 LRC=26 DIN=25)",   27, 26, 25 },
  { "M2 BCLK<->LRC swapped",                 26, 27, 25 },
  { "M3 LRC<->DIN swapped",                  27, 25, 26 },
  { "M4 BCLK<->DIN swapped",                 25, 26, 27 },
  { "M5 all shifted one hole (down)",        26, 25, 33 },
  { "M6 all shifted one hole (up)",          14, 27, 26 },
};
const int NUM_MAPS = 6;

int curMap    = 0;
int volume    = 18;          // 0..21
bool i2sReady = false;

// ----------------------------------------------------------------------
// I2S — auto-detects esp32 core v2 vs v3, re-inits on mapping change
// ----------------------------------------------------------------------
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // ---------- esp32 core 3.x ----------
  #include <ESP_I2S.h>
  I2SClass i2s;

  void i2sDeinit() { if (i2sReady) { i2s.end(); i2sReady = false; } }
  void i2sInit(int bclk, int lrc, int din) {
    i2sDeinit();
    i2s.setPins(bclk, lrc, din);
    i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    i2sReady = true;
  }
  void i2sWriteSamples(int16_t *buf, int count) {
    i2s.write((uint8_t *)buf, count * sizeof(int16_t));
  }
#else
  // ---------- esp32 core 2.x ----------
  #include <driver/i2s.h>

  void i2sDeinit() { if (i2sReady) { i2s_driver_uninstall(I2S_NUM_0); i2sReady = false; } }
  void i2sInit(int bclk, int lrc, int din) {
    i2sDeinit();
    i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = true,     // precision audio clock = less rasp (auto-fallback below)
      .tx_desc_auto_clear = true
    };
    i2s_pin_config_t pins = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = bclk,
      .ws_io_num = lrc,
      .data_out_num = din,
      .data_in_num = I2S_PIN_NO_CHANGE
    };
    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
      cfg.use_apll = false;                    // this core can't do APLL -> plain clock
      i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    }
    i2s_set_pin(I2S_NUM_0, &pins);
    i2sReady = true;
  }
  void i2sWriteSamples(int16_t *buf, int count) {
    size_t written = 0;
    i2s_write(I2S_NUM_0, buf, count * sizeof(int16_t), &written, portMAX_DELAY);
  }
#endif

// ----------------------------------------------------------------------
// Chime (same as the beep test sketch)
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// Chime (same as the beep test sketch) — clean 256-step sine
// ----------------------------------------------------------------------
int16_t SINE256[256];

void buildSine() {
  for (int i = 0; i < 256; i++)
    SINE256[i] = (int16_t)(32767.0 * sinf(6.28318530718 * i / 256.0));
}

void playTone(int freqHz, int ms) {
  uint32_t step  = (uint32_t)(((double)freqHz * 4294967296.0) / SAMPLE_RATE);
  uint32_t phase = 0;
  long total = (long)SAMPLE_RATE * ms / 1000;
  long fade  = SAMPLE_RATE / 50;
  if (fade * 2 > total) fade = total / 2;
  int16_t buf[64];
  long sent = 0;
  while (sent < total) {
    int n = 64;
    if (sent + n > total) n = (int)(total - sent);
    for (int i = 0; i < n; i++) {
      long idx = sent + i;
      int env = volume;
      if (idx < fade)               env = volume * idx / fade;
      else if (idx >= total - fade) env = volume * (total - idx) / fade;
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
// Mapping announcements + commands
// ----------------------------------------------------------------------
void announceMap() {
  Serial.println();
  Serial.println("======================================================");
  Serial.print(">>> TRYING: "); Serial.println(MAPS[curMap].name);
  Serial.println(">>> Listen!  CLEAN beep-beep-beeeep = found it!");
  Serial.println(">>> Garble or silence = press 'm' again.");
  Serial.println("======================================================");
}

void applyMap() {
  Serial.println();
  Serial.print("Re-wiring in software -> ");
  Serial.println(MAPS[curMap].name);
  i2sInit(MAPS[curMap].bclk, MAPS[curMap].lrc, MAPS[curMap].din);
  delay(100);
  announceMap();
  playChime();   // immediate test
  playChime();
}

void printHelp() {
  Serial.println();
  Serial.println("--------------- WIRE DOCTOR — commands ---------------");
  Serial.println("  m    next mapping (press and LISTEN)");
  Serial.println("  1-6  jump straight to a mapping");
  Serial.println("  b    chime now        + / -  volume");
  Serial.println("  p    show current mapping");
  Serial.println("-------------------------------------------------------");
  Serial.println();
}

void handleChar(char c) {
  if (c == '\r' || c == '\n' || c == ' ') return;
  if (c == 'm' || c == 'M') { curMap = (curMap + 1) % NUM_MAPS; applyMap(); }
  else if (c >= '1' && c <= '6') { curMap = c - '1'; applyMap(); }
  else if (c == 'b' || c == 'B') { Serial.println(">> chime"); playChime(); }
  else if (c == '+') { if (volume < 21) volume++; Serial.print(">> volume "); Serial.println(volume); }
  else if (c == '-') { if (volume > 0)  volume--; Serial.print(">> volume "); Serial.println(volume); }
  else if (c == 'p' || c == 'P') announceMap();
  else if (c == 'h' || c == '?') printHelp();
  else { Serial.print("?? '"); Serial.write(c); Serial.println("' — send h for help"); }
}

// ----------------------------------------------------------------------
void setup() {
  Serial.begin(BAUD);
  delay(800);
  buildSine();
  Serial.println();
  Serial.println("VocalBridge — SPEAKER WIRE DOCTOR");
  Serial.println("==================================");
  Serial.println("Keep all 5 wires plugged in exactly as they are.");
  Serial.println("I will try 6 pin arrangements. Press 'm' and listen.");
  Serial.println("When the chime is CLEAN, note the mapping shown.");
  printHelp();
  applyMap();
}

void loop() {
  playChime();
  Serial.println("(chime — mapping shown above; press m to try the next one)");
  for (int t = 0; t < 300; t++) {
    if (Serial.available()) handleChar(Serial.read());
    delay(10);
  }
}
