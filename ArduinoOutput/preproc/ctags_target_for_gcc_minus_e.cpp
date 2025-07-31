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

// Wi-Fi creds
const char* ssid = "FatCat";
const char* password = "snip1234";

// server + WS
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// pins
# 30 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
// motion params
const float STEPPER_MAX_SPEED = 800.0;
const float STEPPER_ACCELERATION = 200.0;
const float SERVO_MAX_ANGLE = 270.0;
const float SERVO_SMOOTHING = 0.05; // 0.0 = no smoothing, 1.0 = snap
const float SERVO_MOVE_SPEED = 60.0; // deg/sec when button held

// force sensor
float FORCE_CALIBRATION = 1.0;
const unsigned long FORCE_INTERVAL = 200;
unsigned long lastForceTime = 0;

// hardware objects
AccelStepper baseStepper(AccelStepper::DRIVER, 14, 27);
Servo shoulder, elbow, wrist, grasper;
HX711 scale;

// angles & targets
float shoulderAngle = 90, targetShoulderAngle = 90;
float elbowAngle = 90, targetElbowAngle = 90;
float wristAngle = 90, targetWristAngle = 90;
float grasperAngle = 90, targetGrasperAngle = 90;

// servo velocities
float shoulderSpeed = 0, elbowSpeed = 0, wristSpeed = 0, grasperSpeed = 0;

//––– smoothing + pulse write –––––––––––––––––––––––––––––––––––––––––––––
void updateServo(Servo &servo, float &cur, float tgt) {
  cur += (tgt - cur) * SERVO_SMOOTHING;
  float a = ((cur) < (0.0) ? (0.0) : ((cur) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (cur)));
  servo.writeMicroseconds(map((int)a, 0, (int)SERVO_MAX_ANGLE, 500, 2500));
}

//––– WS command handlers –––––––––––––––––––––––––––––––––––––––––––––––––
void handleMotorCommand(const String &motor, const String &dir) {
  if (motor == "base_stepper") {
    baseStepper.setSpeed(dir=="cw" ? STEPPER_MAX_SPEED
                                   : -STEPPER_MAX_SPEED);
    return;
  }
  // servo: set velocity
  float v = (dir=="cw"? +SERVO_MOVE_SPEED : -SERVO_MOVE_SPEED);
  if (motor=="shoulder_servo") shoulderSpeed = v;
  else if (motor=="elbow_servo") elbowSpeed = v;
  else if (motor=="wrist_servo") wristSpeed = v;
  else if (motor=="grasper_servo") grasperSpeed = v;
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
  String t = doc["type"],
         m = doc["motor"],
         dir = doc["dir"];
  if (t=="move") handleMotorCommand(m, dir);
  else handleMotorStop(m);
}

//––– stepper runner on core 0, low prio –––––––––––––––––––––––––––––––––––
void stepperTask(void*){
  for(;;) baseStepper.runSpeed();
}

//––– servo velocity+smoothing @50Hz on core 0, high prio –––––––––––––––––
void servoTask(void*){
  const TickType_t period = ( ( TickType_t ) ( ( ( TickType_t ) ( 20 ) * ( TickType_t ) 
# 126 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 3
                           1000 
# 126 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
                           ) / ( TickType_t ) 1000U ) );
  TickType_t lastWake = xTaskGetTickCount();
  const float dt = 20.0f/1000.0f;

  for(;;){
    // advance targets by velocity
    targetShoulderAngle = ((targetShoulderAngle + shoulderSpeed*dt) < (0.0) ? (0.0) : ((targetShoulderAngle + shoulderSpeed*dt) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetShoulderAngle + shoulderSpeed*dt)));
    targetElbowAngle = ((targetElbowAngle + elbowSpeed*dt) < (0.0) ? (0.0) : ((targetElbowAngle + elbowSpeed*dt) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetElbowAngle + elbowSpeed*dt)));
    targetWristAngle = ((targetWristAngle + wristSpeed*dt) < (0.0) ? (0.0) : ((targetWristAngle + wristSpeed*dt) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetWristAngle + wristSpeed*dt)));
    targetGrasperAngle = ((targetGrasperAngle + grasperSpeed*dt) < (0.0) ? (0.0) : ((targetGrasperAngle + grasperSpeed*dt) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetGrasperAngle + grasperSpeed*dt)));

    // apply smoothing & write
    updateServo(shoulder, shoulderAngle, targetShoulderAngle);
    updateServo(elbow, elbowAngle, targetElbowAngle);
    updateServo(wrist, wristAngle, targetWristAngle);
    updateServo(grasper, grasperAngle, targetGrasperAngle);

    do { ( void ) xTaskDelayUntil( ( &lastWake ), ( period ) ); } while( 0 );
  }
}

void setup(){
  Serial0.begin(115200);

  // stepper init
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

  // Wi-Fi + server
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status()!=WL_CONNECTED) delay(500);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send_P(200, "text/html", MAIN_PAGE);
  });
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();

  // HX711
  scale.begin(18, 19);
  scale.set_scale(1.0);
  scale.tare();

  // motor tasks on core 0
  xTaskCreatePinnedToCore(stepperTask, "stepperTask", 1000, nullptr, 0, nullptr, 0);
  xTaskCreatePinnedToCore(servoTask, "servoTask", 2048, nullptr, 1, nullptr, 0);
}

void loop(){
  // only force broadcasts
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
