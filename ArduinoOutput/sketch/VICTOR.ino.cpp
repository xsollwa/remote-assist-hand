#include <Arduino.h>
#line 1 "C:\\Hapatic_Feedback\\remote-assist-hand\\Code\\VICTOR\\VICTOR.ino"
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
const float SERVO_STEP_DEG       = 5.0;  // Increased for digital servos
const float SERVO_MAX_ANGLE      = 270.0;
float       FORCE_CALIBRATION    = 1.0;
const unsigned long FORCE_INTERVAL = 500;
unsigned long lastForceTime = 0;

// Objects
AccelStepper baseStepper(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN);
Servo shoulder, elbow, wrist, grasper;
HX711 scale;

float shoulderAngle = 90;
float elbowAngle    = 90;
float wristAngle    = 90;
float grasperAngle  = 90;

float targetShoulderAngle = 90;
float targetElbowAngle    = 90;
float targetWristAngle    = 90;
float targetGrasperAngle  = 90;

bool stepper_run = false;

#line 56 "C:\\Hapatic_Feedback\\remote-assist-hand\\Code\\VICTOR\\VICTOR.ino"
void handleMotorCommand(const String& motor, const String& dir);
#line 84 "C:\\Hapatic_Feedback\\remote-assist-hand\\Code\\VICTOR\\VICTOR.ino"
void handleMotorStop(const String& motor);
#line 92 "C:\\Hapatic_Feedback\\remote-assist-hand\\Code\\VICTOR\\VICTOR.ino"
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
#line 120 "C:\\Hapatic_Feedback\\remote-assist-hand\\Code\\VICTOR\\VICTOR.ino"
void setup();
#line 172 "C:\\Hapatic_Feedback\\remote-assist-hand\\Code\\VICTOR\\VICTOR.ino"
void loop();
#line 202 "C:\\Hapatic_Feedback\\remote-assist-hand\\Code\\VICTOR\\VICTOR.ino"
void loop2(void* pvParameters);
#line 56 "C:\\Hapatic_Feedback\\remote-assist-hand\\Code\\VICTOR\\VICTOR.ino"
void handleMotorCommand(const String& motor, const String& dir) {
  if (motor == "base_stepper") {
    if (!stepper_run){
      baseStepper.setSpeed(dir == "cw" ? STEPPER_MAX_SPEED : -STEPPER_MAX_SPEED);
      stepper_run = true;
    }
  } else {

    if (motor == "shoulder_servo") { 
      if (dir == "cw"){targetShoulderAngle = targetShoulderAngle + 1;}
      else{targetShoulderAngle = targetShoulderAngle - 1;}
    }
    else if (motor == "elbow_servo")    { 
      if (dir == "cw"){targetElbowAngle = targetElbowAngle + 2;}
      else{targetElbowAngle = targetElbowAngle - 2;}
    }
    else if (motor == "wrist_servo")    { 
      if (dir == "cw"){targetWristAngle = targetWristAngle + 4;}
      else{targetWristAngle = targetWristAngle - 4;}
    }
    else if (motor == "grasper_servo")  { 
      if (dir == "cw"){targetGrasperAngle = targetGrasperAngle + 3;}
      else{targetGrasperAngle = targetGrasperAngle - 3;}
    }
    else return;
  }
}

void handleMotorStop(const String& motor) {
  if (motor == "base_stepper") {
    baseStepper.setSpeed(0);
    stepper_run = false;
  }
  // Servo stays at current angle
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      String msg = String((char*)data);
      Serial.println("WS Message: " + msg);

      StaticJsonDocument<256> d;
      DeserializationError error = deserializeJson(d, msg);
      if (error) {
        Serial.println("JSON Parse Failed");
        return;
      }

      String t   = d["type"];
      String m   = d["motor"];
      String dir = d["dir"];

      if (t == "move") {
        handleMotorCommand(m, dir);
      } else if (t == "stop") {
        handleMotorStop(m);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Stepper reset
  pinMode(STEPPER_RST_PIN, OUTPUT);
  digitalWrite(STEPPER_RST_PIN, HIGH);
  delay(100);
  baseStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  baseStepper.setAcceleration(STEPPER_ACCELERATION);
  baseStepper.setSpeed(0);  // Don't move until user input

  // Servos
  shoulder.attach(SHOULDER_PIN, 500, 2500);
  elbow.attach(ELBOW_PIN, 500, 2500);
  wrist.attach(WRIST_PIN, 500, 2500);
  grasper.attach(GRASPER_PIN, 500, 2500);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  // Serve Web Page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", MAIN_PAGE);
  });

  // WebSocket Setup
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // HX711 init
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(1.0);
  scale.tare();

  xTaskCreatePinnedToCore (
    loop2,     // Function to implement the task
    "loop2",   // Name of the task
    1000,      // Stack size in bytes
    NULL,      // Task input parameter
    0,         // Priority of the task
    NULL,      // Task handle.
    0          // Core where the task should run
  );

  server.begin();
}

void loop() {

  float ang;
  shoulderAngle = shoulderAngle - (shoulderAngle - targetShoulderAngle) * 0.05;
  ang = constrain(shoulderAngle, 0, SERVO_MAX_ANGLE);
  shoulder.writeMicroseconds(map((int)ang, 0, (int)SERVO_MAX_ANGLE, 500, 2500));
  elbowAngle = elbowAngle - (elbowAngle - targetElbowAngle) * 0.05;
  ang = constrain(elbowAngle, 0, SERVO_MAX_ANGLE);
  elbow.writeMicroseconds(map((int)ang, 0, (int)SERVO_MAX_ANGLE, 500, 2500));
  wristAngle = wristAngle - (wristAngle - targetWristAngle) * 0.05;
  ang = constrain(wristAngle, 0, SERVO_MAX_ANGLE);
  wrist.writeMicroseconds(map((int)ang, 0, (int)SERVO_MAX_ANGLE, 500, 2500));
  grasperAngle = grasperAngle - (grasperAngle - targetGrasperAngle) * 0.05;
  ang = constrain(grasperAngle, 0, SERVO_MAX_ANGLE);
  grasper.writeMicroseconds(map((int)ang, 0, (int)SERVO_MAX_ANGLE, 500, 2500)); 

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

void loop2 (void* pvParameters) {
  while (1) {
    baseStepper.runSpeed();
  }
}

