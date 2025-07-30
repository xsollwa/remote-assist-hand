# 1 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
# 2 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 2
# 3 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 2
# 4 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 2
# 5 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 2
# 6 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 2
# 7 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 2
# 8 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 2
# 9 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 2
# 10 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 2

// Wi-Fi credentials
const char* ssid = "FatCat";
const char* password = "snip1234";

// Async server & WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Pins
# 30 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
// Motion & force
const float STEPPER_MAX_SPEED = 800.0;
const float STEPPER_ACCELERATION = 200.0;
const float SERVO_MAX_ANGLE = 270.0;
const float SERVO_SMOOTHING = 0.05; // 0.0 = no smoothing, 1.0 = instant snap
const float SERVO_MOVE_SPEED = 60.0; // degrees per second when button held
float FORCE_CALIBRATION = 1.0;
const unsigned long FORCE_INTERVAL = 200;
unsigned long lastForceTime = 0;

// Stepper, servos & scale
AccelStepper baseStepper(AccelStepper::DRIVER, 14, 27);
Servo shoulder, elbow, wrist, grasper;
HX711 scale;

// current & target angles
float shoulderAngle = 90, targetShoulderAngle = 90;
float elbowAngle = 90, targetElbowAngle = 90;
float wristAngle = 90, targetWristAngle = 90;
float grasperAngle = 90, targetGrasperAngle = 90;

// servo speeds (deg/sec)
float shoulderSpeed = 0, elbowSpeed = 0, wristSpeed = 0, grasperSpeed = 0;

//––– Helper: smooth & write servo –––––––––––––––––––––––––––––––––––––––––
void updateServo(Servo &servo, float &currentAngle, float targetAngle) {
  // exponential smoothing toward target
  currentAngle += (targetAngle - currentAngle) * SERVO_SMOOTHING;
  // clamp & map to pulse width
  float ang = ((currentAngle) < (0.0) ? (0.0) : ((currentAngle) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (currentAngle)));
  servo.writeMicroseconds(map((int)ang, 0, (int)SERVO_MAX_ANGLE, 500, 2500));
}

//––– Handle move vs stop ––––––––––––––––––––––––––––––––––––––––––––––––––
void handleMotorCommand(const String& motor, const String& dir) {
  if (motor == "base_stepper") {
    if (!baseStepper.isRunning()) {
      baseStepper.setSpeed(dir == "cw" ? STEPPER_MAX_SPEED
                                       : -STEPPER_MAX_SPEED);
      // FreeRTOS task loop2 will pick it up
    }
    return;
  }
  // set servo speeds
  float v = (dir == "cw" ? +SERVO_MOVE_SPEED : -SERVO_MOVE_SPEED);
  if (motor == "shoulder_servo") shoulderSpeed = v;
  else if (motor == "elbow_servo") elbowSpeed = v;
  else if (motor == "wrist_servo") wristSpeed = v;
  else if (motor == "grasper_servo") grasperSpeed = v;
}

void handleMotorStop(const String& motor) {
  if (motor == "base_stepper") {
    baseStepper.setSpeed(0);
  }
  // zero-out servo speed
  if (motor == "shoulder_servo") shoulderSpeed = 0;
  else if (motor == "elbow_servo") elbowSpeed = 0;
  else if (motor == "wrist_servo") wristSpeed = 0;
  else if (motor == "grasper_servo") grasperSpeed = 0;
}

//––– WebSocket events –––––––––––––––––––––––––––––––––––––––––––––––––––––
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg,
                      uint8_t *data, size_t len) {
  if (type != WS_EVT_DATA) return;
  auto *info = (AwsFrameInfo*)arg;
  if (!info->final || info->opcode != WS_TEXT) return;

  String msg((char*)data, len);
  StaticJsonDocument<256> d;
  if (deserializeJson(d, msg)) return;

  String t = d["type"];
  String m = d["motor"];
  String dir = d["dir"];
  if (t == "move") handleMotorCommand(m, dir);
  else if (t == "stop") handleMotorStop(m);
}

//––– FreeRTOS task: run stepper at constant speed –––––––––––––––––––––––––
void loop2(void* pvParameters) {
  while (true) {
    baseStepper.runSpeed();
  }
}

//––– FreeRTOS task: servo velocity + smoothing at fixed rate ––––––––––––––
void servoTask(void* pvParameters) {
  const TickType_t period = ( ( TickType_t ) ( ( ( TickType_t ) ( 20 ) * ( TickType_t ) 
# 120 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 3
                           1000 
# 120 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
                           ) / ( TickType_t ) 1000U ) ); // 20 ms → 50 Hz
  TickType_t lastWake = xTaskGetTickCount();
  const float dt = 20.0f / 1000.0f; // seconds per tick

  while (true) {
    // update targets by velocity
    targetShoulderAngle = ((targetShoulderAngle + shoulderSpeed * dt) < (0.0) ? (0.0) : ((targetShoulderAngle + shoulderSpeed * dt) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetShoulderAngle + shoulderSpeed * dt)));
    targetElbowAngle = ((targetElbowAngle + elbowSpeed * dt) < (0.0) ? (0.0) : ((targetElbowAngle + elbowSpeed * dt) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetElbowAngle + elbowSpeed * dt)));
    targetWristAngle = ((targetWristAngle + wristSpeed * dt) < (0.0) ? (0.0) : ((targetWristAngle + wristSpeed * dt) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetWristAngle + wristSpeed * dt)));
    targetGrasperAngle = ((targetGrasperAngle + grasperSpeed * dt) < (0.0) ? (0.0) : ((targetGrasperAngle + grasperSpeed * dt) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetGrasperAngle + grasperSpeed * dt)));
    // apply smoothing & write pulses
    updateServo(shoulder, shoulderAngle, targetShoulderAngle);
    updateServo(elbow, elbowAngle, targetElbowAngle);
    updateServo(wrist, wristAngle, targetWristAngle);
    updateServo(grasper, grasperAngle, targetGrasperAngle);

    do { ( void ) xTaskDelayUntil( ( &lastWake ), ( period ) ); } while( 0 );
  }
}

void setup() {
  Serial0.begin(115200);
  // stepper reset + config
  pinMode(12, 0x03);
  digitalWrite(12, 0x1);
  delay(100);
  baseStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  baseStepper.setAcceleration(STEPPER_ACCELERATION);

  // attach servos
  shoulder.attach(32, 500, 2500);
  elbow.attach(33, 500, 2500);
  wrist.attach(25, 500, 2500);
  grasper.attach(26, 500, 2500);

  // Wi-Fi + WebServer + WS
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send_P(200, "text/html", MAIN_PAGE);
  });
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();

  // HX711 scale
  scale.begin(18, 19);
  scale.set_scale(1.0);
  scale.tare();

  // start FreeRTOS tasks
  xTaskCreatePinnedToCore(loop2, "stepperTask", 1000, nullptr, 0, nullptr, 0);
  xTaskCreatePinnedToCore(servoTask, "servoTask", 2048, nullptr, 1, nullptr, 1);
}

void loop() {
  // only periodic force reading + broadcast
  if (millis() - lastForceTime >= FORCE_INTERVAL) {
    lastForceTime = millis();
    float raw = scale.get_units(5);
    float force = raw * FORCE_CALIBRATION;
    StaticJsonDocument<128> doc;
    doc["type"] = "force";
    doc["motor"] = "grasper_servo";
    doc["force"] = force;
    String out; serializeJson(doc, out);
    ws.textAll(out);
  }
}
