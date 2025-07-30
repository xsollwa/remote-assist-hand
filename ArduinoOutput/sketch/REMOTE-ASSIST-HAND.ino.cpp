#include <Arduino.h>
#line 1 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <AccelStepper.h>
#include <ESP32Servo.h>
#include <ESP32PWM.h>
#include "HX711.h"
#include "main_page.h"

// Wi-Fi credentials
const char* ssid     = "FatCat";
const char* password = "snip1234";

// Async server setup
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Pins
#define STEPPER_STEP_PIN 14
#define STEPPER_DIR_PIN  27
#define STEPPER_RST_PIN  12
#define SHOULDER_PIN     32
#define ELBOW_PIN        33
#define WRIST_PIN        25
#define GRASPER_PIN      26
#define HX711_DT_PIN     18
#define HX711_SCK_PIN    19

// Motion & force
const float STEPPER_MAX_SPEED    = 800.0;
const float STEPPER_ACCELERATION = 200.0;
const float SERVO_STEP_DEG       = 5.0;    // Increased for digital servos
const float SERVO_MAX_ANGLE      = 270.0;
const float SERVO_SMOOTHING      = 0.05;   // smoothing factor: 0.0=no move, 1.0=instant
float       FORCE_CALIBRATION    = 1.0;
const unsigned long FORCE_INTERVAL = 200;
unsigned long lastForceTime = 0;

// Objects
AccelStepper baseStepper(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN);
Servo shoulder, elbow, wrist, grasper;
HX711 scale;

// current & target angles
float shoulderAngle = 90,    targetShoulderAngle = 90;
float elbowAngle    = 90,    targetElbowAngle    = 90;
float wristAngle    = 90,    targetWristAngle    = 90;
float grasperAngle  = 90,    targetGrasperAngle  = 90;

bool stepper_run = false;

//––– Helper: smooth & write servo ––––––––––––––––––––––––––––––––––––––––––––––
#line 54 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void updateServo(Servo &servo, float &currentAngle, float targetAngle);
#line 63 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void handleMotorCommand(const String& motor, const String& dir);
#line 87 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void handleMotorStop(const String& motor);
#line 95 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
#line 118 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void setup();
#line 160 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void loop();
#line 181 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void loop2(void* pvParameters);
#line 54 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
void updateServo(Servo &servo, float &currentAngle, float targetAngle) {
  // exponential smoothing toward target
  currentAngle += (targetAngle - currentAngle) * SERVO_SMOOTHING;
  // clamp & map to pulse width
  float ang = constrain(currentAngle, 0.0, SERVO_MAX_ANGLE);
  servo.writeMicroseconds(map((int)ang, 0, (int)SERVO_MAX_ANGLE, 500, 2500));
}

//––– Handle incoming move/stop commands ––––––––––––––––––––––––––––––––––––––
void handleMotorCommand(const String& motor, const String& dir) {
  if (motor == "base_stepper") {
    if (!stepper_run) {
      baseStepper.setSpeed(dir == "cw" ? STEPPER_MAX_SPEED : -STEPPER_MAX_SPEED);
      stepper_run = true;
    }
    return;
  }

  // bump targets & clamp immediately
  if (motor == "shoulder_servo") {
    targetShoulderAngle = constrain(targetShoulderAngle + (dir=="cw"?2:-2), 0.0, SERVO_MAX_ANGLE);
  }
  else if (motor == "elbow_servo") {
    targetElbowAngle = constrain(targetElbowAngle + (dir=="cw"?2:-2), 0.0, SERVO_MAX_ANGLE);
  }
  else if (motor == "wrist_servo") {
    targetWristAngle = constrain(targetWristAngle + (dir=="cw"?4:-4), 0.0, SERVO_MAX_ANGLE);
  }
  else if (motor == "grasper_servo") {
    targetGrasperAngle = constrain(targetGrasperAngle + (dir=="cw"?3:-3), 0.0, SERVO_MAX_ANGLE);
  }
}

void handleMotorStop(const String& motor) {
  if (motor == "base_stepper") {
    baseStepper.setSpeed(0);
    stepper_run = false;
  }
  // servos just hold their last position
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      String msg = String((char*)data);
      Serial.println("WS Message: " + msg);

      StaticJsonDocument<256> d;
      if (deserializeJson(d, msg)) {
        Serial.println("JSON Parse Failed");
        return;
      }
      String t   = d["type"];
      String m   = d["motor"];
      String dir = d["dir"];

      if (t == "move")  handleMotorCommand(m, dir);
      else if (t == "stop") handleMotorStop(m);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Stepper reset + config
  pinMode(STEPPER_RST_PIN, OUTPUT);
  digitalWrite(STEPPER_RST_PIN, HIGH);
  delay(100);
  baseStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  baseStepper.setAcceleration(STEPPER_ACCELERATION);
  baseStepper.setSpeed(0);

  // Attach servos
  shoulder.attach(SHOULDER_PIN, 500, 2500);
  elbow.attach(ELBOW_PIN,     500, 2500);
  wrist.attach(WRIST_PIN,     500, 2500);
  grasper.attach(GRASPER_PIN, 500, 2500);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("WiFi connected: " + WiFi.localIP().toString());

  // Serve page + WebSocket
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", MAIN_PAGE);
  });
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();

  // HX711
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(1.0);
  scale.tare();

  // Stepper task
  xTaskCreatePinnedToCore(
    loop2, "loop2", 1000, NULL, 0, NULL, 0
  );
}

void loop() {
  //––– Smooth all servos in one go ––––––––––––––––––––––––––––––––––––––––––––
  updateServo(shoulder, shoulderAngle, targetShoulderAngle);
  updateServo(elbow,    elbowAngle,    targetElbowAngle);
  updateServo(wrist,    wristAngle,    targetWristAngle);
  updateServo(grasper,  grasperAngle,  targetGrasperAngle);

  //––– Periodic force reading + broadcast ––––––––––––––––––––––––––––––––––––
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

void loop2(void* pvParameters) {
  while (1) {
    baseStepper.runSpeed();
  }
}

