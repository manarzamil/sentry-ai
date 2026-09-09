/*
 * Sentry AI - AI Smart Safety Helmet
 * Firmware for ESP32 (Arduino framework)
 *
 * Functionality: three-phase fall detection (MPU-6050), GPS location with
 * last-known-fix caching (NEO-6M), rule-based fatigue scoring, buzzer alerts,
 * incident persistence in NVS, and a self-hosted Wi-Fi SoftAP dashboard.
 *
 * Credentials are NOT stored in this file. See config.example.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <math.h>

#include "config.h"   // defines SSID and PASSWORD

Adafruit_MPU6050 mpu;
TinyGPSPlus      gps;
HardwareSerial   gpsSerial(2);
WebServer        server(80);
Preferences      prefs;

// Wi-Fi SoftAP credentials are provided by config.h (git-ignored).
// Copy config.example.h to config.h and set your own values before flashing.

const int BUZZER_PIN = 25;
const int BUTTON_PIN = 4;
const int SWITCH_PIN = 13;

const char* WORKER_ID   = "W-01";
const char* WORKER_NAME = "Worker 1";

const float    FF_THR     =  4.9f;
const float    IMP_THR    = 24.5f;
const float    STILL_DEV  =  2.94f;
const uint32_t FF_MIN_MS  =  80;
const uint32_t IMP_MAX_MS = 500;
const uint32_t STILL_MS   = 600;

const int   AI_BUF_SIZE  = 50;
const float FAT_ACT_THR  =  1.18f;
const float FAT_TILT_THR = 25.0f;
const int   FAT_DUR_S    = 30;

enum FallPhase { FP_IDLE, FP_FREEFALL, FP_IMPACT, FP_CONFIRM };
FallPhase fallPhase    = FP_IDLE;
uint32_t  ffStartMs    = 0;
uint32_t  impStartMs   = 0;
uint32_t  stillStartMs = 0;

float ax = 0, ay = 0, az = 0;
float gx = 0, gy = 0, gz = 0;
float totalAcc = 9.81f;
float tiltDeg  = 0.0f;

float    lastValidLat = 0.0f;
float    lastValidLng = 0.0f;
bool     gpsValid     = false;
uint32_t gpsLostMs    = 0;

bool fallDetected = false;
bool sosActive    = false;
bool fatigueAlert = false;
int  fatigueScore = 0;
bool helmetWorn   = false;
bool systemActive = false;

int fallCount = 0;
int sosCount  = 0;

String alertLog = "";

float aiBuf[AI_BUF_SIZE];
int   aiBufIdx   = 0;
int   aiBufCount = 0;

struct Zone { float lat, lng; int cnt; char name[12]; };
Zone zones[5];
int  zoneCount = 0;

bool     buzOn      = false;
uint32_t buzStart   = 0;
int      buzPattern = 0;

uint32_t lastAiMs       = 0;
uint32_t lastFatAlert   = 0;
int      continuousFatS = 0;
int      offlineQueue   = 0;
uint32_t bootTime       = 0;
uint32_t switchOnTime   = 0;

void addLog(const String& msg) {
  alertLog = msg + "||" + alertLog;
  int sep = 0, pos = 0;
  while ((pos = alertLog.indexOf("||", pos)) != -1 && sep < 9) {
    sep++; pos += 2;
  }
  if (sep >= 9 && pos != -1) alertLog = alertLog.substring(0, pos);
}

void startupBeep() {
  digitalWrite(BUZZER_PIN, HIGH); delay(100);
  digitalWrite(BUZZER_PIN, LOW);  delay(100);
  digitalWrite(BUZZER_PIN, HIGH); delay(100);
  digitalWrite(BUZZER_PIN, LOW);  delay(100);
  digitalWrite(BUZZER_PIN, HIGH); delay(300);
  digitalWrite(BUZZER_PIN, LOW);
}

void shutdownBeep() {
  digitalWrite(BUZZER_PIN, HIGH); delay(500);
  digitalWrite(BUZZER_PIN, LOW);  delay(100);
  digitalWrite(BUZZER_PIN, HIGH); delay(200);
  digitalWrite(BUZZER_PIN, LOW);
}

void saveSessionData() {
  prefs.begin("session", false);
  prefs.putInt("fall_cnt", fallCount);
  prefs.putInt("sos_cnt",  sosCount);
  prefs.putInt("zone_cnt", zoneCount);
  for (int i = 0; i < zoneCount; i++) {
    String key = "z" + String(i);
    String val = String(zones[i].lat, 6) + "," +
                 String(zones[i].lng, 6) + "," +
                 String(zones[i].cnt);
    prefs.putString(key.c_str(), val);
  }
  prefs.end();
}

void loadSessionData() {
  prefs.begin("session", true);
  fallCount = prefs.getInt("fall_cnt", 0);
  sosCount  = prefs.getInt("sos_cnt",  0);
  zoneCount = prefs.getInt("zone_cnt", 0);
  for (int i = 0; i < zoneCount && i < 5; i++) {
    String key = "z" + String(i);
    String val = prefs.getString(key.c_str(), "");
    if (val.length() > 0) {
      int c1 = val.indexOf(',');
      int c2 = val.indexOf(',', c1 + 1);
      zones[i].lat = val.substring(0, c1).toFloat();
      zones[i].lng = val.substring(c1 + 1, c2).toFloat();
      zones[i].cnt = val.substring(c2 + 1).toInt();
      snprintf(zones[i].name, 12, "Zone %c", 'A' + i);
    }
  }
  prefs.end();
  Serial.printf("[BOOT] Loaded: %d falls %d SOS %d zones\n",
    fallCount, sosCount, zoneCount);
}

void clearSessionData() {
  prefs.begin("session", false);
  prefs.clear();
  prefs.end();
  fallCount = 0;
  sosCount  = 0;
  zoneCount = 0;
  alertLog  = "";
  Serial.println("[SESSION] Cleared");
}

void saveOffline(const char* type, float lat, float lng) {
  prefs.begin("helm", false);
  int n = prefs.getInt("n", 0);
  if (n < 20) {
    String v = String(type) + "," + String(lat, 6) + "," + String(lng, 6);
    prefs.putString(("e" + String(n)).c_str(), v);
    prefs.putInt("n", n + 1);
    offlineQueue = n + 1;
  }
  prefs.end();
}

void flushOfflineEvents() {
  prefs.begin("helm", false);
  int n = prefs.getInt("n", 0);
  if (n > 0) {
    Serial.printf("[WiFi] Flushing %d stored events\n", n);
    prefs.clear();
    offlineQueue = 0;
  }
  prefs.end();
}

void updateZone(float lat, float lng) {
  if (lat == 0 && lng == 0) return;
  const float R = 0.0002f;
  for (int i = 0; i < zoneCount; i++) {
    float d = sqrtf(powf(lat - zones[i].lat, 2) + powf(lng - zones[i].lng, 2));
    if (d < R) { zones[i].cnt++; saveSessionData(); return; }
  }
  if (zoneCount < 5) {
    zones[zoneCount].lat = lat;
    zones[zoneCount].lng = lng;
    zones[zoneCount].cnt = 1;
    snprintf(zones[zoneCount].name, 12, "Zone %c", 'A' + zoneCount);
    zoneCount++;
    saveSessionData();
  }
}

String zonesJSON() {
  String j = "[";
  for (int i = 0; i < zoneCount; i++) {
    if (i) j += ",";
    j += "{\"n\":\""; j += zones[i].name;
    j += "\",\"lat\":";  j += String(zones[i].lat, 6);
    j += ",\"lng\":";    j += String(zones[i].lng, 6);
    j += ",\"c\":";      j += zones[i].cnt; j += "}";
  }
  return j + "]";
}

void readGPS() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
  if (gps.location.isValid() && gps.location.age() < 2000) {
    lastValidLat = gps.location.lat();
    lastValidLng = gps.location.lng();
    gpsValid     = true;
    gpsLostMs    = 0;
  } else {
    gpsValid = false;
    if (!gpsLostMs) gpsLostMs = millis();
  }
}

void detectHelmetWorn() {
  helmetWorn = (fabsf(totalAcc - 9.81f) < 3.0f && tiltDeg < 60.0f);
}

void detectFall() {
  uint32_t now = millis();
  switch (fallPhase) {

    case FP_IDLE:
      if (totalAcc < FF_THR) {
        fallPhase = FP_FREEFALL;
        ffStartMs = now;
      }
      break;

    case FP_FREEFALL:
      if (totalAcc >= FF_THR) {
        if (now - ffStartMs >= FF_MIN_MS) {
          fallPhase  = FP_IMPACT;
          impStartMs = now;
        } else {
          fallPhase = FP_IDLE;
        }
      }
      if (now - ffStartMs > 1500) fallPhase = FP_IDLE;
      break;

    case FP_IMPACT:
      if (totalAcc > IMP_THR) {
        stillStartMs = now;
        fallPhase    = FP_CONFIRM;
      }
      if (now - impStartMs > IMP_MAX_MS) fallPhase = FP_IDLE;
      break;

    case FP_CONFIRM:
      if (fabsf(totalAcc - 9.81f) < STILL_DEV) {
        if (now - stillStartMs >= STILL_MS && !fallDetected) {
          fallDetected = true;
          fallCount++;
          digitalWrite(BUZZER_PIN, HIGH);
          buzOn = true; buzStart = now; buzPattern = 0;
          String entry = "FALL #" + String(fallCount) +
            " | " + String(lastValidLat, 5) + "," + String(lastValidLng, 5);
          addLog(entry);
          saveOffline("FALL", lastValidLat, lastValidLng);
          updateZone(lastValidLat, lastValidLng);
          saveSessionData();
          Serial.println("[FALL] CONFIRMED");
        }
      } else {
        if (now - stillStartMs < 200) fallPhase = FP_IDLE;
      }
      break;
  }
}

void runAI() {
  if (aiBufCount < AI_BUF_SIZE) return;
  if (millis() - lastAiMs < 2500) return;
  lastAiMs = millis();

  float sum = 0;
  for (int i = 0; i < AI_BUF_SIZE; i++) sum += fabsf(aiBuf[i] - 9.81f);
  float avgAct = sum / AI_BUF_SIZE;

  float var = 0;
  for (int i = 0; i < AI_BUF_SIZE; i++) {
    float d = fabsf(aiBuf[i] - 9.81f) - avgAct;
    var += d * d;
  }
  var /= AI_BUF_SIZE;

  float tilt  = tiltDeg;
  float score = 0;

  if (avgAct < FAT_ACT_THR)  score += (1.0f - avgAct / FAT_ACT_THR) * 40.0f;
  if (tilt   > FAT_TILT_THR) score += constrain((tilt - FAT_TILT_THR) / 20.0f * 35.0f, 0.0f, 35.0f);
  if (var    < 0.05f)        score += 25.0f;

  fatigueScore = (int)constrain(score, 0, 100);

  if (fatigueScore > 60) {
    continuousFatS += 2;
    if (continuousFatS >= FAT_DUR_S && millis() - lastFatAlert > 120000UL) {
      fatigueAlert = true;
      lastFatAlert = millis();
      buzOn = true; buzStart = millis(); buzPattern = 2;
      addLog("FATIGUE | act:" + String(avgAct, 2) +
             " tilt:" + String(tilt, 0) + "deg sc:" + String(fatigueScore));
    }
  } else {
    continuousFatS = max(0, continuousFatS - 1);
    if (fatigueScore < 30) fatigueAlert = false;
  }

  Serial.printf("[AI] act=%.2f tilt=%.0f var=%.4f score=%d worn=%s\n",
    avgAct, tilt, var, fatigueScore, helmetWorn ? "YES" : "NO");
}

void controlBuzzer() {
  if (!buzOn) { digitalWrite(BUZZER_PIN, LOW); return; }
  uint32_t e = millis() - buzStart;
  int c;
  switch (buzPattern) {
    case 0:
      if (e < 10000) { c = e % 300; digitalWrite(BUZZER_PIN, c < 200 ? HIGH : LOW); }
      else buzOn = false;
      break;
    case 1:
      if (e < 15000) { c = e % 1000; digitalWrite(BUZZER_PIN, c < 600 ? HIGH : LOW); }
      else buzOn = false;
      break;
    case 2:
      if (e < 5000) { c = e % 1000; digitalWrite(BUZZER_PIN, c < 500 ? HIGH : LOW); }
      else buzOn = false;
      break;
  }
}

void systemStart() {
  systemActive  = true;
  switchOnTime  = millis();
  fallDetected  = false;
  sosActive     = false;
  fatigueAlert  = false;
  fallPhase     = FP_IDLE;
  fatigueScore  = 0;
  aiBufIdx      = 0;
  aiBufCount    = 0;
  buzOn         = false;
  digitalWrite(BUZZER_PIN, LOW);
  loadSessionData();
  startupBeep();
  addLog("SYSTEM ON");
  Serial.println("[SWITCH] Monitoring started");
}

void systemStop() {
  systemActive = false;
  fallPhase    = FP_IDLE;
  buzOn        = false;
  digitalWrite(BUZZER_PIN, LOW);
  saveSessionData();
  shutdownBeep();
  addLog("SYSTEM OFF");
  Serial.println("[SWITCH] Monitoring stopped - data saved");
}

void handleData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  uint32_t lostS     = gpsLostMs ? (millis() - gpsLostMs) / 1000 : 0;
  uint32_t uptimeSec = systemActive ? (millis() - switchOnTime) / 1000 : 0;
  String j = "{";
  j += "\"worker_id\":\"";    j += WORKER_ID;   j += "\"";
  j += ",\"worker_name\":\""; j += WORKER_NAME; j += "\"";
  j += ",\"system_active\":"; j += systemActive ? "true" : "false";
  j += ",\"ax\":";  j += String(ax, 2);
  j += ",\"ay\":";  j += String(ay, 2);
  j += ",\"az\":";  j += String(az, 2);
  j += ",\"mag\":"; j += String(totalAcc, 2);
  j += ",\"tilt\":"; j += String(tiltDeg, 1);
  j += ",\"gx\":";  j += String(gx, 1);
  j += ",\"gy\":";  j += String(gy, 1);
  j += ",\"gz\":";  j += String(gz, 1);
  j += ",\"lat\":"; j += String(lastValidLat, 6);
  j += ",\"lng\":"; j += String(lastValidLng, 6);
  j += ",\"gps_valid\":";   j += gpsValid ? "true" : "false";
  j += ",\"gps_cached\":";  j += (!gpsValid && lastValidLat != 0) ? "true" : "false";
  j += ",\"gps_lost_s\":";  j += lostS;
  j += ",\"fall\":";        j += fallDetected ? "true" : "false";
  j += ",\"fall_phase\":";  j += (int)fallPhase;
  j += ",\"sos\":";         j += sosActive    ? "true" : "false";
  j += ",\"fatigue\":";     j += fatigueAlert ? "true" : "false";
  j += ",\"fat_score\":";   j += fatigueScore;
  j += ",\"fall_cnt\":";    j += fallCount;
  j += ",\"sos_cnt\":";     j += sosCount;
  j += ",\"offline_q\":";   j += offlineQueue;
  j += ",\"helmet_worn\":"; j += helmetWorn ? "true" : "false";
  j += ",\"uptime_s\":";    j += uptimeSec;
  j += ",\"log\":\"";       j += alertLog; j += "\"";
  j += ",\"zones\":";       j += zonesJSON();
  j += "}";
  server.send(200, "application/json", j);
}

void handleRoot() {
  uint32_t uptimeSec = systemActive ? (millis() - switchOnTime) / 1000 : 0;
  int h = uptimeSec / 3600;
  int m = (uptimeSec % 3600) / 60;
  int s = uptimeSec % 60;
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>Smart Helmet</title><style>";
  html += "body{font-family:sans-serif;background:#0a0e1a;color:#dce8f5;text-align:center;padding:30px;}";
  html += "h1{color:#ffab00;}.safe{color:#00e5b0;}.danger{color:#ff3d6b;}.warn{color:#ffab00;}";
  html += ".btn{display:inline-block;margin:8px;padding:12px 22px;border-radius:8px;";
  html += "background:#1a2840;color:#dce8f5;text-decoration:none;border:1px solid #1e2d47;}";
  html += ".btn.red{border-color:rgba(255,61,107,.6);color:#ff3d6b;}";
  html += ".btn.orange{border-color:rgba(255,171,0,.6);color:#ffab00;}";
  html += "p{font-size:1.1rem;margin:6px 0;}</style></head><body>";
  html += "<h1>" + String(WORKER_NAME) + " - " + String(WORKER_ID) + "</h1>";

  if (!systemActive) {
    html += "<p class='warn'>SYSTEM OFF - flip switch to start monitoring</p>";
  } else {
    html += "<p class='safe'>SYSTEM ON - monitoring active</p>";
    html += helmetWorn ? "<p class='safe'>Helmet: ON HEAD</p>" : "<p class='warn'>Helmet: NOT WORN</p>";
    html += fallDetected ? "<p class='danger'>FALL DETECTED!</p>" : "<p class='safe'>Worker Safe</p>";
    html += "<p>Fatigue Score: <b>" + String(fatigueScore) + "%</b></p>";
    html += "<p>Falls this session: <b>" + String(fallCount) + "</b></p>";
    html += "<p>Session time: <b>" + String(h) + "h " + String(m) + "m " + String(s) + "s</b></p>";
    if (gpsValid)
      html += "<p>GPS: " + String(lastValidLat, 5) + ", " + String(lastValidLng, 5) + "</p>";
    else if (lastValidLat != 0)
      html += "<p>GPS cached: " + String(lastValidLat, 5) + ", " + String(lastValidLng, 5) + "</p>";
    else
      html += "<p>Searching for GPS...</p>";
  }

  html += "<br><a class='btn red' href='/reset'>Reset Alert</a>";
  html += "<a class='btn orange' href='/clearsession'>Clear Session</a>";
  html += "<a class='btn' href='/data'>Raw JSON</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleReset() {
  fallDetected = false;
  sosActive    = false;
  fatigueAlert = false;
  fallPhase    = FP_IDLE;
  buzOn        = false;
  digitalWrite(BUZZER_PIN, LOW);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleClearSession() {
  clearSessionData();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  bootTime = millis();
  Serial.println("[BOOT] Smart Helmet ready");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("[ERROR] MPU6050 not found!");
    while (1) {
      digitalWrite(BUZZER_PIN, HIGH); delay(200);
      digitalWrite(BUZZER_PIN, LOW);  delay(200);
    }
  }
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
  Serial.println("[OK] MPU6050 ready");

  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("[OK] GPS ready");

  WiFi.softAP(SSID, PASSWORD);
  Serial.print("[OK] AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/",             handleRoot);
  server.on("/reset",        handleReset);
  server.on("/data",         handleData);
  server.on("/clearsession", handleClearSession);
  server.begin();
  Serial.println("[OK] Server ready - waiting for switch");

  if (digitalRead(SWITCH_PIN) == LOW) {
    systemStart();
  }
}

void loop() {
  server.handleClient();

  static bool lastSwitch = HIGH;
  bool curSwitch = digitalRead(SWITCH_PIN);

  if (lastSwitch == HIGH && curSwitch == LOW) {
    systemStart();
  }

  if (lastSwitch == LOW && curSwitch == HIGH) {
    systemStop();
  }

  lastSwitch = curSwitch;

  if (!systemActive) {
    delay(100);
    return;
  }

  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  ax = accel.acceleration.x;
  ay = accel.acceleration.y;
  az = accel.acceleration.z;

  gx = gyro.gyro.x * RAD_TO_DEG;
  gy = gyro.gyro.y * RAD_TO_DEG;
  gz = gyro.gyro.z * RAD_TO_DEG;

  totalAcc = sqrtf(ax*ax + ay*ay + az*az);
  tiltDeg  = degrees(acosf(constrain(az / totalAcc, -1.0f, 1.0f)));

  aiBuf[aiBufIdx] = totalAcc;
  aiBufIdx = (aiBufIdx + 1) % AI_BUF_SIZE;
  if (aiBufCount < AI_BUF_SIZE) aiBufCount++;

  detectHelmetWorn();
  detectFall();
  runAI();
  readGPS();
  controlBuzzer();

  static bool lastBtn = HIGH;
  bool curBtn = digitalRead(BUTTON_PIN);
  if (lastBtn == HIGH && curBtn == LOW) {
    sosActive = true;
    sosCount++;
    buzOn = true; buzStart = millis(); buzPattern = 1;
    addLog("SOS #" + String(sosCount));
    saveSessionData();
    Serial.println("[SOS] Manual SOS pressed!");
  }
  lastBtn = curBtn;

  static uint32_t lastDbg = 0;
  if (millis() - lastDbg > 1000) {
    Serial.printf("acc=%.2f tilt=%.0f gps=%s fat=%d%% phase=%d fall=%s worn=%s\n",
      totalAcc, tiltDeg,
      gpsValid ? "LIVE" : (lastValidLat ? "CACHED" : "SEARCH"),
      fatigueScore, (int)fallPhase,
      fallDetected ? "YES" : "no",
      helmetWorn ? "YES" : "no");
    lastDbg = millis();
  }

  delay(20);
}
