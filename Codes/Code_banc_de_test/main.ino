#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Servo.h>

// --- OLED 128x64 ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Encodeur ---
const int ENC_CLK = 2;
const int ENC_DT  = 4;
const int ENC_SW  = 5;

// --- Servo ---
Servo myservo;
const int SERVO_PIN = 6;

// --- Capteurs ---
const int FLEX_PIN      = A1;
const int GRAPHITE_PIN  = A0;
const float R_SHUNT     = 10000.0; // Flex 10kΩ
const float GAIN        = 1.0;     // Graphite amplifié

// --- Encodeur variables ---
volatile int encoderPos = 0;
int lastCLK = LOW;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// --- Paramètres ---
int angleMax = 90;
int numSteps = 90;
int numMeasures = 1;
int parameterIndex = 0;
bool readyToStart = false;

// --- Menu capteur ---
enum SensorMode { FLEX_ONLY, GRAPHITE_ONLY, BOTH };
SensorMode sensorMode = FLEX_ONLY;

void setup() {
  Serial.begin(115200);

  // OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){ while(1); }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Encodeur
  pinMode(ENC_CLK, INPUT);
  pinMode(ENC_DT, INPUT);
  pinMode(ENC_SW, INPUT_PULLUP);
  lastCLK = digitalRead(ENC_CLK);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), handleEncoder, CHANGE);

  // Servo
  myservo.attach(SERVO_PIN);
  myservo.write(0);

  displayParameters();
}

void loop() {
  // --- Bouton ---
  bool reading = digitalRead(ENC_SW);
  if(lastButtonState == HIGH && reading == LOW){
    if(millis() - lastDebounceTime > debounceDelay){
      lastDebounceTime = millis();
      parameterIndex++;
      if(parameterIndex > 3){
        readyToStart = true;
        parameterIndex = 0;
      }
      displayParameters();
    }
  }
  lastButtonState = reading;

  // --- Appliquer rotation encodeur ---
  noInterrupts();
  int pos = encoderPos;
  encoderPos = 0;
  interrupts();

  if(pos != 0){
    switch(parameterIndex){
      case 0: // angleMax
        angleMax += pos;
        if(angleMax < 1) angleMax = 1;
        if(angleMax > 180) angleMax = 180;
        numSteps = angleMax;
        break;
      case 1: // nombre de pas
        numSteps += pos;
        if(numSteps < 1) numSteps = 1;
        if(numSteps > 500) numSteps = 500;
        break;
      case 2: // numMeasures
        numMeasures += pos;
        if(numMeasures < 1) numMeasures = 1;
        if(numMeasures > 20) numMeasures = 20;
        break;
      case 3: // choix capteur
        int mode = int(sensorMode) + pos;
        if(mode < 0) mode = 2;
        if(mode > 2) mode = 0;
        sensorMode = SensorMode(mode);
        break;
    }
    displayParameters();
  }

  // --- Lancement cycle ---
  if(readyToStart){
    displayMessage("Cycle démarré...");
    runCycle();
    displayMessage("Cycle terminé");
    readyToStart = false;
    parameterIndex = 0;
    displayParameters();
  }

  delay(5);
}

// --- Interrupt encodeur ---
void handleEncoder(){
  int clkState = digitalRead(ENC_CLK);
  int dtState  = digitalRead(ENC_DT);

  if(clkState != lastCLK){
    if(dtState != clkState) encoderPos++;
    else encoderPos--;
  }
  lastCLK = clkState;
}

// --- Affichage OLED ---
void displayParameters(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("Réglage parametres:"));
  
  display.print(F("Angle max: ")); display.println(angleMax);
  if(parameterIndex==0) display.println(F("<--"));
  
  display.print(F("Nombre de pas: ")); display.println(numSteps);
  if(parameterIndex==1) display.println(F("<--"));
  
  display.print(F("Mesures/angle: ")); display.println(numMeasures);
  if(parameterIndex==2) display.println(F("<--"));

  display.print(F("Capteur: "));
  switch(sensorMode){
    case FLEX_ONLY: display.println(F("Flex")); break;
    case GRAPHITE_ONLY: display.println(F("Graphite")); break;
    case BOTH: display.println(F("Les deux")); break;
  }
  if(parameterIndex==3) display.println(F("<--"));

  if(readyToStart){
    display.println();
    display.println(F("PRÊT A LANCER"));
  }
  display.display();
}

void displayMessage(String msg){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(msg);
  display.display();
}

// --- Cycle servo + mesure ---
void runCycle(){
  Serial.println("START"); // signal pour Python

  float angleStep = float(angleMax)/float(numSteps);
  int stepDelay = 25; // ms

  for(int i=0; i<=numSteps; i++){
    int angle = round(i*angleStep);
    myservo.write(angle);
    delay(stepDelay);

    float sumFlex = 0;
    float sumGraphite = 0;

    for(int m=0; m<numMeasures; m++){
      if(sensorMode == FLEX_ONLY || sensorMode == BOTH){
        int adcFlex = analogRead(FLEX_PIN);
        float Rflex = R_SHUNT * (1023.0/float(adcFlex) - 1.0);
        sumFlex += Rflex;
      }
      if(sensorMode == GRAPHITE_ONLY || sensorMode == BOTH){
        int adcGraphite = analogRead(GRAPHITE_PIN);
        float Rgraphite = float(adcGraphite) * GAIN;
        sumGraphite += Rgraphite;
      }
    }

    float avgFlex = (sensorMode == FLEX_ONLY || sensorMode == BOTH) ? sumFlex/float(numMeasures) : 0;
    float avgGraphite = (sensorMode == GRAPHITE_ONLY || sensorMode == BOTH) ? sumGraphite/float(numMeasures) : 0;

    Serial.print("A"); Serial.print(angle);
    if(sensorMode == FLEX_ONLY) Serial.print("\t"), Serial.println(avgFlex);
    else if(sensorMode == GRAPHITE_ONLY) Serial.print("\t"), Serial.println(avgGraphite);
    else Serial.print("\t"), Serial.print(avgFlex), Serial.print("\t"), Serial.println(avgGraphite);
  }

  // --- Retour ---
  for(int i=numSteps; i>=0; i--){
    int angle = round(i*angleStep);
    myservo.write(angle);
    delay(stepDelay);

    float sumFlex = 0;
    float sumGraphite = 0;

    for(int m=0; m<numMeasures; m++){
      if(sensorMode == FLEX_ONLY || sensorMode == BOTH){
        int adcFlex = analogRead(FLEX_PIN);
        float Rflex = R_SHUNT * (1023.0/float(adcFlex) - 1.0);
        sumFlex += Rflex;
      }
      if(sensorMode == GRAPHITE_ONLY || sensorMode == BOTH){
        int adcGraphite = analogRead(GRAPHITE_PIN);
        float Rgraphite = float(adcGraphite) * GAIN;
        sumGraphite += Rgraphite;
      }
    }

    float avgFlex = (sensorMode == FLEX_ONLY || sensorMode == BOTH) ? sumFlex/float(numMeasures) : 0;
    float avgGraphite = (sensorMode == GRAPHITE_ONLY || sensorMode == BOTH) ? sumGraphite/float(numMeasures) : 0;

    Serial.print("R"); Serial.print(angle);
    if(sensorMode == FLEX_ONLY) Serial.print("\t"), Serial.println(avgFlex);
    else if(sensorMode == GRAPHITE_ONLY) Serial.print("\t"), Serial.println(avgGraphite);
    else Serial.print("\t"), Serial.print(avgFlex), Serial.print("\t"), Serial.println(avgGraphite);
  }

  Serial.println("END"); // fin cycle
}