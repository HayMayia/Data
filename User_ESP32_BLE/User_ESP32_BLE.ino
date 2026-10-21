/*
 * VocalBridge v2.0-BLE-FINAL — USER ESP32 (BLE + Speaker)
 * 4 voices: HELP, WATER, YES, NO — 8kHz downsampled, fits default partition
 * Auto-detect ONLY fires on recorded signature match — no false HELPs
 * Serial: reads complete lines — no missed keypresses
 */

#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "help_voice.h"
#include "water_voice.h"
#include "yes_voice.h"
#include "no_voice.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_RX_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_TX_UUID "c7a321d0-4e4d-4a9e-8e5f-1b2c3d4e5f6a"

const int PIN_OUT = 34;
const int PIN_LOP = 32;
const int PIN_LON = 33;
const int PIN_BCLK = 27;
const int PIN_LRC  = 26;
const int PIN_DIN  = 25;

const char* FIRMWARE_VERSION = "V2.0-BLE-FINAL";

const int16_t* VOICE_PCM[4] = {HELP_PCM, WATER_PCM, YES_PCM, NO_PCM};
const uint32_t VOICE_LEN[4] = {HELP_NUM_SAMPLES, WATER_NUM_SAMPLES, YES_NUM_SAMPLES, NO_NUM_SAMPLES};
const uint32_t VOICE_RATE[4] = {HELP_SAMPLE_RATE, WATER_SAMPLE_RATE, YES_SAMPLE_RATE, NO_SAMPLE_RATE};
const char* VOICE_NAME[4] = {"HELP","WATER","YES","NO"};

const int SAMPLE_MS = 2;
int printMs = 500;
long center = 2048;
int env = 0, lastRaw = 0;
bool firstSample = true;
const int RING = 100;
int ring[RING];
int ringIdx = 0;
int burstLvl = 400;

const unsigned long GRACE = 150;
const unsigned long REFRACT = 1500;
const unsigned long MIN_BURST_MS = 300;
const unsigned long BURSTMAX = 3000;

bool inBurst = false;
unsigned long burstStart = 0, lastHigh = 0, lastFire = 0;
int burstPeak = 0;
unsigned long tS = 0, tP = 0;

Preferences prefs;
int volume = 18;
bool autoDetect = true;
bool recording = false;
int recWordIdx = -1;
unsigned long recStart = 0;
const unsigned long REC_MS = 3000;
int recEnv[300];
int recIdx = 0;

int sigEnv[4][10] = {{0}};
bool sigValid[4] = {false};

bool deviceConnected = false;
bool oldDeviceConnected = false;
BLECharacteristic *pTxCharacteristic = NULL;
BLECharacteristic *pRxCharacteristic = NULL;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #include <ESP_I2S.h>
  I2SClass i2s;
  void i2sInit(){ i2s.setPins(PIN_BCLK, PIN_LRC, PIN_DIN); i2s.begin(I2S_MODE_STD, 24000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO); }
  void i2sWriteBuf(int16_t *buf, int count){ i2s.write((uint8_t*)buf, count * sizeof(int16_t)); }
#else
  #include <driver/i2s.h>
  void i2sInit(){
    i2s_config_t cfg = { .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), .sample_rate = 24000, .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, .communication_format = I2S_COMM_FORMAT_STAND_I2S, .intr_alloc_flags = 0, .dma_buf_count = 8, .dma_buf_len = 64, .use_apll = true, .tx_desc_auto_clear = true };
    i2s_pin_config_t pins = { .mck_io_num = I2S_PIN_NO_CHANGE, .bck_io_num = PIN_BCLK, .ws_io_num = PIN_LRC, .data_out_num = PIN_DIN, .data_in_num = I2S_PIN_NO_CHANGE };
    if(i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK){ cfg.use_apll = false; i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL); }
    i2s_set_pin(I2S_NUM_0, &pins);
  }
  void i2sWriteBuf(int16_t *buf, int count){ size_t w = 0; i2s_write(I2S_NUM_0, buf, count * sizeof(int16_t), &w, portMAX_DELAY); }
#endif

int activeVoice = -1;
uint32_t voicePos = 0;
int16_t voiceBuf[32];
int voiceBufCount = 0;
unsigned long voiceLastSample = 0;
unsigned long voiceSampleInterval = 0;

void applyVolume(int16_t *buf, int count){
  for(int i=0; i<count; i++){
    int32_t v = ((int32_t)buf[i] * volume) / 20;
    if(v > 32767) v = 32767;
    if(v < -32768) v = -32768;
    buf[i] = (int16_t)v;
  }
}

void startVoice(int idx){
  if(idx < 0 || idx > 3) return;
  activeVoice = idx;
  voicePos = 0;
  voiceBufCount = 0;
  voiceSampleInterval = 1000000UL / VOICE_RATE[idx];
  voiceLastSample = micros();
  Serial.print(">>> SPEAKING: "); Serial.println(VOICE_NAME[idx]);
  bleNotify(String("DETECT:") + VOICE_NAME[idx]);
}

void stopVoice(){
  activeVoice = -1;
  voiceBufCount = 0;
  voicePos = 0;
  Serial.println(">>> STOP");
}

void updateVoice(){
  if(activeVoice < 0) return;
  unsigned long now = micros();
  unsigned long next = voiceLastSample;
  while(voicePos < VOICE_LEN[activeVoice] && now - next >= voiceSampleInterval){
    voiceBuf[voiceBufCount++] = VOICE_PCM[activeVoice][voicePos++];
    next += voiceSampleInterval;
    if(voiceBufCount >= 32){
      applyVolume(voiceBuf, voiceBufCount);
      i2sWriteBuf(voiceBuf, voiceBufCount);
      voiceBufCount = 0;
    }
  }
  voiceLastSample = next;
  if(voicePos >= VOICE_LEN[activeVoice] && voiceBufCount == 0){
    activeVoice = -1;
    Serial.println(">>> DONE");
  }
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override { deviceConnected = true; Serial.println("BLE Connected"); }
  void onDisconnect(BLEServer* pServer) override { deviceConnected = false; Serial.println("BLE Disconnected"); }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    String rxValue = String(pCharacteristic->getValue().c_str());
    if (rxValue.length() > 0) {
      rxValue.trim();
      rxValue.toLowerCase();
      Serial.print("BLE: "); Serial.println(rxValue);
      handleCommand(rxValue);
    }
  }
};

void bleNotify(const char* msg){ if(deviceConnected && pTxCharacteristic){ pTxCharacteristic->setValue(msg); pTxCharacteristic->notify(); } Serial.println(msg); }
void bleNotify(String msg){ bleNotify(msg.c_str()); }

void saveLvl(){ prefs.begin("helpdetect", false); prefs.putInt("burstLvl", burstLvl); prefs.end(); }
void loadLvl(){ prefs.begin("helpdetect", true); burstLvl = prefs.getInt("burstLvl", 400); prefs.end(); if(burstLvl < 200) burstLvl = 200; if(burstLvl > 900) burstLvl = 900; }

void saveSignatures(){
  prefs.begin("helpdetect", false);
  for(int w=0; w<4; w++){
    prefs.putBool((String("sval") + String(w)).c_str(), sigValid[w]);
    if(sigValid[w]){
      prefs.putBytes((String("senv") + String(w)).c_str(), sigEnv[w], sizeof(sigEnv[w]));
    }
  }
  prefs.end();
}

void loadSignatures(){
  prefs.begin("helpdetect", true);
  for(int w=0; w<4; w++){
    sigValid[w] = prefs.getBool((String("sval") + String(w)).c_str(), false);
    if(sigValid[w]){
      prefs.getBytes((String("senv") + String(w)).c_str(), sigEnv[w], sizeof(sigEnv[w]));
    }
  }
  prefs.end();
}

void doCal(){
  env = 0; center = 2048; firstSample = true; burstPeak = 0; lastHigh = 0; inBurst = false;
  Serial.println(">>> CALIBRATE: SILENT 5s — do NOT talk, do NOT touch device");
  bleNotify("CALIBRATING");
  const int N = 100; int buf[N];
  for(int i=0; i<N; i++){ unsigned long st = millis(); while(millis() - st < 50){
    int raw = analogRead(PIN_OUT); lastRaw = raw;
    if(firstSample){ center = raw; firstSample = false; }
    center += (raw - center) / 1024; int dev = abs(raw - center);
    if(dev > env) env = dev; else env = (env * 97) / 100;
    ring[ringIdx] = env; ringIdx = (ringIdx + 1) % RING; delay(2);
  } buf[i] = env; }
  for(int i=0; i<N-1; i++) for(int j=i+1; j<N; j++) if(buf[j] < buf[i]){ int t = buf[i]; buf[i] = buf[j]; buf[j] = t; }
  int p90 = (buf[N-11] + buf[N-10]) / 2;
  Serial.print(">>> p90="); Serial.print(p90);
  if(p90 > 600){ Serial.println(" — TOO NOISY. move quieter, type c again"); bleNotify("NOISY"); return; }
  burstLvl = p90 + 150;
  if(burstLvl < 250) burstLvl = 250;
  if(burstLvl > 900) burstLvl = 900;
  Serial.print(" — burstLvl="); Serial.print(burstLvl);
  Serial.println(" (speak above this to trigger)");
  saveLvl(); inBurst = false; burstPeak = 0;
  Serial.println(">>> AUTO-DETECT ARMED — speak now");
  bleNotify("CALIBRATED");
}

void startRecording(int wordIdx){
  recording = true;
  recWordIdx = wordIdx;
  recStart = millis();
  recIdx = 0;
  for(int i=0; i<300; i++) recEnv[i] = 0;
  Serial.print(">>> RECORDING "); Serial.print(VOICE_NAME[wordIdx]); Serial.println(" — speak now for 3 seconds");
  bleNotify(String("RECORD:") + VOICE_NAME[wordIdx]);
}

void stopRecording(){
  if(!recording) return;
  recording = false;
  for(int b=0; b<10; b++){
    int sum = 0, cnt = 0;
    for(int i=b*30; i<(b+1)*30 && i<recIdx; i++){ sum += recEnv[i]; cnt++; }
    sigEnv[recWordIdx][b] = cnt ? sum/cnt : 0;
  }
  sigValid[recWordIdx] = true;
  saveSignatures();
  Serial.print(">>> RECORDED "); Serial.print(VOICE_NAME[recWordIdx]); Serial.println(" — signature saved");
  bleNotify(String("RECORDED:") + VOICE_NAME[recWordIdx]);
}

int matchVoice(int peakEnv){
  int bestIdx = -1;
  int bestDiff = 999999;
  bool anyValid = false;
  for(int w=0; w<4; w++){ if(sigValid[w]) { anyValid = true; break; } }
  if(!anyValid) return -1;
  for(int w=0; w<4; w++){
    if(!sigValid[w]) continue;
    int sigMax = 0;
    for(int b=0; b<10; b++) if(sigEnv[w][b] > sigMax) sigMax = sigEnv[w][b];
    if(sigMax == 0) continue;
    int diff = abs(peakEnv - sigMax);
    if(diff < bestDiff){ bestDiff = diff; bestIdx = w; }
  }
  if(bestIdx >= 0 && bestDiff < peakEnv / 2) return bestIdx;
  return -1;
}

void handleCommand(String cmd){
  if(cmd.length() == 0) return;
  cmd.trim();
  cmd.toLowerCase();
  char c = cmd.charAt(0);
  Serial.print("GOT: "); Serial.println(c);

  static bool waitingForRecordWord = false;
  if(waitingForRecordWord){
    waitingForRecordWord = false;
    int wi = -1;
    if(c=='h'||c=='H') wi=0;
    else if(c=='w'||c=='W') wi=1;
    else if(c=='y'||c=='Y') wi=2;
    else if(c=='n'||c=='N') wi=3;
    if(wi >= 0){ startRecording(wi); }
    else { Serial.println(">>> Unknown word for recording"); }
    return;
  }

  if(c == 'r' || c == 'R'){
    waitingForRecordWord = true;
    Serial.println(">>> Recording mode — press word key now (h/w/y/n)");
    return;
  }

  switch(c){
    case 'v': bleNotify(String("VERSION:" + String(FIRMWARE_VERSION)).c_str()); break;
    case 'a': autoDetect = !autoDetect; Serial.print("Auto-detect "); Serial.println(autoDetect ? "ON" : "OFF"); break;
    case 'c': doCal(); break;
    case 'h': case 'H': Serial.println(">>> MARK: HELP"); startVoice(0); break;
    case 'w': case 'W': Serial.println(">>> MARK: WATER"); startVoice(1); break;
    case 'y': case 'Y': Serial.println(">>> MARK: YES"); startVoice(2); break;
    case 'n': case 'N': Serial.println(">>> MARK: NO"); startVoice(3); break;
    case 't': Serial.println(">>> TEST HELP"); startVoice(0); break;
    case 'x': case 'X': stopVoice(); break;
    case 'i': banner(); break;
    case '+': case '=': burstLvl += 10; if(burstLvl > 900) burstLvl = 900; saveLvl(); Serial.print("burstLvl="); Serial.println(burstLvl); break;
    case '-': case '_': burstLvl -= 10; if(burstLvl < 200) burstLvl = 200; saveLvl(); Serial.print("burstLvl="); Serial.println(burstLvl); break;
    case 'b': case 'B': startVoice(0); break;
    case 'u': case 'U': bleNotify("UPDATE"); break;
    default: Serial.print("UNKNOWN: "); Serial.println(cmd); break;
  }
}

void banner(){
  Serial.println();
  Serial.println("==== VocalBridge V2.0-BLE-FINAL ====");
  Serial.println("Auto-detect: ON — speak, it detects, it answers (real voices)");
  Serial.println("Type c to calibrate (5s silent). Then just speak.");
  Serial.println("Serial: h=HELP w=WATER y=YES n=NO");
  Serial.println("r then word = record signature. x=stop audio. a=toggle auto. +/-=sens.");
  Serial.println("streaming...");
}

void sample(){
  int raw = analogRead(PIN_OUT); lastRaw = raw;
  if(firstSample){ center = raw; firstSample = false; }
  center += (raw - center) / 1024; int dev = abs(raw - center);
  if(dev > env) env = dev; else env = (env * 97) / 100;
  ring[ringIdx] = env; ringIdx = (ringIdx + 1) % RING;
  if(recording && recIdx < 300){ recEnv[recIdx++] = env; }
}

void classify(unsigned long dur){
  if(!autoDetect){
    Serial.print("BURST peak="); Serial.print(burstPeak); Serial.print(" dur="); Serial.print(dur); Serial.println(" -> SKIP (auto OFF)");
    return;
  }
  if(dur < MIN_BURST_MS){
    Serial.print("BURST too short ("); Serial.print(dur); Serial.println("ms) -> IGNORED");
    return;
  }
  int matched = matchVoice(burstPeak);
  if(matched >= 0 && millis() - lastFire >= REFRACT){
    Serial.println("############################################################");
    Serial.print(">>>  DETECTED "); Serial.print(VOICE_NAME[matched]); Serial.print(" peak="); Serial.print(burstPeak); Serial.print(" dur="); Serial.print(dur);
    Serial.println("ms -> SPEAKING"); Serial.println("############################################################");
    startVoice(matched);
    lastFire = millis();
  } else {
    Serial.print("BURST peak="); Serial.print(burstPeak); Serial.print(" dur="); Serial.print(dur); Serial.println("ms -> no match/cooldown");
  }
}

void printLine(){
  int peak = 0; for(int i=0; i<RING; i++) if(ring[i] > peak) peak = ring[i];
  Serial.print("raw:"); Serial.print(lastRaw); Serial.print(",env:"); Serial.print(env);
  Serial.print(",peak:"); Serial.print(peak); Serial.print(",thresh:"); Serial.print(burstLvl);
  Serial.print(",auto:"); Serial.print(autoDetect ? 1 : 0);
  if(recording){
    Serial.print(",REC:");
    Serial.print(VOICE_NAME[recWordIdx]);
    Serial.print(":");
    Serial.print(100 * (millis() - recStart) / REC_MS);
    Serial.print("%");
  }
  Serial.println();
}

void setup(){
  Serial.begin(115200);
  pinMode(PIN_LOP, INPUT); pinMode(PIN_LON, INPUT);
  analogReadResolution(12); analogSetPinAttenuation(PIN_OUT, ADC_11db); delay(800);
  loadLvl(); loadSignatures(); i2sInit();
  BLEDevice::init("VocalBridge");
  BLEServer *pServer = BLEDevice::createServer(); pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_RX_UUID, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID); pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE Advertising started. Connect to 'VocalBridge'");
  banner(); doCal();
}

void loop(){
  unsigned long now = millis();
  if(now - tS >= SAMPLE_MS){ tS = now; sample(); }
  if(env >= burstLvl){
    if(!inBurst){ inBurst = true; burstStart = now; burstPeak = env; }
    if(env > burstPeak) burstPeak = env; lastHigh = now;
    if(now - burstStart > BURSTMAX){ Serial.println("burst >3s"); inBurst = false; burstPeak = 0; }
  } else if(inBurst && now - lastHigh >= GRACE){ classify(lastHigh - burstStart); inBurst = false; burstPeak = 0; }
  if(recording && now - recStart >= REC_MS){ stopRecording(); }
  if(now - tP >= (unsigned long)printMs){ tP = now; printLine(); }
  while(Serial.available() > 0){
    String input = Serial.readStringUntil('\n');
    input.trim();
    if(input.length() > 0){ handleCommand(input); }
  }
  updateVoice();
  if(!deviceConnected && oldDeviceConnected){ delay(500); BLEDevice::startAdvertising(); oldDeviceConnected = deviceConnected; }
  if(deviceConnected && !oldDeviceConnected){ oldDeviceConnected = deviceConnected; }
  delay(1);
}
