#include <Arduino.h>
#line 1 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <AccelStepper.h>
#include <ESP32Servo.h>
#include <ESP32PWM.h>
#include <Preferences.h>
#include "HX711.h"
#include "main_page.h"

// Wi-Fi creds
const char* ssid     = "FatCat";
const char* password = "snip1234";

// server + WS
AsyncWebServer server(80);
AsyncWebSocket  ws("/ws");

// pins
#define STEPPER_STEP_PIN 14
#define STEPPER_DIR_PIN  27
#define STEPPER_RST_PIN  12
#define SHOULDER_PIN     32
#define ELBOW_PIN        33
#define WRIST_PIN        25
#define GRASPER_PIN      26
#define HX711_DT_PIN     18
#define HX711_SCK_PIN    19

// motion params
const float STEPPER_MAX_SPEED    = 800.0;
const float STEPPER_ACCELERATION = 200.0;
const float SERVO_SMOOTHING      = 0.05;   // 0=no filter, 1=instant
const float SERVO_MOVE_SPEED     = 60.0;   // deg/sec when held

// safe motion limits (degrees)
const float SHOULDER_MIN_ANG = 30.0;
const float SHOULDER_MAX_ANG = 150.0;
const float ELBOW_MIN_ANG    = 0.0;
const float ELBOW_MAX_ANG    = 135.0;
const float WRIST_MIN_ANG    = 10.0;
const float WRIST_MAX_ANG    = 160.0;
const float GRASPER_MIN_ANG  = 20.0;
const float GRASPER_MAX_ANG  = 100.0;

// force sensor
float       FORCE_CALIBRATION    = 1.0;
const unsigned long FORCE_INTERVAL = 200;

// drift & filter state
static float lastForce       = 0.0;
static long  runningOffset   = 0;
static bool  offsetInited    = false;
unsigned long lastDriftTime  = 0;

// hardware
AccelStepper baseStepper(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN);
Servo        shoulder, elbow, wrist, grasper;
HX711        scale;
Preferences  prefs;

// angles & targets
float shoulderAngle, targetShoulderAngle;
float elbowAngle,    targetElbowAngle;
float wristAngle,    targetWristAngle;
float grasperAngle,  targetGrasperAngle;

// velocities (deg/sec)
float shoulderSpeed = 0, elbowSpeed = 0, wristSpeed = 0, grasperSpeed = 0;

//––– smoothing + pulse write with per-servo limits –––––––––––––––––––––––––
#line 73 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void updateServo(Servo &servo, float &cur, float tgt, float minA, float maxA);
#line 81 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void handleMotorCommand(const String &motor, const String &dir);
#line 93 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void handleMotorStop(const String &motor);
#line 117 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void onWebSocketEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len);
#line 139 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void stepperTask(void*);
#line 144 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void servoTask(void*);
#line 168 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void updateOffset();
#line 174 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
float getFilteredForce();
#line 183 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void forceTask(void*);
#line 217 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void setup();
#line 281 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void loop();
#line 73 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void updateServo(Servo &servo, float &cur, float tgt, float minA, float maxA) {
  cur += (tgt - cur) * SERVO_SMOOTHING;
  float a = constrain(cur, minA, maxA);
  int us = map((int)a, (int)minA, (int)maxA, 500, 2500);
  servo.writeMicroseconds(us);
}

//––– WebSocket command handlers –––––––––––––––––––––––––––––––––––––––––
void handleMotorCommand(const String &motor, const String &dir) {
  if (motor == "base_stepper") {
    baseStepper.setSpeed(dir=="cw"? +STEPPER_MAX_SPEED : -STEPPER_MAX_SPEED);
    return;
  }
  float v = (dir=="cw"? +SERVO_MOVE_SPEED : -SERVO_MOVE_SPEED);
  if      (motor=="shoulder_servo") shoulderSpeed = v;
  else if (motor=="elbow_servo")    elbowSpeed    = v;
  else if (motor=="wrist_servo")    wristSpeed    = v;
  else if (motor=="grasper_servo")  grasperSpeed  = v;
}

void handleMotorStop(const String &motor) {
  if (motor == "base_stepper") {
    baseStepper.setSpeed(0);
    return;
  }
  if (motor=="shoulder_servo") {
    shoulderSpeed = 0;
    targetShoulderAngle = shoulderAngle;
    prefs.putFloat("shAng", shoulderAngle);
  } else if (motor=="elbow_servo") {
    elbowSpeed = 0;
    targetElbowAngle = elbowAngle;
    prefs.putFloat("elAng", elbowAngle);
  } else if (motor=="wrist_servo") {
    wristSpeed = 0;
    targetWristAngle = wristAngle;
    prefs.putFloat("wrAng", wristAngle);
  } else if (motor=="grasper_servo") {
    grasperSpeed = 0;
    targetGrasperAngle = grasperAngle;
    prefs.putFloat("grAng", grasperAngle);
  }
}

void onWebSocketEvent(AsyncWebSocket *s, AsyncWebSocketClient *c,
                      AwsEventType type, void *arg,
                      uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client %u connected\n", c->id());
  }
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client %u disconnected\n", c->id());
  }
  else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (!info->final || info->opcode!=WS_TEXT) return;
    String msg((char*)data, len);
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg)) return;
    String t = doc["type"], m = doc["motor"], dir = doc["dir"];
    if (t=="move")      handleMotorCommand(m, dir);
    else                  handleMotorStop(m);
  }
}

//––– stepper runner on core 0 –––––––––––––––––––––––––––––––––––––––
void stepperTask(void*) {
  for(;;) baseStepper.runSpeed();
}

//––– servo velocity + smoothing @50Hz on core 0 –––––––––––––––––––
void servoTask(void*) {
  const TickType_t period = pdMS_TO_TICKS(20);
  TickType_t lastWake = xTaskGetTickCount();
  const float dt = 20.0f/1000.0f;
  for (;;) {
    targetShoulderAngle = constrain(targetShoulderAngle + shoulderSpeed*dt,
                                    SHOULDER_MIN_ANG, SHOULDER_MAX_ANG);
    targetElbowAngle    = constrain(targetElbowAngle    + elbowSpeed*dt,
                                    ELBOW_MIN_ANG,    ELBOW_MAX_ANG);
    targetWristAngle    = constrain(targetWristAngle    + wristSpeed*dt,
                                    WRIST_MIN_ANG,    WRIST_MAX_ANG);
    targetGrasperAngle  = constrain(targetGrasperAngle  + grasperSpeed*dt,
                                    GRASPER_MIN_ANG,  GRASPER_MAX_ANG);

    updateServo(shoulder, shoulderAngle,       targetShoulderAngle, SHOULDER_MIN_ANG, SHOULDER_MAX_ANG);
    updateServo(elbow,    elbowAngle,          targetElbowAngle,    ELBOW_MIN_ANG,    ELBOW_MAX_ANG);
    updateServo(wrist,    wristAngle,          targetWristAngle,    WRIST_MIN_ANG,    WRIST_MAX_ANG);
    updateServo(grasper,  grasperAngle,        targetGrasperAngle,  GRASPER_MIN_ANG,  GRASPER_MAX_ANG);

    vTaskDelayUntil(&lastWake, period);
  }
}

//––– HX711 helpers –––––––––––––––––––––––––––––––––––––––––––––––––
void updateOffset(){
  long raw = scale.read_average(5);
  runningOffset = runningOffset*(1 - 0.01) + raw*0.01;
  scale.set_offset(runningOffset);
}

float getFilteredForce(){
  float units = scale.get_units(10);
  float filt  = lastForce*(1 - 0.2f) + units*0.2f;
  lastForce    = filt;
  if (fabs(filt) < 0.05f) filt = 0.0f;
  return filt * FORCE_CALIBRATION;
}

//––– force-streaming on core 1 –––––––––––––––––––––––––––––––––
void forceTask(void*) {
  const TickType_t period = pdMS_TO_TICKS(FORCE_INTERVAL);
  TickType_t lastWake = xTaskGetTickCount();
  int aliveCounter = 0;

  for (;;) {
    if (millis() - lastDriftTime > 5000) {
      updateOffset();
      lastDriftTime = millis();
    }

    float force = getFilteredForce();
    StaticJsonDocument<128> doc;
    doc["type"]  = "force";
    doc["motor"] = "grasper_servo";
    doc["force"] = force;
    String out; serializeJson(doc, out);
    ws.textAll(out);

    if (++aliveCounter >= 50) {
      StaticJsonDocument<64> hb;
      hb["type"]  = "heartbeat";
      hb["force"] = force;
      String j;
      serializeJson(hb, j);  // ← fixed: doc then dest
      ws.textAll(j);
      ws.pingAll();
      aliveCounter = 0;
    }

    vTaskDelayUntil(&lastWake, period);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(STEPPER_RST_PIN, OUTPUT);
  digitalWrite(STEPPER_RST_PIN, HIGH);
  delay(100);

  baseStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  baseStepper.setAcceleration(STEPPER_ACCELERATION);

  shoulder.attach(SHOULDER_PIN, 500, 2500);
  elbow.attach(ELBOW_PIN,       500, 2500);
  wrist.attach(WRIST_PIN,       500, 2500);
  grasper.attach(GRASPER_PIN,   500, 2500);

  prefs.begin("servo", false);
  // load last or default to midpoint
  targetShoulderAngle = prefs.getFloat("shAng", (SHOULDER_MIN_ANG+SHOULDER_MAX_ANG)/2);
  shoulderAngle       = targetShoulderAngle;
  shoulder.writeMicroseconds(map((int)shoulderAngle, SHOULDER_MIN_ANG, SHOULDER_MAX_ANG, 500, 2500));

  targetElbowAngle = prefs.getFloat("elAng", (ELBOW_MIN_ANG+ELBOW_MAX_ANG)/2);
  elbowAngle       = targetElbowAngle;
  elbow.writeMicroseconds(map((int)elbowAngle, ELBOW_MIN_ANG, ELBOW_MAX_ANG, 500, 2500));

  targetWristAngle = prefs.getFloat("wrAng", (WRIST_MIN_ANG+WRIST_MAX_ANG)/2);
  wristAngle       = targetWristAngle;
  wrist.writeMicroseconds(map((int)wristAngle, WRIST_MIN_ANG, WRIST_MAX_ANG, 500, 2500));

  targetGrasperAngle = prefs.getFloat("grAng", (GRASPER_MIN_ANG+GRASPER_MAX_ANG)/2);
  grasperAngle       = targetGrasperAngle;
  grasper.writeMicroseconds(map((int)grasperAngle, GRASPER_MIN_ANG, GRASPER_MAX_ANG, 500, 2500));

  // Wi-Fi + server
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send_P(200, "text/html", MAIN_PAGE);
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *r){ r->send(204); });
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();

  // HX711 startup tare
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(FORCE_CALIBRATION);
  delay(500);
  long sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += scale.read();
    delay(20);
  }
  runningOffset  = sum / 50;
  scale.set_offset(runningOffset);
  lastDriftTime  = millis();

  // tasks
  xTaskCreatePinnedToCore(stepperTask, "stepper", 1000, nullptr, 0, nullptr, 0);
  xTaskCreatePinnedToCore(servoTask,   "servo",   2048, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(forceTask,   "force",   2048, nullptr, 1, nullptr, 1);
}

void loop() {
  delay(1000);
}

