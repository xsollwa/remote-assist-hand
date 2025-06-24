#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <AccelStepper.h>
#include <ESP32Servo.h>
#include "HX711.h"

#include "main_page.h"       


// Wi-Fi
const char* ssid     = "MY_SSID";
const char* password = "MY_PASSWORD";

// Servers
WebServer        httpServer(80);
WebSocketsServer webSocket(81);

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
const float STEPPER_ACCELERATION = 400.0;
const float SERVO_STEP_DEG       = 2.0;
const float SERVO_MAX_ANGLE      = 270.0;
float       FORCE_CALIBRATION    = 1.0;
const unsigned long FORCE_INTERVAL = 200;
unsigned long lastForceTime = 0;

// Objects
AccelStepper baseStepper(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN);
Servo       shoulder, elbow, wrist, grasper;
HX711       scale;

float shoulderAngle = SERVO_MAX_ANGLE/2;
float elbowAngle    = SERVO_MAX_ANGLE/2;
float wristAngle    = SERVO_MAX_ANGLE/2;
float grasperAngle  = SERVO_MAX_ANGLE/2;

void setup() {
  Serial.begin(115200);

  // Stepper reset
  pinMode(STEPPER_RST_PIN, OUTPUT);
  digitalWrite(STEPPER_RST_PIN, HIGH);
  baseStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  baseStepper.setAcceleration(STEPPER_ACCELERATION);

  // Servos
  shoulder.attach(SHOULDER_PIN, 500, 2500);
  elbow   .attach(ELBOW_PIN,    500, 2500);
  wrist   .attach(WRIST_PIN,    500, 2500);
  grasper .attach(GRASPER_PIN,  500, 2500);
  auto initS = [&](Servo &s, float &ang){
    int p = map((int)ang, 0, (int)SERVO_MAX_ANGLE, 500, 2500);
    s.writeMicroseconds(p);
  };
  initS(shoulder, shoulderAngle);
  initS(elbow,    elbowAngle);
  initS(wrist,    wristAngle);
  initS(grasper,  grasperAngle);

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  // HTTP server → serves MAIN_PAGE at “/”
  httpServer.on("/", HTTP_GET, [](){
    httpServer.send_P(200, "text/html", MAIN_PAGE);
  });
  httpServer.begin();

  // WebSocket server → port 81
  webSocket.begin();
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t len){
    if (type!=WStype_TEXT) return;
    DynamicJsonDocument d(256);
    if (deserializeJson(d, payload, len)) return;
    String t   = d["type"],
           m   = d["motor"],
           dir = d["dir"];
    if (t=="move") {
      if (m=="base_stepper") {
        baseStepper.setSpeed(dir=="cw"? STEPPER_MAX_SPEED : -STEPPER_MAX_SPEED);
      } else {
        Servo* sv; float* ang;
        if      (m=="shoulder_servo"){ sv=&shoulder; ang=&shoulderAngle;}
        else if (m=="elbow_servo")   { sv=&elbow;    ang=&elbowAngle;   }
        else if (m=="wrist_servo")   { sv=&wrist;    ang=&wristAngle;   }
        else if (m=="grasper_servo") { sv=&grasper;  ang=&grasperAngle; }
        else return;
        *ang = constrain(*ang + (dir=="cw"? SERVO_STEP_DEG : -SERVO_STEP_DEG),
                        0, SERVO_MAX_ANGLE);
        sv->writeMicroseconds(map((int)*ang, 0, (int)SERVO_MAX_ANGLE, 500, 2500));
      }
    } else if (t=="stop" && m=="base_stepper") {
      baseStepper.setSpeed(0);
    }
  });

  // HX711 init
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(1.0);
  scale.tare();
}

void loop() {
  httpServer.handleClient();
  webSocket.loop();
  baseStepper.run();

  if (millis() - lastForceTime >= FORCE_INTERVAL) {
    lastForceTime = millis();
    float raw   = scale.get_units(5);
    float force = raw * FORCE_CALIBRATION;
    DynamicJsonDocument doc(128);
    doc["type"]  = "force";
    doc["motor"] = "grasper_servo";
    doc["force"] = force;
    String out; serializeJson(doc, out);
    webSocket.broadcastTXT(out);
  }
}
