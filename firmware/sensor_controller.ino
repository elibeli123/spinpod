#include <Wire.h>
#include <SoftwareSerial.h>
#include "SparkFun_AS7265X.h"

// UART to LED Metro

// Sensor Metro: D4=RX, D5=TX
SoftwareSerial link(4, 5);

bool waitForLineUART(char* out, size_t outSize, unsigned long timeoutMs) {
  unsigned long t0 = millis();
  size_t n = 0;
  while (millis() - t0 < timeoutMs) {
    while (link.available()) {
      char c = (char)link.read();
      if (c == '\n') {
        out[n] = 0;
        if (n > 0 && out[n - 1] == '\r') out[n - 1] = 0;
        return true;
      }
      if (n + 1 < outSize) out[n++] = c;
    }
  }
  return false;
}

bool sendCmdWaitOK(const char* cmd, unsigned long timeoutMs = 2000) {
  link.println(cmd);
  char resp[96];
  if (!waitForLineUART(resp, sizeof(resp), timeoutMs)) return false;
  return (strcmp(resp, "OK") == 0 || strcmp(resp, "PONG") == 0);
}

bool sendCmdWaitOK_retry(const char* cmd, uint8_t tries = 3, unsigned long timeoutMs = 2000) {
  for (uint8_t k = 0; k < tries; k++) {
    if (sendCmdWaitOK(cmd, timeoutMs)) return true;
    delay(50);
  }
  return false;
}

// LED Metro helpers
bool ledPing() { return sendCmdWaitOK_retry("PING"); }

bool ledInitAllDrivers() {
  return sendCmdWaitOK_retry("INIT", 2, 8000);
}

bool ledSelectChannel(uint8_t ch) {
  char cmd[8];
  snprintf(cmd, sizeof(cmd), "CH=%u", ch);
  return sendCmdWaitOK_retry(cmd);
}

bool ledAllOff() { return sendCmdWaitOK_retry("OFF"); }

bool ledOnIndex(uint8_t idx) {
  char cmd[12];
  snprintf(cmd, sizeof(cmd), "ON=%u", idx);
  return sendCmdWaitOK_retry(cmd);
}

bool ledSetBrightnessAll5(const uint8_t b[5]) {
  char cmd[48];
  snprintf(cmd, sizeof(cmd), "B=%u,%u,%u,%u,%u", b[0], b[1], b[2], b[3], b[4]);
  return sendCmdWaitOK_retry(cmd);
}

bool ledSetBrightnessOne(uint8_t i, uint8_t val) {
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "B%u=%u", i, val);
  return sendCmdWaitOK_retry(cmd);
}

// Optional passthrough for per-module brightness
bool ledSetBrightnessModule5(uint8_t moduleIdx, const uint8_t b[5]) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "BM%u=%u,%u,%u,%u,%u",
           moduleIdx, b[0], b[1], b[2], b[3], b[4]);
  return sendCmdWaitOK_retry(cmd);
}

bool ledSetBrightnessModuleOne(uint8_t moduleIdx, uint8_t ledIdx, uint8_t val) {
  char cmd[24];
  snprintf(cmd, sizeof(cmd), "BM%u_%u=%u", moduleIdx, ledIdx, val);
  return sendCmdWaitOK_retry(cmd);
}


// Sensors: TCA mux + AS7265x
#define SENSOR_MUX_ADDR 0x70

AS7265X sensor;

const uint8_t MAX_SYSTEMS = 8;
bool ch_ok[MAX_SYSTEMS] = {false};

// labels (for output)
const int ledWavelengths[5] = {695, 465, 850, 640, 569};
const uint8_t ledIdxMap[5]  = {0, 1, 2, 3, 4};

// Timing model
const float singleIntegrationTime = 0.504 * 2;
const float targetIntegrationTime = 10;
const int num = int(targetIntegrationTime / singleIntegrationTime);

uint8_t brightness5[5] = {100, 100, 100, 100, 100};

bool started = false;

// Ordered run list
uint8_t runList[MAX_SYSTEMS] = {0, 1, 2, 3, 4, 5, 6, 7};
uint8_t runCount = MAX_SYSTEMS;

// End-of-run cooldown: 10 minutes
const unsigned long COOLDOWN_AFTER_ALL_MS = 60000UL;

// Sensor mux helpers 
void sensorMuxSelect(uint8_t ch) {
  if (ch > 7) return;
  Wire.beginTransmission(SENSOR_MUX_ADDR);
  Wire.write(1 << ch);
  Wire.endTransmission();
  delay(5);
}

// Helps continuous mode after switching channels
void flushOneSampleAfterSwitch(uint8_t ch) {
  sensorMuxSelect(ch);
  delay(10);
  while (sensor.dataAvailable() == false) {}
  (void)sensor.getCalibratedA();
}

bool initSensorOnChannel(uint8_t ch) {
  sensorMuxSelect(ch);
  if (!sensor.begin()) return false;

  sensor.disableIndicator();
  sensor.disableBulb(AS7265x_LED_WHITE);
  sensor.disableBulb(AS7265x_LED_IR);
  sensor.disableBulb(AS7265x_LED_UV);

  sensor.setIntegrationCycles(179);
  sensor.setMeasurementMode(AS7265X_MEASUREMENT_MODE_6CHAN_CONTINUOUS);
  sensor.setGain(AS7265X_GAIN_16X);

  flushOneSampleAfterSwitch(ch);
  return true;
}

void initAllSensors() {
  for (uint8_t ch = 0; ch < MAX_SYSTEMS; ch++) {
    ch_ok[ch] = initSensorOnChannel(ch);
    Serial.print("Sensor CH"); Serial.print(ch);
    Serial.println(ch_ok[ch] ? " init OK" : " init FAIL");
    delay(20);
  }
}

// Output row 
void printMeasurement(uint8_t sensor_id, const char* label) {
  sensorMuxSelect(sensor_id);
  while (sensor.dataAvailable() == false) {}

  Serial.print(millis());  Serial.print(",");
  Serial.print(sensor_id); Serial.print(",");
  Serial.print(label);     Serial.print(",");

  Serial.print(sensor.getCalibratedA()); Serial.print(",");
  Serial.print(sensor.getCalibratedB()); Serial.print(",");
  Serial.print(sensor.getCalibratedC()); Serial.print(",");
  Serial.print(sensor.getCalibratedD()); Serial.print(",");
  Serial.print(sensor.getCalibratedE()); Serial.print(",");
  Serial.print(sensor.getCalibratedF()); Serial.print(",");
  Serial.print(sensor.getCalibratedG()); Serial.print(",");
  Serial.print(sensor.getCalibratedH()); Serial.print(",");
  Serial.print(sensor.getCalibratedR()); Serial.print(",");
  Serial.print(sensor.getCalibratedI()); Serial.print(",");
  Serial.print(sensor.getCalibratedS()); Serial.print(",");
  Serial.print(sensor.getCalibratedJ()); Serial.print(",");
  Serial.print(sensor.getCalibratedT()); Serial.print(",");
  Serial.print(sensor.getCalibratedU()); Serial.print(",");
  Serial.print(sensor.getCalibratedV()); Serial.print(",");
  Serial.print(sensor.getCalibratedW()); Serial.print(",");
  Serial.print(sensor.getCalibratedK()); Serial.print(",");
  Serial.print(sensor.getCalibratedL());
  Serial.println();
}

// One sensor system cycle
void runOneSensorSystem(uint8_t sid) {
  ledSelectChannel(sid);
  flushOneSampleAfterSwitch(sid);

  // Baseline
  ledAllOff();
  delay(500);
  for (int m = 0; m < 5; m++) printMeasurement(sid, "Baseline");

  // 5 LEDs
  for (int i = 0; i < 5; i++) {
    ledOnIndex(ledIdxMap[i]);
    delay(1500);

    char label[12];
    snprintf(label, sizeof(label), "%dnm", ledWavelengths[i]);

    for (int m = 0; m < num; m++) printMeasurement(sid, label);

    ledAllOff();
  }
}


// Run-list helpers

void setRunListAll() {
  runCount = MAX_SYSTEMS;
  for (uint8_t i = 0; i < MAX_SYSTEMS; i++) runList[i] = i;
}

bool systemAlreadyInList(uint8_t sid) {
  for (uint8_t i = 0; i < runCount; i++) {
    if (runList[i] == sid) return true;
  }
  return false;
}

bool parseListCommand(const String& line) {
  // expects LIST=1,7,4 or LIST=4
  runCount = 0;

  int start = 5; // after "LIST="
  for (int k = start; k <= (int)line.length(); k++) {
    if (k == (int)line.length() || line[k] == ',') {
      String part = line.substring(start, k);
      part.trim();

      if (part.length() == 0) return false;
      int sid = part.toInt();
      if (sid < 0 || sid > 7) return false;
      if (runCount >= MAX_SYSTEMS) return false;

      // prevent duplicates
      bool duplicate = false;
      for (uint8_t i = 0; i < runCount; i++) {
        if (runList[i] == (uint8_t)sid) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) return false;

      runList[runCount++] = (uint8_t)sid;
      start = k + 1;
    }
  }

  return (runCount > 0);
}

bool parseExceptCommand(const String& line) {
  // expects EXCEPT=7 or EXCEPT=1,7,4
  bool excluded[MAX_SYSTEMS] = {false};

  int start = 7; // after "EXCEPT="
  bool sawAny = false;

  for (int k = start; k <= (int)line.length(); k++) {
    if (k == (int)line.length() || line[k] == ',') {
      String part = line.substring(start, k);
      part.trim();

      if (part.length() == 0) return false;
      int sid = part.toInt();
      if (sid < 0 || sid > 7) return false;

      excluded[sid] = true;
      sawAny = true;
      start = k + 1;
    }
  }

  if (!sawAny) return false;

  runCount = 0;
  for (uint8_t sid = 0; sid < MAX_SYSTEMS; sid++) {
    if (!excluded[sid]) runList[runCount++] = sid;
  }

  return (runCount > 0);
}

void printRunList() {
  Serial.print("runList=");
  for (uint8_t i = 0; i < runCount; i++) {
    Serial.print(runList[i]);
    if (i + 1 < runCount) Serial.print(",");
  }
  Serial.println();
}

// =====================
// USB command parser on Sensor Metro
// =====================
String usbBuf;

void printStatus() {
  Serial.println("=== STATUS ===");
  Serial.print("started="); Serial.println(started ? "true" : "false");
  printRunList();

  Serial.print("brightness5=");
  Serial.print(brightness5[0]); Serial.print(",");
  Serial.print(brightness5[1]); Serial.print(",");
  Serial.print(brightness5[2]); Serial.print(",");
  Serial.print(brightness5[3]); Serial.print(",");
  Serial.println(brightness5[4]);

  for (uint8_t i = 0; i < MAX_SYSTEMS; i++) {
    Serial.print("Sensor CH"); Serial.print(i);
    Serial.print(": "); Serial.println(ch_ok[i] ? "OK" : "FAIL");
  }

  Serial.println("Commands:");
  Serial.println("  LIST=ALL");
  Serial.println("  LIST=1,7,4");
  Serial.println("  EXCEPT=7");
  Serial.println("  EXCEPT=1,7,4");
  Serial.println("  B=a,b,c,d,e           -> set ALL modules");
  Serial.println("  B0=.. B1=.. B2=..     -> set ALL modules");
  Serial.println("  BM5=100,100,100,100,100");
  Serial.println("  BM5_0=120");
  Serial.println("  START | STOP | STATUS");
}

uint8_t clampU8(long v) {
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return (uint8_t)v;
}

void handleUsbLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "STATUS") {
    printStatus();
    return;
  }

  if (line == "LIST=ALL") {
    setRunListAll();
    Serial.println("OK LIST");
    return;
  }

  if (line.startsWith("LIST=")) {
    if (parseListCommand(line)) Serial.println("OK LIST");
    else Serial.println("ERR bad LIST");
    return;
  }

  if (line.startsWith("EXCEPT=")) {
    if (parseExceptCommand(line)) Serial.println("OK EXCEPT");
    else Serial.println("ERR bad EXCEPT");
    return;
  }

  if (line == "START") {
    started = true;
    Serial.println("OK START");
    return;
  }

  if (line == "STOP") {
    started = false;
    ledAllOff();
    Serial.println("OK STOP");
    return;
  }

  // Global brightness commands
  if (line.startsWith("B=")) {
    int vals[5] = {0};
    int idx = 0;
    int start = 2;
    for (int k = 2; k <= (int)line.length(); k++) {
      if (k == (int)line.length() || line[k] == ',') {
        String part = line.substring(start, k);
        part.trim();
        if (idx < 5) vals[idx++] = part.toInt();
        start = k + 1;
      }
    }
    if (idx != 5) {
      Serial.println("ERR B= needs 5 comma values");
      return;
    }
    for (int i = 0; i < 5; i++) brightness5[i] = clampU8(vals[i]);
    Serial.println("OK B (stored)");
    ledSetBrightnessAll5(brightness5);
    return;
  }

  if (line.length() >= 4 && line[0] == 'B' && isDigit(line[1]) && line[2] == '=') {
    int which = line.substring(1, 2).toInt();
    if (which < 0 || which > 4) {
      Serial.println("ERR Bn index 0..4");
      return;
    }
    int v = line.substring(3).toInt();
    brightness5[which] = clampU8(v);
    Serial.println("OK Bn (stored)");
    ledSetBrightnessOne((uint8_t)which, brightness5[which]);
    return;
  }

  // Per-module brightness passthrough
  if (line.startsWith("BM") && line.indexOf('_') < 0 && line.indexOf('=') > 2) {
    int eq = line.indexOf('=');
    int moduleIdx = line.substring(2, eq).toInt();
    if (moduleIdx < 0 || moduleIdx > 7) {
      Serial.println("ERR BM module 0..7");
      return;
    }

    int vals[5] = {0};
    int idx = 0;
    int start = eq + 1;
    for (int k = eq + 1; k <= (int)line.length(); k++) {
      if (k == (int)line.length() || line[k] == ',') {
        String part = line.substring(start, k);
        part.trim();
        if (idx < 5) vals[idx++] = part.toInt();
        start = k + 1;
      }
    }

    if (idx != 5) {
      Serial.println("ERR BM needs 5 comma values");
      return;
    }

    uint8_t tmp[5];
    for (int i = 0; i < 5; i++) tmp[i] = clampU8(vals[i]);

    if (ledSetBrightnessModule5((uint8_t)moduleIdx, tmp)) Serial.println("OK BM");
    else Serial.println("ERR BM send");
    return;
  }

  if (line.startsWith("BM") && line.indexOf('_') > 2 && line.indexOf('=') > line.indexOf('_')) {
    int underscore = line.indexOf('_');
    int eq = line.indexOf('=');

    int moduleIdx = line.substring(2, underscore).toInt();
    int ledIdx = line.substring(underscore + 1, eq).toInt();
    int v = line.substring(eq + 1).toInt();

    if (moduleIdx < 0 || moduleIdx > 7) {
      Serial.println("ERR BM module 0..7");
      return;
    }
    if (ledIdx < 0 || ledIdx > 4) {
      Serial.println("ERR BM led 0..4");
      return;
    }

    if (ledSetBrightnessModuleOne((uint8_t)moduleIdx, (uint8_t)ledIdx, clampU8(v))) Serial.println("OK BM1");
    else Serial.println("ERR BM1 send");
    return;
  }

  Serial.println("ERR unknown cmd");
}

void serviceUsbCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      handleUsbLine(usbBuf);
      usbBuf = "";
    } else if (c != '\r') {
      usbBuf += c;
      if (usbBuf.length() > 120) usbBuf = "";
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // UART to LED Metro
  link.begin(38400);

  // I2C for sensors
  Wire.begin();
  Wire.setClock(100000);

  // CSV header
  Serial.println("time(ms),sensor_id,trial,410nm,435nm,460nm,485nm,510nm,535nm,560nm,585nm,610nm,645nm,680nm,705nm,730nm,760nm,810nm,860nm,900nm,940nm");

  // init sensors
  initAllSensors();

  // init LED drivers
  if (!ledPing()) Serial.println("WARN: LED MCU no PONG");
  if (!ledInitAllDrivers()) Serial.println("WARN: LED MCU INIT failed");
  ledAllOff();
  ledSetBrightnessAll5(brightness5);

  // default run list = ALL
  setRunListAll();

  printStatus();
}

void loop() {
  serviceUsbCommands();

  if (!started) return;

  for (uint8_t i = 0; i < runCount; i++) {
    serviceUsbCommands();
    if (!started) break;

    uint8_t sid = runList[i];
    if (!ch_ok[sid]) continue;

    runOneSensorSystem(sid);
  }

  ledAllOff();
  Serial.print("---- END runList ");
  for (uint8_t i = 0; i < runCount; i++) {
    Serial.print(runList[i]);
    if (i + 1 < runCount) Serial.print(",");
  }
  Serial.println("; cooldown 10 min ----");

  unsigned long t0 = millis();
  while (started && (millis() - t0 < COOLDOWN_AFTER_ALL_MS)) {
    serviceUsbCommands();
    delay(20);
  }
}
