/*
 * VocalBridge — SPEAKER TEST B (plain amplifier driver, e.g. PAM8403)
 * --------------------------------------------------------------------
 * Use this ONLY if your "driver" module has NO microSD slot — meaning it
 * is just an amplifier board (PAM8403 / LM386 / NS4160 etc.). If your
 * module has a microSD slot, it is a DFPlayer Mini — use
 * SpeakerTest_DFPlayer.ino instead.
 *
 * It beeps 3 times, waits 1 s, repeats forever. If you hear beeps,
 * the speaker + driver work. That's the whole test.
 *
 * Board:   ESP32 Dev Module
 * Wiring:  Amp VCC   -> VIN (5V)
 *          Amp GND   -> GND
 *          Amp IN +  -> GPIO25 (1k resistor in series if you have one)
 *          Amp IN -  -> GND
 *          Speaker   -> the amp's OUT + / OUT - screw or solder pads
 * Baud:    115200 (optional — just to print status)
 *
 * Why bit-banging instead of tone()/ledc: it works on EVERY version of
 * the ESP32 Arduino core, so this never fails to compile.
 */

const int AUDIO_PIN = 25;
const int BAUD = 115200;

void beep(int ms) {
  // 1 kHz square wave: 250 us high + 250 us low = 1000 Hz
  for (long t = 0; t < ms * 1000L; t += 500) {
    digitalWrite(AUDIO_PIN, HIGH);
    delayMicroseconds(250);
    digitalWrite(AUDIO_PIN, LOW);
    delayMicroseconds(250);
  }
}

void setup() {
  Serial.begin(BAUD);
  pinMode(AUDIO_PIN, OUTPUT);
  Serial.println();
  Serial.println("VocalBridge SpeakerTest B — plain amp board");
  Serial.println("You should hear: beep-beep-beeeeep (1 kHz), every ~2 s.");
  Serial.println("Loud and clean = speaker + driver work.");
}

void loop() {
  beep(150);
  delay(120);
  beep(150);
  delay(120);
  beep(350);
  Serial.println("beep!");
  delay(1100);
}
