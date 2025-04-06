#include <AccelStepper.h>
#include <Servo.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// Pin mappings
const int stepper1StepPin = A0;
const int stepper2StepPin = A1;
const int stepper3StepPin = A2;
const int directionPin = A3;
const int enablePin = 8;
const int reel1SensorPin = 8;
const int reel2SensorPin = 7;
const int reel3SensorPin = 4;
const int coinSensorPin = 12;
const int servoPin = 9;
const int startButtonPin = 2;   
const int blueLEDPin = 3;
const int whiteLEDPin = 5;      
const int coinTrayLEDPin = 6;
const int txPin = 11;
const int rxPin = 10;

// Game configuration
const float RTP = 96.0;          
const int SYMBOLS_PER_REEL = 10;   
const int STEPS_PER_SYMBOL = 320;  
const int FULL_REEL_STEPS = SYMBOLS_PER_REEL * STEPS_PER_SYMBOL;

// Symbol definitions
enum Symbol { CHERRY, LEMON, ORANGE, PLUM, BELL, BAR, WATERMELON, SEVEN };
const String SYMBOL_NAMES[] = {
  "CHERRY", "LEMON", "ORANGE", "PLUM", 
  "BELL", "BAR", "WATERMELON", "SEVEN"
};

// Payout table: {symbol1, symbol2, symbol3, payout}
const int PAYOUT_TABLE[][4] = {
  {BAR, BAR, BAR, 12},
  {SEVEN, SEVEN, SEVEN, 5},
  {WATERMELON, WATERMELON, WATERMELON, 3},
  {WATERMELON, WATERMELON, -1, 2},
  {BELL, BELL, BELL, 2},
  {BAR, BAR, -1, 1},
  {CHERRY, CHERRY, CHERRY, 1},
};
const int PAYOUT_TABLE_SIZE = sizeof(PAYOUT_TABLE) / sizeof(PAYOUT_TABLE[0]);

// Game variables
bool gameReady = false;
int currentCredits = 0;
Symbol reelPositions[3];   
Symbol targetPositions[3]; 

// Hardware objects
AccelStepper stepper1(AccelStepper::DRIVER, stepper1StepPin, directionPin);
AccelStepper stepper2(AccelStepper::DRIVER, stepper2StepPin, directionPin);
AccelStepper stepper3(AccelStepper::DRIVER, stepper3StepPin, directionPin);
Servo coinServo;
SoftwareSerial softwareSerial(rxPin, txPin);
DFRobotDFPlayerMini dfPlayer;

void setup() {
  pinMode(reel1SensorPin, INPUT);
  pinMode(reel2SensorPin, INPUT);
  pinMode(reel3SensorPin, INPUT);
  pinMode(coinSensorPin, INPUT);
  pinMode(startButtonPin, INPUT);
  pinMode(blueLEDPin, OUTPUT);
  pinMode(whiteLEDPin, OUTPUT);
  pinMode(coinTrayLEDPin, OUTPUT);

  Serial.begin(9600);
  softwareSerial.begin(9600);

  Serial.println("Initializing system...");

  if (!dfPlayer.begin(softwareSerial)) {
    Serial.println("DFPlayer error");
    while(true);
  }
  dfPlayer.volume(20);

  stepper1.setMaxSpeed(1000);
  stepper2.setMaxSpeed(1000);
  stepper3.setMaxSpeed(1000);
  stepper1.setAcceleration(500);
  stepper2.setAcceleration(500);
  stepper3.setAcceleration(500);

  coinServo.attach(servoPin);
  coinServo.write(0);

  digitalWrite(blueLEDPin, HIGH);
  delay(1000);
  digitalWrite(blueLEDPin, LOW);

  Serial.println("Calibrating reels...");
  calibrateReels();
}

void loop() {
  if (digitalRead(coinSensorPin) == HIGH) {
    currentCredits++;
    Serial.println("Coin inserted! Credits: " + String(currentCredits));
    digitalWrite(whiteLEDPin, HIGH);
    dfPlayer.play(5);
    while(digitalRead(coinSensorPin) == HIGH) delay(10);
    delay(100);
  }

  if (digitalRead(startButtonPin) == HIGH && gameReady && currentCredits > 0) {
    startGame();
    delay(500);
  }
}

void calibrateReels() {
  gameReady = false;
  digitalWrite(blueLEDPin, HIGH);

  calibrateReel(stepper1, reel1SensorPin, 1);
  calibrateReel(stepper2, reel2SensorPin, 2);
  calibrateReel(stepper3, reel3SensorPin, 3);

  for (int i = 0; i < 3; i++) reelPositions[i] = CHERRY;

  digitalWrite(blueLEDPin, LOW);
  digitalWrite(whiteLEDPin, HIGH);
  gameReady = true;
  Serial.println("Reel calibration completed.");
  Serial.println("Current credits: " + String(currentCredits));
}

void calibrateReel(AccelStepper &stepper, int sensorPin, int reelNumber) {
  Serial.println("Homing reel " + String(reelNumber));
  
  stepper.setMaxSpeed(300);
  stepper.setAcceleration(1000);
  
  while (digitalRead(sensorPin) == LOW) {
    stepper.move(100);
    stepper.run();
  }
  
  stepper.move(-20);
  while (stepper.distanceToGo() != 0) stepper.run();
  stepper.setCurrentPosition(0);
  
  Serial.println("Reel " + String(reelNumber) + " calibrated");
}

void startGame() {
  // Deduct one credit for the bet
  currentCredits--;
  digitalWrite(whiteLEDPin, LOW);
  dfPlayer.play(1);

  determineOutcome();
  spinAndStopReels();
  evaluateWin();

  digitalWrite(whiteLEDPin, HIGH);
  gameReady = true;
}

void determineOutcome() {
  bool shouldWin = (random(100) < RTP);
  int payout = 0;

  if (shouldWin) {
    int winIndex = random(PAYOUT_TABLE_SIZE);
    for (int i = 0; i < 3; i++) {
      targetPositions[i] = (Symbol)PAYOUT_TABLE[winIndex][i];
      if (targetPositions[i] == -1) 
        targetPositions[i] = (Symbol)random(SEVEN + 1);
    }
  } else {
    for (int i = 0; i < 3; i++)
      targetPositions[i] = (Symbol)random(SEVEN + 1);
  }

  Serial.print("Target combination: ");
  for (int i = 0; i < 3; i++) 
    Serial.print(SYMBOL_NAMES[targetPositions[i]] + " ");
  Serial.println();
}

void spinAndStopReels() {
  digitalWrite(blueLEDPin, HIGH);
  dfPlayer.play(2);

  // Adjusted rotations: 3,4,5 -> Reel 1 stops first
  setupReel(stepper1, targetPositions[0], 3);
  setupReel(stepper2, targetPositions[1], 4);
  setupReel(stepper3, targetPositions[2], 5);

  while (stepper1.isRunning() || stepper2.isRunning() || stepper3.isRunning()) {
    stepper1.run();
    stepper2.run();
    stepper3.run();
    
    // LED animation
    digitalWrite(blueLEDPin, (millis()/200) % 2 ? HIGH : LOW);
  }
  
  digitalWrite(blueLEDPin, LOW);
  updateReelPositions();
}

void setupReel(AccelStepper &stepper, Symbol target, int rotations) {
  long current = stepper.currentPosition();
  long targetSteps = target * STEPS_PER_SYMBOL;
  long distance = (targetSteps - current) + rotations * FULL_REEL_STEPS;
  
  stepper.moveTo(distance);
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(800);
}

void updateReelPositions() {
  reelPositions[0] = targetPositions[0];
  reelPositions[1] = targetPositions[1];
  reelPositions[2] = targetPositions[2];
}

void evaluateWin() {
  Serial.print("Result: ");
  for (int i = 0; i < 3; i++)
    Serial.print(SYMBOL_NAMES[reelPositions[i]] + " ");
  Serial.println();

  int payout = 0;
  for (int i = 0; i < PAYOUT_TABLE_SIZE; i++) {
    bool match = true;
    for (int j = 0; j < 3; j++) {
      if (PAYOUT_TABLE[i][j] != -1 && reelPositions[j] != PAYOUT_TABLE[i][j]) {
        match = false;
        break;
      }
    }
    if (match) {
      payout = PAYOUT_TABLE[i][3];
      break;
    }
  }

  if (payout > 0) {
    Serial.println("WIN! Payout: " + String(payout));
    dfPlayer.play(4);
    // WINNING COINS ARE DISPENSED ONLY, THEY ARE NOT ADDED TO currentCredits
    dispenseCoins(payout);
  } else {
    Serial.println("No win");
  }
}

void dispenseCoins(int coins) {
  for (int i = 0; i < coins; i++) {
    coinServo.write(90);
    delay(500);
    coinServo.write(0);
    delay(500);
    digitalWrite(coinTrayLEDPin, !digitalRead(coinTrayLEDPin));
  }
  digitalWrite(coinTrayLEDPin, LOW);
}
