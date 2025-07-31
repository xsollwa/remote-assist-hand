#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <AccelStepper.h>
#include <ESP32Servo.h>
#include <ESP32PWM.h>
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
const float SERVO_MAX_ANGLE      = 270.0;
const float SERVO_SMOOTHING      = 0.05;   // 0.0 = no smoothing, 1.0 = snap
const float SERVO_MOVE_SPEED     = 60.0;   // deg/sec when button held

// force sensor
float       FORCE_CALIBRATION    = 1.0;
const unsigned long FORCE_INTERVAL = 200;
unsigned long lastForceTime = 0;

// hardware objects
AccelStepper baseStepper(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN);
Servo        shoulder, elbow, wrist, grasper;
HX711        scale;

// angles & targets
float shoulderAngle       = 90, targetShoulderAngle = 90;
float elbowAngle          = 90, targetElbowAngle    = 90;
float wristAngle          = 90, targetWristAngle    = 90;
float grasperAngle        = 90, targetGrasperAngle  = 90;

// servo velocities
float shoulderSpeed = 0, elbowSpeed = 0, wristSpeed = 0, grasperSpeed = 0;

//––– smoothing + pulse write –––––––––––––––––––––––––––––––––––––––––––––
void updateServo(Servo &servo, float &cur, float tgt) {
  cur += (tgt - cur) * SERVO_SMOOTHING;
  float a = constrain(cur, 0.0, SERVO_MAX_ANGLE);
  servo.writeMicroseconds(map((int)a, 0, (int)SERVO_MAX_ANGLE, 500, 2500));
}

//––– WS command handlers –––––––––––––––––––––––––––––––––––––––––––––––––
void handleMotorCommand(const String &motor, const String &dir) {
  if (motor == "base_stepper") {
    baseStepper.setSpeed(dir=="cw" ?  STEPPER_MAX_SPEED
                                   : -STEPPER_MAX_SPEED);
    return;
  }
  // servo: set velocity
  float v = (dir=="cw"? +SERVO_MOVE_SPEED : -SERVO_MOVE_SPEED);
  if (motor=="shoulder_servo") shoulderSpeed = v;
  else if (motor=="elbow_servo")    elbowSpeed    = v;
  else if (motor=="wrist_servo")    wristSpeed    = v;
  else if (motor=="grasper_servo")  grasperSpeed  = v;
}

void handleMotorStop(const String &motor) {
  if (motor == "base_stepper") {
    baseStepper.setSpeed(0);
    return;
  }
  // zero velocity + freeze target
  if (motor=="shoulder_servo") {
    shoulderSpeed = 0;
    targetShoulderAngle = shoulderAngle;
  }
  else if (motor=="elbow_servo") {
    elbowSpeed = 0;
    targetElbowAngle = elbowAngle;
  }
  else if (motor=="wrist_servo") {
    wristSpeed = 0;
    targetWristAngle = wristAngle;
  }
  else if (motor=="grasper_servo") {
    grasperSpeed = 0;
    targetGrasperAngle = grasperAngle;
  }
}

void onWebSocketEvent(AsyncWebSocket *s, AsyncWebSocketClient *c,
                      AwsEventType type, void *arg,
                      uint8_t *data, size_t len) {
  if (type!=WS_EVT_DATA) return;
  auto *info = (AwsFrameInfo*)arg;
  if (!info->final || info->opcode!=WS_TEXT) return;

  String msg((char*)data, len);
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg)) return;
  String t   = doc["type"],
         m   = doc["motor"],
         dir = doc["dir"];
  if (t=="move") handleMotorCommand(m, dir);
  else          handleMotorStop(m);
}

//––– stepper runner on core 0, low prio –––––––––––––––––––––––––––––––––––
void stepperTask(void*){
  for(;;) baseStepper.runSpeed();
}

//––– servo velocity+smoothing @50Hz on core 0, high prio –––––––––––––––––
void servoTask(void*){
  const TickType_t period = pdMS_TO_TICKS(20);
  TickType_t lastWake = xTaskGetTickCount();
  const float dt = 20.0f/1000.0f;

  for(;;){
    // advance targets by velocity
    targetShoulderAngle = constrain(targetShoulderAngle + shoulderSpeed*dt, 0.0, SERVO_MAX_ANGLE);
    targetElbowAngle    = constrain(targetElbowAngle    + elbowSpeed*dt,    0.0, SERVO_MAX_ANGLE);
    targetWristAngle    = constrain(targetWristAngle    + wristSpeed*dt,    0.0, SERVO_MAX_ANGLE);
    targetGrasperAngle  = constrain(targetGrasperAngle  + grasperSpeed*dt,  0.0, SERVO_MAX_ANGLE);

    // apply smoothing & write
    updateServo(shoulder, shoulderAngle,       targetShoulderAngle);
    updateServo(elbow,    elbowAngle,          targetElbowAngle);
    updateServo(wrist,    wristAngle,          targetWristAngle);
    updateServo(grasper,  grasperAngle,        targetGrasperAngle);

    vTaskDelayUntil(&lastWake, period);
  }
}

void setup(){
  Serial.begin(115200);

  // stepper init
  pinMode(STEPPER_RST_PIN, OUTPUT);
  digitalWrite(STEPPER_RST_PIN, HIGH);
  delay(100);
  baseStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  baseStepper.setAcceleration(STEPPER_ACCELERATION);

  // attach servos
  shoulder.attach(SHOULDER_PIN, 500, 2500);
  elbow.attach(ELBOW_PIN,       500, 2500);
  wrist.attach(WRIST_PIN,       500, 2500);
  grasper.attach(GRASPER_PIN,   500, 2500);

  // Wi-Fi + server
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status()!=WL_CONNECTED) delay(500);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send_P(200, "text/html", MAIN_PAGE);
  });
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();

  // HX711
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(1.0);
  scale.tare();

  // motor tasks on core 0
  xTaskCreatePinnedToCore(stepperTask, "stepperTask", 1000, nullptr, 0, nullptr, 0);
  xTaskCreatePinnedToCore(servoTask,   "servoTask",   2048, nullptr, 1, nullptr, 0);
}

void loop(){
  // only force broadcasts
  if (millis() - lastForceTime >= FORCE_INTERVAL) {
    lastForceTime = millis();
    float raw   = scale.get_units(5);
    float force = raw * FORCE_CALIBRATION;
    StaticJsonDocument<128> doc;
    doc["type"]  = "force";
    doc["motor"] = "grasper_servo";
    doc["force"] = force;
    String out; serializeJson(doc, out);
    ws.textAll(out);
  }
}
