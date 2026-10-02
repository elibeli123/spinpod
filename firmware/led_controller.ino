#include <Wire.h>
#include <Adafruit_AW9523.h>
#include <SoftwareSerial.h>

// LED Metro UART: D4=RX, D5=TX
SoftwareSerial link(4, 5);

// LED-driver mux + AW
#define LED_MUX_ADDR 0x70
#define AW_ADDR      0x58

void ledMuxSelect(uint8_t ch) {
  if (ch > 7) return;
  Wire.beginTransmission(LED_MUX_ADDR);
  Wire.write(1 << ch);
  Wire.endTransmission();
  delay(2);
}

Adafruit_AW9523 aw;

const uint8_t ledPins[] = {8, 9, 10, 11, 0};
const uint8_t NUM_LEDS = 5;
const uint8_t NUM_MODULES = 8;

uint8_t currentCh = 0;

uint8_t bByModule[NUM_MODULES][NUM_LEDS] = {
  {100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100},
  {100, 100, 100, 100, 100}
};

void replyOK()   { link.println("OK"); }
void replyPONG() { link.println("PONG"); }
void replyERR(const char* msg) { link.print("ERR "); link.println(msg); }

bool parseIntAfterEquals(const String& s, int &out) {
  int eq = s.indexOf('=');
  if (eq < 0) return false;
  out = s.substring(eq + 1).toInt();
  return true;
}

uint8_t clampU8(int v) {
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return (uint8_t)v;
}

void allOff() {
  ledMuxSelect(currentCh);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    aw.analogWrite(ledPins[i], 0);
  }
}

bool ensureAwReadyOnCurrentChannel() {
  ledMuxSelect(currentCh);
  if (!aw.begin(AW_ADDR)) return false;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    aw.pinMode(ledPins[i], AW9523_LED_MODE);
    aw.analogWrite(ledPins[i], 0);
  }
  return true;
}

bool initAllDrivers() {
  for (uint8_t ch = 0; ch < NUM_MODULES; ch++) {
    currentCh = ch;
    if (!ensureAwReadyOnCurrentChannel()) {
      Serial.print("INIT FAIL CH"); Serial.println(ch);
      return false;
    }
  }
  currentCh = 0;
  ensureAwReadyOnCurrentChannel();
  return true;
}

bool setOneOnByIndex(int idx) {
  if (idx < 0 || idx >= (int)NUM_LEDS) return false;
  ledMuxSelect(currentCh);
  allOff();

  aw.analogWrite(ledPins[idx], bByModule[currentCh][idx]);
  return true;
}

bool parseBList5FromSubstring(const String& s, int startPos, uint8_t out[5]) {
  int vals[5] = {0};
  int idx = 0;
  int start = startPos;

  for (int k = startPos; k <= (int)s.length(); k++) {
    if (k == (int)s.length() || s[k] == ',') {
      String part = s.substring(start, k);
      part.trim();
      if (idx < 5) vals[idx++] = part.toInt();
      start = k + 1;
    }
  }

  if (idx != 5) return false;

  for (int i = 0; i < 5; i++) {
    out[i] = clampU8(vals[i]);
  }
  return true;
}

void printCurrentBrightness(uint8_t moduleIdx) {
  Serial.print("Module "); Serial.print(moduleIdx); Serial.print(" brightness = ");
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    Serial.print(bByModule[moduleIdx][i]);
    if (i < NUM_LEDS - 1) Serial.print(",");
  }
  Serial.println();
}

void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  // USB debug
  Serial.print("RX cmd: ");
  Serial.println(line);

  if (line == "PING") {
    replyPONG();
    return;
  }

  if (line == "INIT") {
    bool ok = initAllDrivers();
    if (ok) replyOK();
    else replyERR("INIT");
    return;
  }

  if (line.startsWith("CH=")) {
    int ch;
    if (!parseIntAfterEquals(line, ch)) { replyERR("bad CH"); return; }
    if (ch < 0 || ch > 7) { replyERR("CH range"); return; }

    currentCh = (uint8_t)ch;
    if (!ensureAwReadyOnCurrentChannel()) { replyERR("AW missing"); return; }

    replyOK();
    return;
  }

  if (line == "OFF") {
    allOff();
    replyOK();
    return;
  }

 
  if (line.startsWith("B=")) {
    uint8_t tmp[5];
    if (!parseBList5FromSubstring(line, 2, tmp)) { replyERR("bad B"); return; }

    for (uint8_t ch = 0; ch < NUM_MODULES; ch++) {
      for (uint8_t i = 0; i < NUM_LEDS; i++) {
        bByModule[ch][i] = tmp[i];
      }
    }

    replyOK();
    return;
  }

  if (line.length() >= 4 && line[0] == 'B' && isDigit(line[1]) && line[2] == '=') {
    int which = line.substring(1, 2).toInt();
    if (which < 0 || which > 4) { replyERR("Bn range"); return; }

    int v = line.substring(3).toInt();
    uint8_t val = clampU8(v);

    for (uint8_t ch = 0; ch < NUM_MODULES; ch++) {
      bByModule[ch][which] = val;
    }

    replyOK();
    return;
  }


  if (line.startsWith("BM") && line.length() >= 5 && isDigit(line[2])) {
    int eq = line.indexOf('=');
    if (eq < 0) { replyERR("bad BM"); return; }

    int moduleIdx = line.substring(2, eq).toInt();
    if (moduleIdx < 0 || moduleIdx > 7) { replyERR("BM range"); return; }

    uint8_t tmp[5];
    if (!parseBList5FromSubstring(line, eq + 1, tmp)) { replyERR("bad BM list"); return; }

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      bByModule[moduleIdx][i] = tmp[i];
    }

    replyOK();
    return;
  }


  if (line.startsWith("BM")) {
    int underscore = line.indexOf('_');
    int eq = line.indexOf('=');

    if (underscore > 2 && eq > underscore) {
      int moduleIdx = line.substring(2, underscore).toInt();
      int ledIdx = line.substring(underscore + 1, eq).toInt();
      int v = line.substring(eq + 1).toInt();

      if (moduleIdx < 0 || moduleIdx > 7) { replyERR("BM mod range"); return; }
      if (ledIdx < 0 || ledIdx > 4) { replyERR("BM led range"); return; }

      bByModule[moduleIdx][ledIdx] = clampU8(v);
      replyOK();
      return;
    }
  }

  if (line.startsWith("ON=")) {
    int idx;
    if (!parseIntAfterEquals(line, idx)) { replyERR("bad ON"); return; }
    if (!setOneOnByIndex(idx)) { replyERR("idx"); return; }
    replyOK();
    return;
  }

  if (line == "PRINTB") {
    for (uint8_t ch = 0; ch < NUM_MODULES; ch++) {
      printCurrentBrightness(ch);
    }
    replyOK();
    return;
  }

  replyERR("unknown");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  link.begin(38400);
  Wire.begin();

  Serial.println("LED Metro ready (PING/INIT/CH/B/BM/ON/OFF).");

  currentCh = 0;
  ensureAwReadyOnCurrentChannel();
}

void loop() {
  static String buf;
  while (link.available()) {
    char c = (char)link.read();
    if (c == '\n') {
      handleLine(buf);
      buf = "";
    } else if (c != '\r') {
      buf += c;
      if (buf.length() > 120) buf = "";
    }
  }
}
