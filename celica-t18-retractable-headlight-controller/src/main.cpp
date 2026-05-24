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
const int PIN_MOTOR_RIGHT = 10;  // PWM Capable Pin
const int PIN_MOTOR_LEFT = 11;   // PWM Capable Pin

// --- PWM SOFT-BRAKE TUNING ---
// Adjust these two numbers to tune the Left Motor!
const unsigned long LEFT_FULL_POWER_MS = 500;  // How many milliseconds to run at 100% speed
const int LEFT_CRAWL_SPEED = 140;              // The brake speed (0-255). 

// The Right motor is perfect, so we just tell it to run at 255 (100%) forever.
const unsigned long RIGHT_FULL_POWER_MS = 5000; 
const int RIGHT_CRAWL_SPEED = 255;              

// --- STATE VARIABLES ---
int leftRawState = 2;   
int rightRawState = 2;  

int leftVirtualPos = 2; 
int rightVirtualPos = 2;

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
  else lastStableWantUp = false; 

  targetState = lastStableWantUp ? 1 : 0;
  currentSwitchRead = lastStableWantUp;

  updateIndependentSensorStates();
  
  leftVirtualPos = leftRawState;
  rightVirtualPos = rightRawState;

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
    rawWantUp = true; 
  } else if (latchLog == 0) {
    rawWantUp = lastStableWantUp; 
  } else {
    rawWantUp = false; 
  }

  // 2. SWITCH DEBOUNCE
  if (rawWantUp != currentSwitchRead) {
    switchDebounceTime = millis();
    currentSwitchRead = rawWantUp;
  }

  if ((millis() - switchDebounceTime) > 50) { 
    if (currentSwitchRead != lastStableWantUp) {
      targetState = currentSwitchRead ? 1 : 0;
      
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

  // 4. LEFT MOTOR EXECUTION (WITH SOFT BRAKE)
  if (moveLeftAuth && !leftJammed) {
    unsigned long currentMoveTime = millis() - leftStartTime;
    
    // Check if it's time to deploy the parachute
    if (currentMoveTime < LEFT_FULL_POWER_MS) {
      analogWrite(PIN_MOTOR_LEFT, 255); // 100% Speed
    } else {
      analogWrite(PIN_MOTOR_LEFT, LEFT_CRAWL_SPEED); // Hit the brakes
    }

    if (leftRawState == targetState) {
      digitalWrite(PIN_MOTOR_LEFT, LOW); 
      moveLeftAuth = false;              
      leftVirtualPos = targetState; 
    } 
    else if (currentMoveTime > 3000) {
      digitalWrite(PIN_MOTOR_LEFT, LOW);
      moveLeftAuth = false;
      leftJammed = true;
    }
  } else {
    digitalWrite(PIN_MOTOR_LEFT, LOW); 
  }

  // 5. RIGHT MOTOR EXECUTION (WITH SOFT BRAKE)
  if (moveRightAuth && !rightJammed) {
    unsigned long currentMoveTime = millis() - rightStartTime;
    
    if (currentMoveTime < RIGHT_FULL_POWER_MS) {
      analogWrite(PIN_MOTOR_RIGHT, 255);
    } else {
      analogWrite(PIN_MOTOR_RIGHT, RIGHT_CRAWL_SPEED);
    }

    if (rightRawState == targetState) {
      digitalWrite(PIN_MOTOR_RIGHT, LOW); 
      moveRightAuth = false;               
      rightVirtualPos = targetState; 
    } 
    else if (currentMoveTime > 3000) {
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