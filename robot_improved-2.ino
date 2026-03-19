/*
 * =====================================================================
 *  Line-Following Robot — Improved Firmware (with live AUTO tuning)
 *  Target: Adafruit Feather ESP32-S3
 *
 *  Added in this version:
 *   - AUTO_BASE and AUTO_TURN_SOFT are runtime tunable (D-pad while holding B)
 *   - A button: "trailer cross" assist -> force forward for a few seconds
 *       * works in MANUAL or AUTO
 *       * requires B held (safety)
 *       * ignores sensors and sticks during the assist window
 *       * AUTO_REVERSE stays true for normal auto following, but assist drives
 *         FORWARD regardless (so the trailer actually crosses)
 * =====================================================================
 */

#include <Bluepad32.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>
#include "telemetry_page.h"

// =====================================================================
//  COMPILE-TIME CONFIGURATION
// =====================================================================

// --- Wi-Fi AP ---
static constexpr const char* AP_SSID = "RobotTelemetry";
static constexpr const char* AP_PASS = "robot1234";

// --- Motor pins & channels ---
static constexpr int M1A = 33, M1B = 38;
static constexpr int M2A = 9,  M2B = 8;
static constexpr bool M1_REVERSED = false;
static constexpr bool M2_REVERSED = false;
static constexpr bool SWAP_SIDES  = true;

static constexpr int PWM_FREQ = 20000;
static constexpr int PWM_RES  = 10;          // 0..1023
static constexpr int CH_M1A = 0, CH_M1B = 1;
static constexpr int CH_M2A = 2, CH_M2B = 3;

// --- Ramp ---
static constexpr int RAMP_RATE_PER_SEC = 5000;

// --- IR sensors ---
static constexpr int IR_LEFT   = 10;
static constexpr int IR_CENTER = 5;
static constexpr int IR_RIGHT  = 11;
static constexpr bool IR_ACTIVE_LOW = false;
static constexpr int IR_VOTE_N = 3;

// --- Encoders ---
static constexpr int ENC_L_A = 6,  ENC_L_B = 12;
static constexpr int ENC_R_A = 14, ENC_R_B = 18;
static constexpr bool INVERT_RIGHT_ENCODER = true;
static constexpr float CPR_4X = 3760.0f;

// RPM smoothing
static constexpr uint32_t SPEED_INTERVAL_MS = 250;
static constexpr float    RPM_ALPHA         = 0.20f;

// Wheel geometry (mm)
static constexpr float WHEEL_DIAMETER_MM  = 65.0f;
static constexpr float WHEEL_TRACK_MM     = 140.0f;

// --- Servo ---
static constexpr int  SERVO_PIN        = 1;
static constexpr bool SERVO_INVERT     = true;
static constexpr int  SERVO_MIN_DEG    = 165;
static constexpr int  SERVO_MAX_DEG    = 190;
static constexpr float SERVO_SPEED_DPS = 120.0f;

// --- Manual drive ---
static constexpr int DB_STICK = 120;
static constexpr int DB_TRIG  = 40;
static constexpr int SLOW_FWD_MAX  = 500;
static constexpr int FAST_FWD_MAX  = 1023;
static constexpr int FAST_TURN_MAX = 1023;
static constexpr int SLOW_SPIN_MAX = 500;
static constexpr int FLIP_LY =  1;
static constexpr int FLIP_RY =  1;
static constexpr int FLIP_RX = -1;

// --- Auto line follow ---
static constexpr bool AUTO_REVERSE    = true;   // keep true (your working setup)
static constexpr int  AUTO_TURN_HARD  = 250;
static constexpr int  AUTO_RECOVER    = 150;
static constexpr int  AUTO_RECOVER_FAST = 250;
static constexpr uint32_t LINE_LOST_ESCALATE_MS = 500;
static constexpr uint32_t LINE_LOST_ABORT_MS    = 2500;

// --- Timing ---
static constexpr uint32_t CTRL_MS      = 10;
static constexpr uint32_t TELEMETRY_MS = 100;
static constexpr uint32_t NET_TASK_DELAY_MS = 5;

// =====================================================================
//  RUNTIME-TUNABLE AUTO PARAMETERS (changed via D-pad)
// =====================================================================
static int auto_base      = 700;  // D-pad Up/Down
static int auto_turn_soft = 1000;  // D-pad Left/Right

static constexpr int AUTO_BASE_MIN = 0;
static constexpr int AUTO_BASE_MAX = 1023;
static constexpr int AUTO_TURN_MIN = 0;
static constexpr int AUTO_TURN_MAX = 1023;

static constexpr int AUTO_BASE_STEP = 50;
static constexpr int AUTO_TURN_STEP = 50;

// =====================================================================
//  A-BUTTON "TRAILER CROSS" ASSIST
// =====================================================================
static constexpr uint32_t ASSIST_MS = 2000;     // drive forward for this long
static constexpr int      ASSIST_SCALE = 90;   // % of auto_base used as forward speed
static uint32_t assistUntil = 0;
static constexpr int ASSIST_DIR = -1;   // +1 or -1. Set so A drives the robot FORWARD.

// =====================================================================
//  QUADRATURE TABLE
// =====================================================================
static const int8_t QTAB[16] = {
   0, -1, +1,  0,
  +1,  0,  0, -1,
  -1,  0,  0, +1,
   0, +1, -1,  0
};

// =====================================================================
//  HARDWARE OBJECTS
// =====================================================================
static WebServer         httpServer(80);
static WebSocketsServer  wsServer(81);
static ControllerPtr     pads[BP32_MAX_GAMEPADS];
static Servo             myServo;

// =====================================================================
//  SHARED STATE STRUCTS
// =====================================================================
struct EncoderState {
  volatile long     left  = 0;
  volatile long     right = 0;
  volatile uint8_t  lastStateL = 0;
  volatile uint8_t  lastStateR = 0;
};

struct OdometryState {
  float distLeft_mm  = 0.0f;
  float distRight_mm = 0.0f;
  float distance_mm  = 0.0f;
  float heading_deg  = 0.0f;
};

struct SpeedState { float rpmL = 0.0f; float rpmR = 0.0f; };
struct IrState { bool left=false, center=false, right=false; };
struct MotorState { int outM1=0, outM2=0; };
struct ServoState { int deg=180; uint32_t lastMs=0; };
struct DriveMode { bool autoMode=false; };

struct TelemetrySnapshot {
  bool  autoMode;
  bool  ctrlConnected;
  bool  irL, irC, irR;
  float rpmL, rpmR;
  float distance_mm;
  float heading_deg;
  int   servoDeg;
  int   cmdLeft, cmdRight;
  int   autoBase;
  int   autoTurnSoft;
  int   assistActive;
};

static EncoderState    enc;
static OdometryState   odo;
static SpeedState      spd;
static IrState         ir;
static MotorState      mot;
static ServoState      servo_st;
static DriveMode       driveMode;
static TelemetrySnapshot telSnap;

static portMUX_TYPE    telMux  = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE    encMux  = portMUX_INITIALIZER_UNLOCKED;

static int lastCmdLeft  = 0;
static int lastCmdRight = 0;

// =====================================================================
//  UTILS
// =====================================================================
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int deadband(int v, int db) { return (v > -db && v < db) ? 0 : v; }
static inline long scaleAxis(int axis, int maxOut) {
  axis = clampi(axis, -1023, 1023);
  return (long)axis * (long)maxOut / 1023L;
}

static void tankMix(int fwd, int turn, int* left, int* right) {
  int l = fwd + turn;
  int r = fwd - turn;
  int maxMag = max(abs(l), abs(r));
  if (maxMag > 1023) {
    l = (int)((long)l * 1023L / maxMag);
    r = (int)((long)r * 1023L / maxMag);
  }
  *left  = l;
  *right = r;
}

// =====================================================================
//  MOTOR DRIVER
// =====================================================================
static void driveHBridge(int chA, int chB, int cmd) {
  cmd = clampi(cmd, -1023, 1023);
  if (cmd >= 0) { ledcWrite(chA, (uint32_t)cmd);  ledcWrite(chB, 0); }
  else { ledcWrite(chA, 0); ledcWrite(chB, (uint32_t)(-cmd)); }
}
static void setM1raw(int cmd) { if (M1_REVERSED) cmd = -cmd; driveHBridge(CH_M1A, CH_M1B, cmd); }
static void setM2raw(int cmd) { if (M2_REVERSED) cmd = -cmd; driveHBridge(CH_M2A, CH_M2B, cmd); }

static void setMotors(int leftCmd, int rightCmd, bool useRamp = true) {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  uint32_t dt  = now - lastMs;
  lastMs = now;
  if (dt > 50) dt = 50;

  leftCmd  = clampi(leftCmd,  -1023, 1023);
  rightCmd = clampi(rightCmd, -1023, 1023);

  int tM1 = SWAP_SIDES ? rightCmd : leftCmd;
  int tM2 = SWAP_SIDES ? leftCmd  : rightCmd;

  if (useRamp) {
    int step = max(1, (int)((long)RAMP_RATE_PER_SEC * (long)dt / 1000L));
    auto rampf = [](int cur, int tgt, int s) -> int {
      if (tgt > cur + s) return cur + s;
      if (tgt < cur - s) return cur - s;
      return tgt;
    };
    mot.outM1 = rampf(mot.outM1, tM1, step);
    mot.outM2 = rampf(mot.outM2, tM2, step);
  } else {
    mot.outM1 = tM1;
    mot.outM2 = tM2;
  }

  setM1raw(mot.outM1);
  setM2raw(mot.outM2);

  lastCmdLeft  = leftCmd;
  lastCmdRight = rightCmd;
}

static void stopAll() { setMotors(0, 0, false); }

// =====================================================================
//  IR SENSOR — majority vote
// =====================================================================
static bool isBlackRaw(int pin) {
  int v = digitalRead(pin);
  return IR_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

static void readIR() {
  static uint8_t vL[IR_VOTE_N] = {}, vC[IR_VOTE_N] = {}, vR[IR_VOTE_N] = {};
  static int idx = 0;

  vL[idx] = isBlackRaw(IR_LEFT)   ? 1 : 0;
  vC[idx] = isBlackRaw(IR_CENTER) ? 1 : 0;
  vR[idx] = isBlackRaw(IR_RIGHT)  ? 1 : 0;
  idx = (idx + 1) % IR_VOTE_N;

  int sumL=0, sumC=0, sumR=0;
  for (int i=0;i<IR_VOTE_N;i++){ sumL+=vL[i]; sumC+=vC[i]; sumR+=vR[i]; }
  int thresh = (IR_VOTE_N / 2) + 1;
  ir.left   = sumL >= thresh;
  ir.center = sumC >= thresh;
  ir.right  = sumR >= thresh;
}

// =====================================================================
//  AUTO LINE FOLLOW
// =====================================================================
static void lineFollowStep() {
  static uint32_t lineLostSince = 0;
  static int lastTurn = 0;

  int fwd = (AUTO_REVERSE ? -auto_base : +auto_base);

  int code = (ir.left ? 4 : 0) | (ir.center ? 2 : 0) | (ir.right ? 1 : 0);

  int turn = 0;
  bool onLine = true;

  switch (code) {
    case 2:
    case 7:
    case 5:
      turn = 0;
      lastTurn = 0;
      break;

    case 6: turn = +auto_turn_soft; lastTurn = +1; break; // line left -> steer right
    case 4: turn = +AUTO_TURN_HARD; lastTurn = +1; break;
    case 3: turn = -auto_turn_soft; lastTurn = -1; break; // line right -> steer left
    case 1: turn = -AUTO_TURN_HARD; lastTurn = -1; break;

    default:
      onLine = false;
      break;
  }

  if (onLine) {
    lineLostSince = 0;
    int leftCmd, rightCmd;
    tankMix(fwd, turn, &leftCmd, &rightCmd);
    setMotors(leftCmd, rightCmd, false);
    return;
  }

  if (lineLostSince == 0) lineLostSince = millis();
  uint32_t lostMs = millis() - lineLostSince;

  if (lostMs >= LINE_LOST_ABORT_MS) {
    stopAll();
    driveMode.autoMode = false;
    Serial.println("AUTO: line lost timeout — disabling auto mode");
    return;
  }

  int recoverSpeed = (lostMs >= LINE_LOST_ESCALATE_MS) ? AUTO_RECOVER_FAST : AUTO_RECOVER;
  int spinTurn = (lastTurn >= 0) ? +recoverSpeed : -recoverSpeed;

  int leftCmd, rightCmd;
  tankMix(0, spinTurn, &leftCmd, &rightCmd);
  setMotors(leftCmd, rightCmd, false);
}

// =====================================================================
//  ENCODER ISRs
// =====================================================================
void IRAM_ATTR isrLeft() {
  uint8_t a = (uint8_t)digitalRead(ENC_L_A);
  uint8_t b = (uint8_t)digitalRead(ENC_L_B);
  uint8_t s = (a << 1) | b;
  uint8_t idx = (enc.lastStateL << 2) | s;
  enc.left += QTAB[idx];
  enc.lastStateL = s;
}

void IRAM_ATTR isrRight() {
  uint8_t a = (uint8_t)digitalRead(ENC_R_A);
  uint8_t b = (uint8_t)digitalRead(ENC_R_B);
  uint8_t s = (a << 1) | b;
  uint8_t idx = (enc.lastStateR << 2) | s;
  int8_t d = QTAB[idx];
  enc.right += INVERT_RIGHT_ENCODER ? -d : d;
  enc.lastStateR = s;
}

// =====================================================================
//  SPEED + ODOMETRY
// =====================================================================
static void updateSpeedAndOdometry() {
  static uint32_t lastT  = 0;
  static long     lastL  = 0;
  static long     lastR  = 0;

  uint32_t now = millis();
  if (now - lastT < SPEED_INTERVAL_MS) return;
  uint32_t dt_ms = now - lastT;
  lastT = now;

  long curL, curR;
  portENTER_CRITICAL(&encMux);
  curL = enc.left;
  curR = enc.right;
  portEXIT_CRITICAL(&encMux);

  long dL = curL - lastL;
  long dR = curR - lastR;
  lastL = curL;
  lastR = curR;

  float dt_s = dt_ms / 1000.0f;

  float revL = dL / CPR_4X;
  float revR = dR / CPR_4X;
  float rawRpmL = (revL / dt_s) * 60.0f;
  float rawRpmR = (revR / dt_s) * 60.0f;
  spd.rpmL += RPM_ALPHA * (rawRpmL - spd.rpmL);
  spd.rpmR += RPM_ALPHA * (rawRpmR - spd.rpmR);

  static constexpr float MM_PER_COUNT = (3.14159265f * WHEEL_DIAMETER_MM) / CPR_4X;
  float dLeft_mm  = dL * MM_PER_COUNT;
  float dRight_mm = dR * MM_PER_COUNT;
  odo.distLeft_mm  += dLeft_mm;
  odo.distRight_mm += dRight_mm;

  float dCenter = (dLeft_mm + dRight_mm) * 0.5f;
  float dTheta  = (dRight_mm - dLeft_mm) / WHEEL_TRACK_MM;
  odo.distance_mm += dCenter;
  odo.heading_deg += dTheta * (180.0f / 3.14159265f);
  while (odo.heading_deg >  180.0f) odo.heading_deg -= 360.0f;
  while (odo.heading_deg < -180.0f) odo.heading_deg += 360.0f;
}

// =====================================================================
//  SERVO
// =====================================================================
static int servoAngleToWrite(int deg) {
  deg = clampi(deg, SERVO_MIN_DEG, SERVO_MAX_DEG);
  return SERVO_INVERT ? (180 - deg) : deg;
}

static void updateServo(ControllerPtr ctl) {
  uint32_t now = millis();
  uint32_t dt  = now - servo_st.lastMs;
  servo_st.lastMs = now;
  if (dt > 100) dt = 100;

  bool L = ctl->l1();
  bool R = ctl->r1();
  if (L == R) return;

  float fstep = SERVO_SPEED_DPS * dt / 1000.0f;
  int   istep = max(1, (int)fstep);

  if (L) servo_st.deg -= istep;
  if (R) servo_st.deg += istep;

  servo_st.deg = clampi(servo_st.deg, SERVO_MIN_DEG, SERVO_MAX_DEG);
  myServo.write(servoAngleToWrite(servo_st.deg));
}

// =====================================================================
//  D-PAD AUTO TUNING (while holding B)
// =====================================================================
static void handleAutoTuning(ControllerPtr ctl) {
  if (!ctl->b()) return;

  static uint8_t last = 0;
  uint8_t now = ctl->dpad();
  if (now == last) return;

  bool changed = false;

  // Up=0x01, Down=0x02, Right=0x04, Left=0x08
  if ((now & 0x01) && !(last & 0x01)) { auto_base = clampi(auto_base + AUTO_BASE_STEP, AUTO_BASE_MIN, AUTO_BASE_MAX); changed = true; }
  if ((now & 0x02) && !(last & 0x02)) { auto_base = clampi(auto_base - AUTO_BASE_STEP, AUTO_BASE_MIN, AUTO_BASE_MAX); changed = true; }
  if ((now & 0x04) && !(last & 0x04)) { auto_turn_soft = clampi(auto_turn_soft + AUTO_TURN_STEP, AUTO_TURN_MIN, AUTO_TURN_MAX); changed = true; }
  if ((now & 0x08) && !(last & 0x08)) { auto_turn_soft = clampi(auto_turn_soft - AUTO_TURN_STEP, AUTO_TURN_MIN, AUTO_TURN_MAX); changed = true; }

  if (changed) {
    Serial.print("AUTO_BASE=");
    Serial.print(auto_base);
    Serial.print("  AUTO_TURN_SOFT=");
    Serial.println(auto_turn_soft);
  }

  last = now;
}

// =====================================================================
//  A BUTTON ASSIST (edge trigger; requires B held)
// =====================================================================
static void handleAssist(ControllerPtr ctl) {
  static bool lastA = false;
  bool nowA = ctl->a();

  if (nowA && !lastA && ctl->b()) {
    assistUntil = millis() + ASSIST_MS;
    Serial.print("ASSIST: forward ");
    Serial.print(ASSIST_MS);
    Serial.println("ms");
  }
  lastA = nowA;
}

static bool assistActive() {
  return (assistUntil != 0) && (millis() < assistUntil);
}

static void runAssistDrive() {
  int fwd = (auto_base * ASSIST_SCALE) / 100;
  fwd *= ASSIST_DIR;                 // <-- flips assist direction only
  int leftCmd, rightCmd;
  tankMix(fwd, 0, &leftCmd, &rightCmd);
  setMotors(leftCmd, rightCmd, false);
}
// =====================================================================
//  CONTROLLER CALLBACKS
// =====================================================================
static void onConnected(ControllerPtr ctl) {
  Serial.println("Controller connected");
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (!pads[i]) { pads[i] = ctl; break; }
  }
}
static void onDisconnected(ControllerPtr ctl) {
  Serial.println("Controller disconnected");
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (pads[i] == ctl) { pads[i] = nullptr; break; }
  }
  stopAll();
}
static bool controllerPresent() {
  for (auto p : pads) if (p && p->isConnected()) return true;
  return false;
}

// =====================================================================
//  TELEMETRY SNAPSHOT + PUSH
// =====================================================================
static void snapshotTelemetry() {
  portENTER_CRITICAL(&telMux);
  telSnap.autoMode      = driveMode.autoMode;
  telSnap.ctrlConnected = controllerPresent();
  telSnap.irL           = ir.left;
  telSnap.irC           = ir.center;
  telSnap.irR           = ir.right;
  telSnap.rpmL          = spd.rpmL;
  telSnap.rpmR          = spd.rpmR;
  telSnap.distance_mm   = odo.distance_mm;
  telSnap.heading_deg   = odo.heading_deg;
  telSnap.servoDeg      = servo_st.deg;
  telSnap.cmdLeft       = lastCmdLeft;
  telSnap.cmdRight      = lastCmdRight;
  telSnap.autoBase      = auto_base;
  telSnap.autoTurnSoft  = auto_turn_soft;
  telSnap.assistActive  = assistActive() ? 1 : 0;
  portEXIT_CRITICAL(&telMux);
}

static void pushTelemetry() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < TELEMETRY_MS) return;
  last = now;

  if (wsServer.connectedClients() == 0) return;

  TelemetrySnapshot snap;
  portENTER_CRITICAL(&telMux);
  snap = telSnap;
  portEXIT_CRITICAL(&telMux);

  // CSV:
  // mode,ctrl,irL,irC,irR,rpmL*10,rpmR*10,dist*10,head*10,servo,cmdL,cmdR,autoBase,autoTurnSoft,assist
  char buf[160];
  int n = snprintf(buf, sizeof(buf),
    "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
    snap.autoMode ? 1 : 0,
    snap.ctrlConnected ? 1 : 0,
    snap.irL ? 1 : 0,
    snap.irC ? 1 : 0,
    snap.irR ? 1 : 0,
    (int)(snap.rpmL * 10.0f),
    (int)(snap.rpmR * 10.0f),
    (int)(snap.distance_mm * 10.0f),
    (int)(snap.heading_deg * 10.0f),
    snap.servoDeg,
    snap.cmdLeft,
    snap.cmdRight,
    snap.autoBase,
    snap.autoTurnSoft,
    snap.assistActive
  );
  if (n > 0) wsServer.broadcastTXT(buf, (size_t)n);
}

// =====================================================================
//  NETWORK TASK  (Core 0)
// =====================================================================
static void networkTask(void* /*param*/) {
  for (;;) {
    wsServer.loop();
    httpServer.handleClient();
    pushTelemetry();
    vTaskDelay(pdMS_TO_TICKS(NET_TASK_DELAY_MS));
  }
}

// =====================================================================
//  SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  ledcSetup(CH_M1A, PWM_FREQ, PWM_RES);  ledcAttachPin(M1A, CH_M1A);
  ledcSetup(CH_M1B, PWM_FREQ, PWM_RES);  ledcAttachPin(M1B, CH_M1B);
  ledcSetup(CH_M2A, PWM_FREQ, PWM_RES);  ledcAttachPin(M2A, CH_M2A);
  ledcSetup(CH_M2B, PWM_FREQ, PWM_RES);  ledcAttachPin(M2B, CH_M2B);

  pinMode(IR_LEFT,   INPUT_PULLUP);
  pinMode(IR_CENTER, INPUT_PULLUP);
  pinMode(IR_RIGHT,  INPUT_PULLUP);

  pinMode(ENC_L_A, INPUT_PULLUP);  pinMode(ENC_L_B, INPUT_PULLUP);
  pinMode(ENC_R_A, INPUT_PULLUP);  pinMode(ENC_R_B, INPUT_PULLUP);

  enc.lastStateL = (uint8_t)((digitalRead(ENC_L_A) << 1) | digitalRead(ENC_L_B));
  enc.lastStateR = (uint8_t)((digitalRead(ENC_R_A) << 1) | digitalRead(ENC_R_B));

  attachInterrupt(digitalPinToInterrupt(ENC_L_A), isrLeft,  CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_L_B), isrLeft,  CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_R_A), isrRight, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_R_B), isrRight, CHANGE);

  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2500);
  servo_st.deg    = SERVO_MAX_DEG;
  servo_st.lastMs = millis();
  myServo.write(servoAngleToWrite(servo_st.deg));

  stopAll();

  BP32.setup(&onConnected, &onDisconnected);
  BP32.enableNewBluetoothConnections(true);

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(AP_SSID, AP_PASS);

  httpServer.on("/", []() { httpServer.send_P(200, "text/html", INDEX_HTML); });
  httpServer.begin();
  wsServer.begin();

  Serial.printf("AP: %s  IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  Serial.println("Controls: B (hold) = motors | Y = toggle AUTO | LB/RB = servo | A = forward assist");
  Serial.print("AUTO_BASE="); Serial.print(auto_base);
  Serial.print("  AUTO_TURN_SOFT="); Serial.println(auto_turn_soft);

  xTaskCreatePinnedToCore(networkTask, "net", 4096, nullptr, 1, nullptr, 0);
}

// =====================================================================
//  LOOP  —  Core 1 control
// =====================================================================
void loop() {
  BP32.update();

  readIR();
  updateSpeedAndOdometry();
  snapshotTelemetry();

  static uint32_t lastCtrl = 0;
  uint32_t now = millis();
  if (now - lastCtrl < CTRL_MS) return;
  lastCtrl = now;

  ControllerPtr ctl = nullptr;
  for (auto p : pads) {
    if (p && p->isConnected()) { ctl = p; break; }
  }
  if (!ctl) { stopAll(); return; }

  updateServo(ctl);

  handleAutoTuning(ctl);
  handleAssist(ctl);

  // Y toggles AUTO/MAN regardless of B held — mode switching needs no safety gating
  static bool lastY = false;
  bool nowY = ctl->y();
  if (nowY && !lastY) {
    driveMode.autoMode = !driveMode.autoMode;
    Serial.printf("AUTO mode: %s\n", driveMode.autoMode ? "ON" : "OFF");
  }
  lastY = nowY;

  // Motors require B held (safety) — Y toggle above is exempt
  if (!ctl->b()) {
    stopAll();
    return;
  }
  if (assistActive()) {
    runAssistDrive();
    return;
  }

  if (driveMode.autoMode) {
    lineFollowStep();
    return;
  }

  int ly = deadband((int)ctl->axisY()  * FLIP_LY, DB_STICK);
  int ry = deadband((int)ctl->axisRY() * FLIP_RY, DB_STICK);
  int rx = deadband((int)ctl->axisRX() * FLIP_RX, DB_STICK);

  int rt = ctl->throttle();
  int lt = ctl->brake();
  if (rt < DB_TRIG) rt = 0;
  if (lt < DB_TRIG) lt = 0;

  int fwdSlow  = (int)scaleAxis(ly, SLOW_FWD_MAX);
  int fwdFast  = (int)scaleAxis(ry, FAST_FWD_MAX);
  int turnFast = (int)scaleAxis(rx, FAST_TURN_MAX);
  int turnSpin = ((lt - rt) * SLOW_SPIN_MAX) / 1023;

  int fwd  = clampi(fwdSlow + fwdFast, -1023, 1023);
  int turn = clampi(turnFast + turnSpin, -1023, 1023);

  int leftCmd, rightCmd;
  tankMix(fwd, turn, &leftCmd, &rightCmd);
  setMotors(leftCmd, rightCmd, true);
}