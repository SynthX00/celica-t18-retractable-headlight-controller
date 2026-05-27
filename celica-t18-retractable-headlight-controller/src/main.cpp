#include <Arduino.h>
// =========================================================
// TOYOTA CELICA BRAWN BOARD - MASTER FIRMWARE v3 (Hybrid Tuning)
// =========================================================

// --- FUNCTION SIGNATURES ---
void updateIndependentSensorStates();

// --- PINS: STALK & SENSORS (Active-Low) ---
const int PIN_IN_HEAD = 6;       
const int PIN_IN_LATCH = 7;      
const int PIN_SENSOR_B = 8;      
const int PIN_SENSOR_A = 9;      

// --- PINS: MULTIPLEXER POWER ---
const int PIN_OUT_RIGHT_COMMON = 5;  
const int PIN_OUT_LEFT_COMMON = 12;  

// --- PINS: MOTORS (PWM Capable) ---
const int PIN_MOTOR_RIGHT = 10;  
const int PIN_MOTOR_LEFT = 11;   

// --- PINS: ANIMATION BUTTONS ---
const int PIN_BTN_WL = 2;  
const int PIN_BTN_WR = 3;  
const int PIN_BTN_SLP = 4; 

// =========================================================
// ⚙️ TUNING & SETTINGS (Adjust these to perfection!)
// =========================================================

// MASTER SPEED LIMITS (0 to 255)
// The Left motor is faster. Lower this number until they move at exactly the same speed!
const int LEFT_MAX_SPEED = 180;  
const int RIGHT_MAX_SPEED = 255;  

// SLEEPY EYE HEIGHT TUNING (Decoupled for millimeter-perfect leveling)
const int LEFT_SLEEPY_FROM_DOWN = 240;  
const int RIGHT_SLEEPY_FROM_DOWN = 240; 
const int LEFT_SLEEPY_FROM_UP = 100;    
const int RIGHT_SLEEPY_FROM_UP = 200;   
// =========================================================

// --- STATE VARIABLES ---
int leftRawState = 2;   
int rightRawState = 2;  
int leftVirtualPos = 2; 
int rightVirtualPos = 2;
int stalkState = 0; 

// --- DRIVER INPUT TRACKING ---
bool lastStableWantUp = false;
bool currentSwitchRead = false;
unsigned long switchDebounceTime = 0;

// --- MOTOR TARGETS & AUTHORIZATION ---
int desiredLeft = 0;
int desiredRight = 0;
bool moveLeftAuth = false;  
bool moveRightAuth = false; 
unsigned long leftStartTime = 0;
unsigned long rightStartTime = 0;
bool leftJammed = false;
bool rightJammed = false;

// --- ANIMATION STATE MACHINE ---
enum Mode { FACTORY, SLEEPY, WINK_L, WINK_R, ALT_WINK };
Mode currentMode = FACTORY;

bool sleepyActive = false;
bool stopOnTime = false; 
unsigned long leftTimeLimit = 0;
unsigned long rightTimeLimit = 0;

int winkPhase = 0;
unsigned long winkTimer = 0;

// Button Tracking
bool wl_last = false;
bool wr_last = false;
bool slp_last = false;
unsigned long pressTimeWL = 0;
unsigned long pressTimeWR = 0;
unsigned long pressTimeSLP = 0;

void setup() {
  Serial.begin(9600);
  
  pinMode(PIN_IN_HEAD, INPUT_PULLUP);
  pinMode(PIN_IN_LATCH, INPUT_PULLUP);
  pinMode(PIN_SENSOR_A, INPUT_PULLUP);
  pinMode(PIN_SENSOR_B, INPUT_PULLUP);
  pinMode(PIN_BTN_WL, INPUT_PULLUP);
  pinMode(PIN_BTN_WR, INPUT_PULLUP);
  pinMode(PIN_BTN_SLP, INPUT_PULLUP);

  pinMode(PIN_OUT_RIGHT_COMMON, OUTPUT);
  pinMode(PIN_OUT_LEFT_COMMON, OUTPUT);
  pinMode(PIN_MOTOR_LEFT, OUTPUT);
  pinMode(PIN_MOTOR_RIGHT, OUTPUT);

  digitalWrite(PIN_MOTOR_LEFT, LOW);
  digitalWrite(PIN_MOTOR_RIGHT, LOW);

  int stalkLog = !digitalRead(PIN_IN_HEAD);
  int latchLog = !digitalRead(PIN_IN_LATCH);
  
  if (stalkLog == 0) lastStableWantUp = true;
  else lastStableWantUp = false; 

  stalkState = lastStableWantUp ? 1 : 0;
  currentSwitchRead = lastStableWantUp;
  desiredLeft = stalkState;
  desiredRight = stalkState;

  updateIndependentSensorStates();
  leftVirtualPos = leftRawState;
  rightVirtualPos = rightRawState;

  if (leftVirtualPos != desiredLeft) {
    moveLeftAuth = true; leftStartTime = millis();
  }
  if (rightVirtualPos != desiredRight) {
    moveRightAuth = true; rightStartTime = millis();
  }
}

void loop() {
  // 1. STALK SAFETY OVERRIDE
  int stalkLog = !digitalRead(PIN_IN_HEAD);
  int latchLog = !digitalRead(PIN_IN_LATCH);

  bool rawWantUp;
  if (stalkLog == 0) rawWantUp = true; 
  else if (latchLog == 0) rawWantUp = lastStableWantUp; 
  else rawWantUp = false; 

  if (rawWantUp != currentSwitchRead) {
    switchDebounceTime = millis();
    currentSwitchRead = rawWantUp;
  }

  if ((millis() - switchDebounceTime) > 50) { 
    if (currentSwitchRead != lastStableWantUp) {
      stalkState = currentSwitchRead ? 1 : 0;
      
      currentMode = FACTORY;
      sleepyActive = false;
      stopOnTime = false;
      winkPhase = 0;
      
      desiredLeft = stalkState;
      desiredRight = stalkState;
      
      if (leftVirtualPos != stalkState) {
        moveLeftAuth = true; leftJammed = false; leftStartTime = millis();
      }
      if (rightVirtualPos != stalkState) {
        moveRightAuth = true; rightJammed = false; rightStartTime = millis();
      }
      lastStableWantUp = currentSwitchRead;
    }
  }

  // 2. ANIMATION BUTTON LOGIC 
  bool readWL = !digitalRead(PIN_BTN_WL);
  bool readWR = !digitalRead(PIN_BTN_WR);
  bool readSLP = !digitalRead(PIN_BTN_SLP);

  if (readWL && !wl_last) pressTimeWL = millis();
  if (readWR && !wr_last) pressTimeWR = millis();
  if (readSLP && !slp_last) pressTimeSLP = millis();

  // --- SLEEPY TOGGLE ---
  if (!readSLP && slp_last) {
    unsigned long holdTime = millis() - pressTimeSLP;
    if (holdTime > 20) { 
      if (sleepyActive) {
        sleepyActive = false; currentMode = FACTORY; stopOnTime = false;
        desiredLeft = stalkState; desiredRight = stalkState;
        moveLeftAuth = true; leftStartTime = millis(); leftVirtualPos = 2;
        moveRightAuth = true; rightStartTime = millis(); rightVirtualPos = 2;
      } else if (currentMode == FACTORY) {
        sleepyActive = true; currentMode = SLEEPY; stopOnTime = true; 
        
        // Decoupled independent timers to perfectly level the height!
        leftTimeLimit = (stalkState == 0) ? LEFT_SLEEPY_FROM_DOWN : LEFT_SLEEPY_FROM_UP;
        rightTimeLimit = (stalkState == 0) ? RIGHT_SLEEPY_FROM_DOWN : RIGHT_SLEEPY_FROM_UP;
        
        moveLeftAuth = true; leftStartTime = millis(); leftVirtualPos = 2;
        moveRightAuth = true; rightStartTime = millis(); rightVirtualPos = 2;
      }
    }
  }

  // --- SINGLE WINK ---
  if (!readWL && wl_last) {
    unsigned long holdTime = millis() - pressTimeWL;
    if (holdTime > 20 && holdTime < 500 && currentMode == FACTORY) {
      currentMode = WINK_L; winkPhase = 1;
    }
  }
  if (!readWR && wr_last) {
    unsigned long holdTime = millis() - pressTimeWR;
    if (holdTime > 20 && holdTime < 500 && currentMode == FACTORY) {
      currentMode = WINK_R; winkPhase = 1;
    }
  }

  // --- ALTERNATING WINK ---
  if (readWL && readWR) {
    if ((millis() - pressTimeWL > 500) && (millis() - pressTimeWR > 500)) {
      if (currentMode == FACTORY) {
        currentMode = ALT_WINK; winkPhase = 1;
      }
    }
  }
  if (currentMode == ALT_WINK && (!readWL || !readWR)) {
    currentMode = FACTORY;
    desiredLeft = stalkState; desiredRight = stalkState;
    moveLeftAuth = true; leftStartTime = millis(); leftVirtualPos = 2;
    moveRightAuth = true; rightStartTime = millis(); rightVirtualPos = 2;
  }

  wl_last = readWL; wr_last = readWR; slp_last = readSLP;

  // 3. WINK STATE MACHINES
  if (currentMode == WINK_L) {
    if (winkPhase == 1) { 
      desiredLeft = (stalkState == 1) ? 0 : 1;
      moveLeftAuth = true; leftStartTime = millis(); leftVirtualPos = 2; stopOnTime = false; winkPhase = 2;
    } else if (winkPhase == 2) { 
      if (!moveLeftAuth) { winkTimer = millis(); winkPhase = 3; }
    } else if (winkPhase == 3) { 
      if (millis() - winkTimer > 300) { 
        desiredLeft = stalkState; moveLeftAuth = true; leftStartTime = millis(); leftVirtualPos = 2; winkPhase = 4;
      }
    } else if (winkPhase == 4) { 
      if (!moveLeftAuth) { currentMode = FACTORY; winkPhase = 0; }
    }
  }

  if (currentMode == WINK_R) {
    if (winkPhase == 1) {
      desiredRight = (stalkState == 1) ? 0 : 1;
      moveRightAuth = true; rightStartTime = millis(); rightVirtualPos = 2; stopOnTime = false; winkPhase = 2;
    } else if (winkPhase == 2) {
      if (!moveRightAuth) { winkTimer = millis(); winkPhase = 3; }
    } else if (winkPhase == 3) {
      if (millis() - winkTimer > 300) { 
        desiredRight = stalkState; moveRightAuth = true; rightStartTime = millis(); rightVirtualPos = 2; winkPhase = 4;
      }
    } else if (winkPhase == 4) {
      if (!moveRightAuth) { currentMode = FACTORY; winkPhase = 0; }
    }
  }

  if (currentMode == ALT_WINK) {
    if (winkPhase == 1) { 
      desiredLeft = (stalkState == 1) ? 0 : 1;
      desiredRight = stalkState; 
      moveLeftAuth = true; leftStartTime = millis(); leftVirtualPos = 2;
      moveRightAuth = true; rightStartTime = millis(); rightVirtualPos = 2;
      stopOnTime = false; winkPhase = 2;
    } else if (winkPhase == 2) { 
      if (!moveLeftAuth && !moveRightAuth) { winkTimer = millis(); winkPhase = 3; }
    } else if (winkPhase == 3) { 
      if (millis() - winkTimer > 150) {
        desiredLeft = stalkState; 
        desiredRight = (stalkState == 1) ? 0 : 1; 
        moveLeftAuth = true; leftStartTime = millis(); leftVirtualPos = 2;
        moveRightAuth = true; rightStartTime = millis(); rightVirtualPos = 2;
        winkPhase = 4;
      }
    } else if (winkPhase == 4) { 
      if (!moveLeftAuth && !moveRightAuth) { winkTimer = millis(); winkPhase = 5; }
    } else if (winkPhase == 5) { 
      if (millis() - winkTimer > 150) { winkPhase = 1; }
    }
  }

  if (moveLeftAuth || moveRightAuth) {
    updateIndependentSensorStates();
  }

  // 4. MOTOR EXECUTION (With Master Throttle Limits)
  if (moveLeftAuth && !leftJammed) {
    unsigned long currentMoveTime = millis() - leftStartTime;
    
    // APPLY MASTER SPEED LIMIT
    analogWrite(PIN_MOTOR_LEFT, LEFT_MAX_SPEED); 

    bool timeStopCondition = stopOnTime && (currentMoveTime >= leftTimeLimit);
    bool sensorStopCondition = !stopOnTime && (leftRawState == desiredLeft);

    if (timeStopCondition || sensorStopCondition) {
      digitalWrite(PIN_MOTOR_LEFT, LOW); 
      moveLeftAuth = false;              
      leftVirtualPos = stopOnTime ? 2 : desiredLeft; 
    } 
    else if (currentMoveTime > 3000) {
      digitalWrite(PIN_MOTOR_LEFT, LOW);
      moveLeftAuth = false; leftJammed = true;
    }
  } else {
    digitalWrite(PIN_MOTOR_LEFT, LOW); 
  }

  if (moveRightAuth && !rightJammed) {
    unsigned long currentMoveTime = millis() - rightStartTime;
    
    // APPLY MASTER SPEED LIMIT
    analogWrite(PIN_MOTOR_RIGHT, RIGHT_MAX_SPEED);

    bool timeStopCondition = stopOnTime && (currentMoveTime >= rightTimeLimit);
    bool sensorStopCondition = !stopOnTime && (rightRawState == desiredRight);

    if (timeStopCondition || sensorStopCondition) {
      digitalWrite(PIN_MOTOR_RIGHT, LOW); 
      moveRightAuth = false;               
      rightVirtualPos = stopOnTime ? 2 : desiredRight; 
    } 
    else if (currentMoveTime > 3000) {
      digitalWrite(PIN_MOTOR_RIGHT, LOW);
      moveRightAuth = false; rightJammed = true;
    }
  } else {
    digitalWrite(PIN_MOTOR_RIGHT, LOW); 
  }
}

void updateIndependentSensorStates() {
  digitalWrite(PIN_OUT_RIGHT_COMMON, LOW);  
  digitalWrite(PIN_OUT_LEFT_COMMON, HIGH);  
  delay(5); 
  
  bool left_A = !digitalRead(PIN_SENSOR_A); 
  bool left_B = !digitalRead(PIN_SENSOR_B); 
  
  if (left_A && !left_B) leftRawState = 0;       
  else if (!left_A && left_B) leftRawState = 1;  
  else leftRawState = 2;                         
  
  digitalWrite(PIN_OUT_LEFT_COMMON, LOW);   
  digitalWrite(PIN_OUT_RIGHT_COMMON, HIGH); 
  delay(5); 
  
  bool right_A = !digitalRead(PIN_SENSOR_A); 
  bool right_B = !digitalRead(PIN_SENSOR_B); 
  
  if (right_A && !right_B) rightRawState = 0;       
  else if (!right_A && right_B) rightRawState = 1;  
  else rightRawState = 2;                           

  digitalWrite(PIN_OUT_LEFT_COMMON, LOW);
  digitalWrite(PIN_OUT_RIGHT_COMMON, LOW);
}