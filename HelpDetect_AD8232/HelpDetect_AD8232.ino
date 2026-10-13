/*
 * VocalBridge — HELP DETECTOR  V1.18 DECREASED (13 Oct 2026)
 * --------------------------------------------------------
 * SIMPLEST: No box tuning, no auto-learn, no 12 thresholds.
 * After you said "NO POSITIONS ARE WORKING" — this is the reset.
 *
 * How it works:
 *   1. Press c while SILENT (hands OFF pads) → sets burstLvl = silence + 60
 *   2. Say HELP small mouth → if peak > burstLvl+60 AND dur 15-1500ms → HELP! (was +120/35 still needed big 614; decreased)
 *      Otherwise → BURST too weak/short (printed)
 *   That's it. One calibrate, one threshold. Works on ANY chin/neck/shoulder spot
 *   because it follows YOUR silence, not a fixed 650/720/780.
 *
 * Placement: keep your current Yellow=left under chin, Red=right neck,
 *   Grey leads down shirt, RL on shoulder bone (no heartbeat). Press pads 10s.
 *
 * Keys: c=calibrate  h=mark  t=test  i=info  +/- sensitivity  r=speed ( - = easier, + = harder )
 *   (h just marks, does NOT change thresholds — no more 12 changes)
 * Board: ESP32 34=OUT 32=LO+ 33=LO-  Baud 115200
 */
#include <Preferences.h>
const int PIN_OUT=34,PIN_LOP=32,PIN_LON=33;
const int SAMPLE_MS=2;
int printMs=50;
long center=2048; int env=0,lastRaw=0; bool firstSample=true;
const int RING=100; int ring[RING]; int ringIdx=0;
int burstLvl=400;
const unsigned long GRACE=100, REFRACT=1200, BURSTMAX=3000;
bool inBurst=false; unsigned long burstStart=0,lastHigh=0,lastFire=0;
int burstPeak=0, maxEnvEver=0, helpCnt=0;
unsigned long tS=0,tP=0;
Preferences prefs;
void saveLvl(){ prefs.begin("helpdetect",false); prefs.putInt("burstLvl",burstLvl); prefs.end();}
void loadLvl(){ prefs.begin("helpdetect",true); burstLvl=prefs.getInt("burstLvl",400); prefs.end();}
void banner(){
  Serial.println();
  Serial.println(F("==== VocalBridge HELP DETECTOR V1.18 DECREASED ===="));
  Serial.println(F("baud=115200  calibrates to YOUR silence"));
  Serial.print(F("burstLvl=")); Serial.println(burstLvl);
  Serial.println(F("HELP = peak > burstLvl+60 AND dur 15-1500ms (decreased from +120/35)"));
  Serial.println(F("PLACEMENT: Yellow left under chin, Red right neck, grey down shirt, RL shoulder"));
  Serial.println(F("keys: c=calibrate(silent!) h=mark t=test i=info r=speed"));
  Serial.println(F("streaming..."));
}
void mark(const __FlashStringHelper *w){ Serial.print(F(">>>MARK:")); Serial.print(w); Serial.print(F(" t=")); Serial.print(millis()/1000.0,2); Serial.println(F("s<<<"));}
void fire(int pk,unsigned long d){
  helpCnt++;
  Serial.println(F("############################################################"));
  Serial.print(F(">>>  HELP! #")); Serial.print(helpCnt); Serial.print(F(" peak=")); Serial.print(pk);
  Serial.print(F(" dur=")); Serial.print(d); Serial.println(F("ms"));
  Serial.println(F("############################################################"));
}
void doCal(){
  Serial.println(F(">>> CALIBRATE: SILENT 5s hands OFF pads..."));
  const int N=100; int buf[N];
  for(int i=0;i<N;i++){ unsigned long st=millis(); while(millis()-st<50){ int raw=analogRead(PIN_OUT); lastRaw=raw; if(firstSample){center=raw; firstSample=false;} center+=(raw-center)/1024; int dev=abs(raw-center); if(dev>env) env=dev; else env=(env*97)/100; if(env>maxEnvEver) maxEnvEver=env; ring[ringIdx]=env; ringIdx=(ringIdx+1)%RING; delay(2);} buf[i]=env; }
  for(int i=0;i<N-1;i++) for(int j=i+1;j<N;j++) if(buf[j]<buf[i]){int t=buf[i];buf[i]=buf[j];buf[j]=t;}
  int p90=(buf[N-11]+buf[N-10])/2;
  burstLvl=p90+60;
  Serial.print(F(">>> burstLvl=")); Serial.println(burstLvl); Serial.println(F(">>> CALIBRATE OK — say HELP normally, should fire")); saveLvl(); inBurst=false; burstPeak=0;
}
void setup(){
  Serial.begin(115200); pinMode(PIN_LOP,INPUT); pinMode(PIN_LON,INPUT);
  analogReadResolution(12); analogSetPinAttenuation(PIN_OUT,ADC_11db); delay(800); loadLvl(); banner();
}
void sample(){
  int raw=analogRead(PIN_OUT); lastRaw=raw;
  if(firstSample){center=raw; firstSample=false;}
  center+=(raw-center)/1024;
  int dev=abs(raw-center);
  if(dev>env) env=dev; else env=(env*97)/100;
  if(env>maxEnvEver) maxEnvEver=env;
  ring[ringIdx]=env; ringIdx=(ringIdx+1)%RING;
}
void printLine(){
  int peak=0; for(int i=0;i<RING;i++) if(ring[i]>peak) peak=ring[i];
  bool pads=(digitalRead(PIN_LOP)==LOW && digitalRead(PIN_LON)==LOW);
  Serial.print(F("raw:")); Serial.print(lastRaw); Serial.print(F(",env:")); Serial.print(env);
  Serial.print(F(",peak:")); Serial.print(peak); Serial.print(F(",pads:")); Serial.print(pads?1:0);
  Serial.print(F(",thresh:")); Serial.print(burstLvl+60); Serial.print(F("  [t=")); Serial.print(millis()/1000.0,2); Serial.print(F("s]"));
  if(!pads) Serial.print(F("  CHECK-PADS!")); Serial.println();
}
void handle(char c){
  if(c=='\r'||c=='\n'||c=='\t'||c==' ') return;
  if(c=='h'||c=='H') mark(F("HELP"));
  else if(c=='w'||c=='W') mark(F("WATER"));
  else if(c=='y'||c=='Y') mark(F("YES"));
  else if(c=='n'||c=='N') mark(F("NO"));
  else if(c=='c'||c=='C') doCal();
  else if(c=='t'||c=='T'){ Serial.println(F(">>> HELP! (TEST)")); }
  else if(c=='i'||c=='I') banner();
  else if(c=='+'||c=='='){ burstLvl+=10; saveLvl(); Serial.print(F(">>> thresh now ")); Serial.print(burstLvl+60); Serial.println(F(" (harder)")); }
  else if(c=='-'||c=='_'){ burstLvl-=10; if(burstLvl<50) burstLvl=50; saveLvl(); Serial.print(F(">>> thresh now ")); Serial.print(burstLvl+60); Serial.println(F(" (easier)")); }
  else if(c=='r'||c=='R'){ printMs=(printMs==50)?250:50; Serial.println(printMs==250?F("slow"):F("fast")); }
}
void classify(unsigned long dur){
  int thr=burstLvl+60; // V1.18 decreased +60 not +120 (your V1.16 log small 363-410 were 67-114 below 477, now 7-54 below 417 — press '-' twice for ~397) // dur 15 not 35: 532/29 was 6 short, now 14 above
  if(burstPeak>=thr && dur>=15 && dur<=1500){
    if(millis()-lastFire>=REFRACT){ fire(burstPeak,dur); lastFire=burstStart; }
    else { Serial.print(F(">>> BURST peak=")); Serial.print(burstPeak); Serial.print(F(" dur=")); Serial.print(dur); Serial.println(F("ms -> echo skipped")); }
  } else {
    Serial.print(F(">>> BURST peak=")); Serial.print(burstPeak); Serial.print(F(" dur=")); Serial.print(dur); Serial.print(F("ms -> NOT HELP ("));
    if(burstPeak<thr) Serial.print(F("too weak"));
    else if(dur<15) Serial.print(F("too short"));
    else Serial.print(F("too long"));
    Serial.println(F(")"));
  }
}
void loop(){
  unsigned long now=millis();
  if(now-tS>=SAMPLE_MS){ tS=now; sample(); }
  if(env>=burstLvl){
    if(!inBurst){ inBurst=true; burstStart=now; burstPeak=env; }
    if(env>burstPeak) burstPeak=env;
    lastHigh=now;
    if(now-burstStart>BURSTMAX){ classify(now-burstStart); Serial.println(F(">>> burst >3s -> pad moved, press c")); inBurst=false; burstPeak=0; }
  } else if(inBurst && now-lastHigh>=GRACE){ classify(lastHigh-burstStart); inBurst=false; burstPeak=0; }
  if(now-tP>= (unsigned long)printMs){ tP=now; printLine(); }
  while(Serial.available()) handle(Serial.read());
}
