/*
 * VocalBridge — SPEAKER TEST A (DFPlayer Mini + speaker)
 * -------------------------------------------------------
 * Answers one question: "does my speaker work?"
 *
 * What it does:
 *   1) Boots, finds the DFPlayer and the SD card, prints a status report
 *   2) Plays track 1 (mp3/0001.mp3 = "Help") about 2 s after boot
 *   3) Then obeys commands from the Serial Monitor:
 *
 *      1-9   play track N (from the /mp3 folder)
 *      + / - volume up / down
 *      n     next track          b    previous track
 *      s     stop                p    pause           r   resume
 *      l     loop track 1        x    root-index play(1) fallback test
 *      h     reprint this help
 *
 * Board:   ESP32 Dev Module
 * Wiring:  DFPlayer VCC -> VIN (5V)
 *          DFPlayer GND -> GND
 *          DFPlayer RX  -> GPIO26   (add a 1k resistor in series if you have one)
 *          DFPlayer TX  -> GPIO27
 *          Speaker      -> SPK_1 and SPK_2   (never to GND!)
 *          microSD      -> FAT32, folder "mp3" in the root, files
 *                          mp3/0001.mp3, mp3/0002.mp3, ...
 * Library: "DFRobotDFPlayerMini" (Arduino Library Manager)
 * Baud:    115200 (Serial Monitor)
 *
 * If your driver module has NO microSD slot (it is just an amplifier,
 * e.g. PAM8403), use SpeakerTest_Tone_Amp.ino instead.
 */

#include <DFRobotDFPlayerMini.h>

const int BAUD = 115200;

// Serial2 pins (matches the project wiring docs):
//   GPIO27 = ESP32 RX  <- DFPlayer TX
//   GPIO26 = ESP32 TX  -> DFPlayer RX
#define DF_RX_PIN 27
#define DF_TX_PIN 26

DFRobotDFPlayerMini player;
bool speakerOnline = false;
int  volume        = 20;          // 0..30 — clones distort near 30, stay <= 25
unsigned long lastCmdMs = 0;      // DFPlayer clones glitch if you spam commands

// --------------------------------------------------------------- helpers
void printHelp() {
  Serial.println();
  Serial.println("---------------- SPEAKER TEST — commands ----------------");
  Serial.println("  1-9   play track N (mp3/000N.mp3)");
  Serial.println("  + -   volume up / down          n / b  next / previous");
  Serial.println("  s     stop                      p / r   pause / resume");
  Serial.println("  l     loop track 1 (send s to stop)");
  Serial.println("  x     fallback: play file #1 found on card (root test)");
  Serial.println("  h     this help");
  Serial.println("----------------------------------------------------------");
  Serial.println();
}

void volumeMsg() {
  Serial.print(">> volume = ");
  Serial.println(volume);
}

void clampVolume() {
  if (volume > 30) volume = 30;
  if (volume < 0)  volume = 0;
}

// Decodes the DFPlayer's own event/error messages (from the DFRobot example)
void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:           Serial.println("[DF] timeout — wiring?"); break;
    case WrongStack:        Serial.println("[DF] stack error — TX/RX crossed?"); break;
    case DFPlayerCardOnline:  Serial.println("[DF] card online"); break;
    case DFPlayerCardInserted: Serial.println("[DF] card inserted"); break;
    case DFPlayerCardRemoved:  Serial.println("[DF] CARD REMOVED — push it back in!"); break;
    case DFPlayerError:
      Serial.print("[DF] ERROR ");
      switch (value) {
        case Busy:            Serial.println("busy"); break;
        case Sleeping:        Serial.println("sleeping"); break;
        case SerialWrongStack: Serial.println("serial stack"); break;
        case ChecksumNotMatch: Serial.println("checksum — flaky wires"); break;
        case FileIndexOut:    Serial.println("no such track number"); break;
        case FileMismatch:    Serial.println("file mismatch"); break;
        default:              Serial.println("unknown"); break;
      }
      break;
    default: break;
  }
}

void handleChar(char c) {
  if (c == '\r' || c == '\n' || c == ' ') return;      // ignore line endings
  if (millis() - lastCmdMs < 150) return;              // don't spam the module
  lastCmdMs = millis();

  if (c >= '1' && c <= '9') {
    int t = c - '0';
    Serial.print(">> play mp3/000"); Serial.print(t); Serial.println(".mp3");
    player.playMp3Folder(t);
  }
  else if (c == '+') { volume += 2; clampVolume(); player.volume(volume); volumeMsg(); }
  else if (c == '-') { volume -= 2; clampVolume(); player.volume(volume); volumeMsg(); }
  else if (c == 'n') { Serial.println(">> next");     player.next(); }
  else if (c == 'b') { Serial.println(">> previous"); player.previous(); }
  else if (c == 's') { Serial.println(">> stop");     player.stop(); }
  else if (c == 'p') { Serial.println(">> pause");    player.pause(); }
  else if (c == 'r') { Serial.println(">> resume");   player.start(); }
  else if (c == 'l') { Serial.println(">> looping track 1 (s = stop)"); player.loop(1); }
  else if (c == 'x') { Serial.println(">> root-index test: play(1)");  player.play(1); }
  else if (c == 'h' || c == '?') printHelp();
  else { Serial.print("?? unknown '"); Serial.write(c); Serial.println("' — send h for help"); }
}

// ----------------------------------------------------------------- setup
void setup() {
  Serial.begin(BAUD);
  delay(800);

  Serial.println();
  Serial.println("VocalBridge SpeakerTest — DFPlayer Mini");
  Serial.println("----------------------------------------");

  Serial2.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);

  // ---- find the DFPlayer (up to 5 tries) ----
  for (int i = 1; i <= 5 && !speakerOnline; i++) {
    Serial.print("Looking for DFPlayer (attempt ");
    Serial.print(i); Serial.println("/5)...");
    speakerOnline = player.begin(Serial2, /*isACK*/ true, /*doReset*/ true);
    if (!speakerOnline) delay(400);
  }

  if (!speakerOnline) {
    Serial.println();
    Serial.println("!! DFPlayer NOT FOUND — sound test cannot run.");
    Serial.println("!! Check in this order:");
    Serial.println("!!   1. VCC -> VIN (5V) and GND -> GND");
    Serial.println("!!   2. RX -> GPIO26 and TX -> GPIO27  (they CROSS — easy to swap!)");
    Serial.println("!!   3. microSD pushed FULLY in, FAT32, at least one real .mp3");
    Serial.println("!!   4. ESP32 powered from USB, not a weak battery");
    Serial.println();
    Serial.println("Fix any of those, then press the ESP32 RST (EN) button.");
    return;
  }

  Serial.println(">> DFPlayer ONLINE.");
  delay(200);
  player.volume(volume);
  delay(150);

  int files = player.readFileCounts();
  Serial.print(">> Files found on SD card: ");
  if (files <= 0 || files == 65535) Serial.println("unknown (card read error — is it FAT32?)");
  else           Serial.println(files);

  printHelp();
  Serial.println(">> Playing mp3/0001.mp3 in 2 seconds — LISTEN...");
  Serial.println();
  delay(2000);
  player.playMp3Folder(1);          // 0001.mp3 = "Help"
}

// ------------------------------------------------------------------ loop
void loop() {
  if (!speakerOnline) {
    delay(5000);
    Serial.println("(still no DFPlayer — check wiring, then press RST)");
    return;
  }

  // report any DFPlayer events/errors
  if (player.available()) printDetail(player.readType(), player.read());

  // obey Serial Monitor commands
  if (Serial.available()) handleChar(Serial.read());
}
