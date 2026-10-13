/*
 * VocalBridge — HELP SPEAKER  V1.0 (13 Oct 2026)
 * --------------------------------------------------------
 * SAME CONNECTIONS AS BEFORE — just add speaker:
 *   AD8232: OUT->GPIO34  LO+->GPIO32  LO- ->GPIO33  3V3/GND
 *   MAX98357A: VIN->5V  GND->GND  BCLK->GPIO27  LRC->GPIO26  DIN->GPIO25
 *   Speaker -> +/-  (SOLDER joints)
 *   Placement: Yellow left under chin, Red right neck, grey down shirt, RL shoulder
 *
 * What it does: when you say HELP (small mouth) it SPEAKS HELP in your voice.
 * Detection = V1.21 CALIB-FIX (+90/25) — burstLvl = silence+60, HELP = peak>burstLvl+90 dur25-1500
 * Voice = your own HELP (help_own_v3_word.wav) 24000Hz via MAX98357A
 *
 * Keys: c=calibrate(silent hands OFF!)  h=mark  t=test-speak  i=info  +/- sens  r=speed  v=volume
 *       b=beep   Press c correctly: burstLvl should be 300-500, not 2000
 */
#include <Preferences.h>
#include "help_voice.h"

const int PIN_OUT=34,PIN_LOP=32,PIN_LON=33;
const int PIN_BCLK=27,PIN_LRC=26,PIN_DIN=25;

const int SAMPLE_MS=2;
int printMs=50;
long center=2048; int env=0,lastRaw=0; bool firstSample=true;
const int RING=100; int ring[RING]; int ringIdx=0;
int burstLvl=400;
const unsigned long GRACE=100, REFRACT=1200, BURSTMAX=3000;
bool inBurst=false; unsigned long burstStart=0,lastHigh=0,lastFire=0;
int burstPeak=0, helpCnt=0;
unsigned long tS=0,tP=0;
Preferences prefs;
int volume=15; // 0..21

// ---- I2S ----
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  #include <ESP_I2S.h>
  I2SClass i2s;
  void i2sInit(){ i2s.setPins(PIN_BCLK,PIN_LRC,PIN_DIN); i2s.begin(I2S_MODE_STD,24000,I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO); }
  void i2sWriteSamples(int16_t *buf,int count){ i2s.write((uint8_t*)buf,count*sizeof(int16_t)); }
#else
  #include <driver/i2s.h>
  void i2sInit(){
    i2s_config_t cfg={ .mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX), .sample_rate=24000, .bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT, .channel_format=I2S_CHANNEL_FMT_ONLY_LEFT, .communication_format=I2S_COMM_FORMAT_STAND_I2S, .intr_alloc_flags=0, .dma_buf_count=8, .dma_buf_len=64, .use_apll=true, .tx_desc_auto_clear=true };
    i2s_pin_config_t pins={ .mck_io_num=I2S_PIN_NO_CHANGE, .bck_io_num=PIN_BCLK, .ws_io_num=PIN_LRC, .data_out_num=PIN_DIN, .data_in_num=I2S_PIN_NO_CHANGE };
    if(i2s_driver_install(I2S_NUM_0,&cfg,0,NULL)!=ESP_OK){ cfg.use_apll=false; i2s_driver_install(I2S_NUM_0,&cfg,0,NULL); }
    i2s_set_pin(I2S_NUM_0,&pins);
  }
  void i2sWriteSamples(int16_t *buf,int count){ size_t w=0; i2s_write(I2S_NUM_0,buf,count*sizeof(int16_t),&w,portMAX_DELAY); }
#endif

void saveLvl(){ prefs.begin("helpdetect",false); prefs.putInt("burstLvl",burstLvl); prefs.end();}
void loadLvl(){ prefs.begin("helpdetect",true); burstLvl=prefs.getInt("burstLvl",400); prefs.end(); if(burstLvl>600) burstLvl=400; }

void banner(){
  Serial.println(); Serial.println(F("==== VocalBridge HELP SPEAKER V1.0 (AD8232+MAX98357A) ===="));
  Serial.println(F("AD8232: OUT=34 LO+=32 LO-=33   MAX98357A: BCLK=27 LRC=26 DIN=25  5V/GND"));
  Serial.print(F("burstLvl=")); Serial.println(burstLvl);
  Serial.println(F("HELP = peak>burstLvl+90 dur25-1500 -> SPEAKS HELP (your voice)"));
  Serial.println(F("PLACEMENT: Yellow left chin, Red right neck, grey down shirt, RL shoulder"));
  Serial.println(F("keys: c=calibrate(silent hands OFF!) h=mark t=test i=info r=speed v=volume +/- sens b=beep"));
  Serial.println(F("streaming..."));
}
void mark(const __FlashStringHelper *w){ Serial.print(F(">>>MARK:")); Serial.print(w); Serial.print(F(" t=")); Serial.print(millis()/1000.0,2); Serial.println(F("s<<<"));}

// ---- speaker ----
int16_t SINE256[256];
void buildSine(){ for(int i=0;i<256;i++) SINE256[i]=(int16_t)(32767.0*sinf(6.28318530718*i/256.0)); }
void playTone(int hz,int ms){
  uint32_t step=(uint32_t)(((double)hz*4294967296.0)/24000); uint32_t phase=0; long total=24000L*ms/1000; long fade=24000/50; if(fade*2>total) fade=total/2; int16_t buf[64]; long sent=0;
  while(sent<total){ int nn=64; if(sent+nn>total) nn=total-sent; for(int i=0;i<nn;i++){ long idx=sent+i; int envV=volume; if(idx<fade) envV=volume*idx/fade; else if(idx>=total-fade) envV=volume*(total-idx)/fade; int32_t s=((int32_t)SINE256[phase>>24]*envV)/21; buf[i]=(int16_t)s; phase+=step; } i2sWriteSamples(buf,nn); sent+=nn; }
}
void playAlertBeeps(){ playTone(1760,110); delay(90); playTone(1760,110); delay(220); }
void playClip(const int16_t *pcm,uint32_t n,uint32_t rate){
  const int FADE=72; int16_t buf[256]; uint32_t pos=0;
  while(pos<n){ uint32_t chunk=(n-pos<256)?(n-pos):256; for(uint32_t i=0;i<chunk;i++){ uint32_t idx=pos+i; int envV=volume; if(idx<FADE) envV=volume*idx/FADE; if(idx>=n-FADE) envV=volume*(n-idx)/FADE; int32_t s=((int32_t)pcm[idx]*envV)/21; buf[i]=(int16_t)s; } i2sWriteSamples(buf,chunk); pos+=chunk; }
}
void sayHELP(){ playAlertBeeps(); playClip(HELP_PCM,HELP_NUM_SAMPLES,HELP_SAMPLE_RATE); }

void fire(int pk,unsigned long d){
  helpCnt++; Serial.println(F("############################################################"));
  Serial.print(F(">>>  HELP! #")); Serial.print(helpCnt); Serial.print(F(" peak=")); Serial.print(pk); Serial.print(F(" dur=")); Serial.print(d); Serial.println(F("ms -> SPEAKING"));
  Serial.println(F("############################################################"));
  sayHELP();
}
void doCal(){
  Serial.println(F(">>> CALIBRATE: SILENT 5s hands OFF pads..."));
  const int N=100; int buf[N];
  for(int i=0;i<N;i++){ unsigned long st=millis(); while(millis()-st<50){ int raw=analogRead(PIN_OUT); lastRaw=raw; if(firstSample){center=raw; firstSample=false;} center+=(raw-center)/1024; int dev=abs(raw-center); if(dev>env) env=dev; else env=(env*97)/100; ring[ringIdx]=env; ringIdx=(ringIdx+1)%RING; delay(2);} buf[i]=env; }
  for(int i=0;i<N-1;i++) for(int j=i+1;j<N;j++) if(buf[j]<buf[i]){int t=buf[i];buf[i]=buf[j];buf[j]=t;}
  int p90=(buf[N-11]+buf[N-10])/2; burstLvl=p90+60;
  if(burstLvl>600){ Serial.print(F(">>> WARNING burstLvl=")); Serial.print(burstLvl); Serial.println(F(" too HIGH! Press c again SILENT hands OFF")); burstLvl=400; Serial.println(F(">>> Reset 400, press c again properly")); }
  Serial.print(F(">>> burstLvl=")); Serial.println(burstLvl); Serial.println(F(">>> CALIBRATE OK — say HELP small mouth")); saveLvl(); inBurst=false; burstPeak=0;
}
void setup(){
  Serial.begin(115200); pinMode(PIN_LOP,INPUT); pinMode(PIN_LON,INPUT);
  analogReadResolution(12); analogSetPinAttenuation(PIN_OUT,ADC_11db); delay(800);
  loadLvl(); i2sInit(); buildSine(); banner();
}
void sample(){
  int raw=analogRead(PIN_OUT); lastRaw=raw;
  if(firstSample){center=raw; firstSample=false;}
  center+=(raw-center)/1024; int dev=abs(raw-center);
  if(dev>env) env=dev; else env=(env*97)/100; ring[ringIdx]=env; ringIdx=(ringIdx+1)%RING;
}
void printLine(){
  int peak=0; for(int i=0;i<RING;i++) if(ring[i]>peak) peak=ring[i];
  bool pads=(digitalRead(PIN_LOP)==LOW && digitalRead(PIN_LON)==LOW);
  Serial.print(F("raw:")); Serial.print(lastRaw); Serial.print(F(",env:")); Serial.print(env);
  Serial.print(F(",peak:")); Serial.print(peak); Serial.print(F(",pads:")); Serial.print(pads?1:0);
  Serial.print(F(",thresh:")); Serial.print(burstLvl+90); Serial.print(F("  [t=")); Serial.print(millis()/1000.0,2); Serial.print(F("s]"));
  if(!pads) Serial.print(F("  CHECK-PADS!")); Serial.println();
}
void handle(char c){
  if(c=='\r'||c=='\n'||c=='\t'||c==' ') return;
  if(c=='h'||c=='H') mark(F("HELP"));
  else if(c=='c'||c=='C') doCal();
  else if(c=='t'||c=='T'){ Serial.println(F(">>> HELP! (TEST) -> SPEAKING")); sayHELP(); }
  else if(c=='i'||c=='I') banner();
  else if(c=='+'||c=='='){ burstLvl+=10; saveLvl(); Serial.print(F(">>> thresh now ")); Serial.print(burstLvl+90); Serial.println(F(" (harder)")); }
  else if(c=='-'||c=='_'){ burstLvl-=10; if(burstLvl<50) burstLvl=50; saveLvl(); Serial.print(F(">>> thresh now ")); Serial.print(burstLvl+90); Serial.println(F(" (easier)")); }
  else if(c=='r'||c=='R'){ printMs=(printMs==50)?250:50; Serial.println(printMs==250?F("slow"):F("fast")); }
  else if(c=='v'||c=='V'){ if(volume<21) volume++; else volume=10; Serial.print(F(">>> volume ")); Serial.println(volume); }
  else if(c=='b'||c=='B'){ Serial.println(F(">>> beep")); playTone(880,150); delay(120); playTone(440,450); }
}
void classify(unsigned long dur){
  int thr=burstLvl+90;
  if(burstPeak>=thr && dur>=25 && dur<=1500){
    if(millis()-lastFire>=REFRACT){ fire(burstPeak,dur); lastFire=burstStart; }
    else { Serial.print(F(">>> BURST peak=")); Serial.print(burstPeak); Serial.print(F(" dur=")); Serial.print(dur); Serial.println(F("ms -> echo skipped")); }
  } else {
    Serial.print(F(">>> BURST peak=")); Serial.print(burstPeak); Serial.print(F(" dur=")); Serial.print(dur); Serial.print(F("ms -> NOT HELP ("));
    if(burstPeak<thr) Serial.print(F("too weak")); else if(dur<25) Serial.print(F("too short")); else Serial.print(F("too long")); Serial.println(F(")"));
  }
}
void loop(){
  unsigned long now=millis();
  if(now-tS>=SAMPLE_MS){ tS=now; sample(); }
  if(env>=burstLvl){
    if(!inBurst){ inBurst=true; burstStart=now; burstPeak=env; }
    if(env>burstPeak) burstPeak=env; lastHigh=now;
    if(now-burstStart>BURSTMAX){ classify(now-burstStart); Serial.println(F(">>> burst >3s -> pad moved, press c")); inBurst=false; burstPeak=0; }
  } else if(inBurst && now-lastHigh>=GRACE){ classify(lastHigh-burstStart); inBurst=false; burstPeak=0; }
  if(now-tP>= (unsigned long)printMs){ tP=now; printLine(); }
  while(Serial.available()) handle(Serial.read());
}
