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

// Async server setup
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Pins
# 30 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
// Motion & force
const float STEPPER_MAX_SPEED = 800.0;
const float STEPPER_ACCELERATION = 200.0;
const float SERVO_STEP_DEG = 5.0; // Increased for digital servos
const float SERVO_MAX_ANGLE = 270.0;
const float SERVO_SMOOTHING = 0.05; // smoothing factor: 0.0=no move, 1.0=instant
float FORCE_CALIBRATION = 1.0;
const unsigned long FORCE_INTERVAL = 200;
unsigned long lastForceTime = 0;

// Objects
AccelStepper baseStepper(AccelStepper::DRIVER, 14, 27);
Servo shoulder, elbow, wrist, grasper;
HX711 scale;

// current & target angles
float shoulderAngle = 90, targetShoulderAngle = 90;
float elbowAngle = 90, targetElbowAngle = 90;
float wristAngle = 90, targetWristAngle = 90;
float grasperAngle = 90, targetGrasperAngle = 90;

bool stepper_run = false;

//––– Helper: smooth & write servo ––––––––––––––––––––––––––––––––––––––––––––––
void updateServo(Servo &servo, float &currentAngle, float targetAngle) {
  // exponential smoothing toward target
  currentAngle += (targetAngle - currentAngle) * SERVO_SMOOTHING;
  // clamp & map to pulse width
  float ang = ((currentAngle) < (0.0) ? (0.0) : ((currentAngle) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (currentAngle)));
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
    targetShoulderAngle = ((targetShoulderAngle + (dir=="cw"?2:-2)) < (0.0) ? (0.0) : ((targetShoulderAngle + (dir=="cw"?2:-2)) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetShoulderAngle + (dir=="cw"?2:-2))));
  }
  else if (motor == "elbow_servo") {
    targetElbowAngle = ((targetElbowAngle + (dir=="cw"?2:-2)) < (0.0) ? (0.0) : ((targetElbowAngle + (dir=="cw"?2:-2)) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetElbowAngle + (dir=="cw"?2:-2))));
  }
  else if (motor == "wrist_servo") {
    targetWristAngle = ((targetWristAngle + (dir=="cw"?4:-4)) < (0.0) ? (0.0) : ((targetWristAngle + (dir=="cw"?4:-4)) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetWristAngle + (dir=="cw"?4:-4))));
  }
  else if (motor == "grasper_servo") {
    targetGrasperAngle = ((targetGrasperAngle + (dir=="cw"?3:-3)) < (0.0) ? (0.0) : ((targetGrasperAngle + (dir=="cw"?3:-3)) > (SERVO_MAX_ANGLE) ? (SERVO_MAX_ANGLE) : (targetGrasperAngle + (dir=="cw"?3:-3))));
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
      Serial0.println("WS Message: " + msg);

      StaticJsonDocument<256> d;
      if (deserializeJson(d, msg)) {
        Serial0.println("JSON Parse Failed");
        return;
      }
      String t = d["type"];
      String m = d["motor"];
      String dir = d["dir"];

      if (t == "move") handleMotorCommand(m, dir);
      else if (t == "stop") handleMotorStop(m);
    }
  }
}

void setup() {
  Serial0.begin(115200);

  // Stepper reset + config
  pinMode(12, 0x03);
  digitalWrite(12, 0x1);
  delay(100);
  baseStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  baseStepper.setAcceleration(STEPPER_ACCELERATION);
  baseStepper.setSpeed(0);

  // Attach servos
  shoulder.attach(32, 500, 2500);
  elbow.attach(33, 500, 2500);
  wrist.attach(25, 500, 2500);
  grasper.attach(26, 500, 2500);

  // WiFi
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial0.println("WiFi connected: " + WiFi.localIP().toString());

  // Serve page + WebSocket
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", MAIN_PAGE);
  });
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();

  // HX711
  scale.begin(18, 19);
  scale.set_scale(1.0);
  scale.tare();

  // Stepper task
  xTaskCreatePinnedToCore(
    loop2, "loop2", 1000, 
# 156 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 3 4
                         __null
# 156 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
                             , 0, 
# 156 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino" 3 4
                                  __null
# 156 "C:\\Users\\golcz\\remote-assist-hand\\Code\\REMOTE-ASSIST-HAND\\REMOTE-ASSIST-HAND.ino"
                                      , 0
  );
}

void loop() {
  //––– Smooth all servos in one go ––––––––––––––––––––––––––––––––––––––––––––
  updateServo(shoulder, shoulderAngle, targetShoulderAngle);
  updateServo(elbow, elbowAngle, targetElbowAngle);
  updateServo(wrist, wristAngle, targetWristAngle);
  updateServo(grasper, grasperAngle, targetGrasperAngle);

  //––– Periodic force reading + broadcast ––––––––––––––––––––––––––––––––––––
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

void loop2(void* pvParameters) {
  while (1) {
    baseStepper.runSpeed();
  }
}
