#include <Arduino.h>
// --- FUNCTION SIGNATURES ---
void updateIndependentSensorStates();

// --- INPUT PINS ---
const int PIN_IN_HEAD = 6;       // Car Pin 8 (Stalk Head)
const int PIN_IN_LATCH = 7;      // Car Pin 6 (Stalk Middle/Latch)
const int PIN_SENSOR_B = 8;      // Car Pin 11 
const int PIN_SENSOR_A = 9;      // Car Pin 7  

// --- MULTIPLEXER POWER PINS ---
const int PIN_OUT_RIGHT_COMMON = 5;  // Car Pin 5
const int PIN_OUT_LEFT_COMMON = 12;  // Car Pin 2

// --- MOTOR POWER PINS ---
const int PIN_MOTOR_RIGHT = 10;  
const int PIN_MOTOR_LEFT = 11;   

// --- STATE VARIABLES ---
int leftRawState = 2;   
int rightRawState = 2;  

// VIRTUAL MEMORY (The Anti-Coasting Lock)
// 0 = Parked Down, 1 = Parked Up, 2 = Lost/Moving
int leftVirtualPos = 2; 
int rightVirtualPos = 2;

// --- SWITCH DEBOUNCE & TRACKING ---
bool lastStableWantUp = false;
bool currentSwitchRead = false;
unsigned long switchDebounceTime = 0;

int targetState = 0;

bool moveLeftAuth = false;  
bool moveRightAuth = false; 

unsigned long leftStartTime = 0;
unsigned long rightStartTime = 0;
bool leftJammed = false;
bool rightJammed = false;

void setup() {
  Serial.begin(9600);
  
  pinMode(PIN_IN_HEAD, INPUT_PULLUP);
  pinMode(PIN_IN_LATCH, INPUT_PULLUP);
  pinMode(PIN_SENSOR_A, INPUT_PULLUP);
  pinMode(PIN_SENSOR_B, INPUT_PULLUP);

  pinMode(PIN_OUT_RIGHT_COMMON, OUTPUT);
  pinMode(PIN_OUT_LEFT_COMMON, OUTPUT);
  pinMode(PIN_MOTOR_LEFT, OUTPUT);
  pinMode(PIN_MOTOR_RIGHT, OUTPUT);

  digitalWrite(PIN_MOTOR_LEFT, LOW);
  digitalWrite(PIN_MOTOR_RIGHT, LOW);

  // --- BOOT SEQUENCE SYNC ---
  int stalkLog = !digitalRead(PIN_IN_HEAD);
  int latchLog = !digitalRead(PIN_IN_LATCH);
  
  if (stalkLog == 0) lastStableWantUp = true;
  else lastStableWantUp = false; // Default closed on boot if in middle or off

  targetState = lastStableWantUp ? 1 : 0;
  currentSwitchRead = lastStableWantUp;

  updateIndependentSensorStates();
  
  // Set virtual position to whatever the sensors currently see
  leftVirtualPos = leftRawState;
  rightVirtualPos = rightRawState;

  // Self-heal if out of sync on boot
  if (leftVirtualPos != targetState) {
    moveLeftAuth = true;
    leftStartTime = millis();
  }
  if (rightVirtualPos != targetState) {
    moveRightAuth = true;
    rightStartTime = millis();
  }
}

void loop() {
  // 1. READ DRIVER COMMAND 
  int stalkLog = !digitalRead(PIN_IN_HEAD);
  int latchLog = !digitalRead(PIN_IN_LATCH);

  bool rawWantUp;
  if (stalkLog == 0) {
    rawWantUp = true; // HEAD forces OPEN
  } else if (latchLog == 0) {
    rawWantUp = lastStableWantUp; // LATCH "keeps" whatever state we were just in
  } else {
    rawWantUp = false; // OFF forces CLOSED
  }

  // 2. SWITCH DEBOUNCE (Filters out mechanical stalk bounce)
  if (rawWantUp != currentSwitchRead) {
    switchDebounceTime = millis();
    currentSwitchRead = rawWantUp;
  }

  if ((millis() - switchDebounceTime) > 50) { // Switch must settle for 50ms
    if (currentSwitchRead != lastStableWantUp) {
      targetState = currentSwitchRead ? 1 : 0;
      
      // Grant permission ONLY if the virtual parked position doesn't match the target
      if (leftVirtualPos != targetState) {
        moveLeftAuth = true;
        leftJammed = false;
        leftStartTime = millis();
      }
      if (rightVirtualPos != targetState) {
        moveRightAuth = true;
        rightJammed = false;
        rightStartTime = millis();
      }
      
      lastStableWantUp = currentSwitchRead;
    }
  }

  // 3. READ PHYSICAL REALITY
  if (moveLeftAuth || moveRightAuth) {
    updateIndependentSensorStates();
  }

  // 4. LEFT MOTOR EXECUTION
  if (moveLeftAuth && !leftJammed) {
    digitalWrite(PIN_MOTOR_LEFT, HIGH);

    if (leftRawState == targetState) {
      digitalWrite(PIN_MOTOR_LEFT, LOW); 
      moveLeftAuth = false;              
      leftVirtualPos = targetState; // LOCK VIRTUAL POSITION (Ignores coasting completely)
    } 
    else if (millis() - leftStartTime > 3000) {
      digitalWrite(PIN_MOTOR_LEFT, LOW);
      moveLeftAuth = false;
      leftJammed = true;
    }
  } else {
    digitalWrite(PIN_MOTOR_LEFT, LOW); 
  }

  // 5. RIGHT MOTOR EXECUTION
  if (moveRightAuth && !rightJammed) {
    digitalWrite(PIN_MOTOR_RIGHT, HIGH);

    if (rightRawState == targetState) {
      digitalWrite(PIN_MOTOR_RIGHT, LOW); 
      moveRightAuth = false;               
      rightVirtualPos = targetState; // LOCK VIRTUAL POSITION (Ignores coasting completely)
    } 
    else if (millis() - rightStartTime > 3000) {
      digitalWrite(PIN_MOTOR_RIGHT, LOW);
      moveRightAuth = false;
      rightJammed = true;
    }
  } else {
    digitalWrite(PIN_MOTOR_RIGHT, LOW); 
  }
}

// ---------------------------------------------------------
// THE MULTIPLEXER LOGIC
// ---------------------------------------------------------
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