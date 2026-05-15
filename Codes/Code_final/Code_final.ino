#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Servo.h>
#include <SoftwareSerial.h>                 // Librairie Bluetooth
#include <SPI.h>                            // Librairie Potentiometre 
#include <stdlib.h>

//-------------------------------------------------//
//                                                 //
//                   DEFINITIONS                   //
//                                                 //
//-------------------------------------------------//

const int VCC = 5;

// --- RESISTANCES CIRCUIT ---
const float R1 = 100000 ;
float R_POTARD ;
const float R3 = 100000;
const float R5 = 10000 ;


// --- POTENTIOMETRE ---
#define MCP_NOP 0b00000000 //Commandes pour potentiomètre (non)
#define MCP_WRITE 0b00010001 //Commandes pour potentiomètre (écriture)
#define MCP_SHTDWN 0b00100001 //Commandes pour potentiomètre (shutdown)
const int ssMCPin = 10; // Define the slave select for the digital pot

// --- Bluetooth ---
#define txPin 7
#define rxPin 8
char serialRX[20];// variable de reception de donnée via RX
char serialTX; // variable de transmission de données via TX
const int speed_serial = 9600;      // communication Bluetooth 
SoftwareSerial mySerial (rxPin, txPin);

// --- Servo ---
Servo myservo;
const int SERVO_PIN = 6;

// --- Capteurs ---
const int FLEX_PIN      = A1;
const int GRAPHITE_PIN  = A0;
const float R_SHUNT     = 10000.0; // Flex 10kΩ
const float GAIN        = 1.0;     // Graphite amplifié

// --- Paramètres ---
int angleMax = 90;
int numSteps = 90;
int numMeasures = 1;
int parameterIndex = 0;
bool readyToStart = true;

//-------------------------------------------------//
//                                                 //
//                   CALIBRATION                   //
//                                                 //
//-------------------------------------------------//

// Fonction écriture d'une donnée dans le MCP IC connecté au ssPin
void SPIWrite(uint8_t cmd, uint8_t data, uint8_t ssPin) 
{
  SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0)); //https://www.arduino.cc/en/Reference/SPISettings
  
  digitalWrite(ssPin, LOW); // SS pin low to select chip
  
  SPI.transfer(cmd);        // Send command code
  SPI.transfer(data);       // Send associated value
  
  digitalWrite(ssPin, HIGH);// SS pin high to de-select chip
  SPI.endTransaction();
}

int Calibration() {
  int Pos_pot = 255; // valeur à entrer dans le potentiomètre pour faire varier sa résistance
  float Vmoy = 2.5; // Valeur de tension vers laquelle on veut tendre
  float Vgraphite; // tension lue en sortie de l'AO
  
  // Recherche type minimum entre 2,5 et Vgraphite
  float Min = 100; // valeur inaccessible
  float Pos_min; // pour retenir la consigne à donner au potentiomètre
  
  do {
    SPIWrite(MCP_WRITE, (uint8_t)Pos_pot, ssMCPin);
    
    delay(150);
    
    int rawADC = analogRead(GRAPHITE_PIN); // Première lecture pour "réveiller" l'ADC
    rawADC = analogRead(GRAPHITE_PIN);    // Deuxième lecture pour la précision
    
    Vgraphite = rawADC * (5.0 / 1023.0);
    
    Serial.print("Pot: "); Serial.print(Pos_pot);
    Serial.print(" -> V: "); Serial.println(Vgraphite);

    if (Pos_pot <= 1) break; // Sécurité pour éviter l'underflow
    if (abs(Vmoy - Vgraphite) < Min) {
      Min = abs(Vmoy - Vgraphite);
      Pos_min = Pos_pot;
    }
    Pos_pot -= 1;
  } while (Pos_pot > 0 && Pos_pot <= 255);

  SPIWrite(MCP_WRITE, (uint8_t)Pos_min, ssMCPin);

  R_POTARD = (Pos_min*10000.0)/255.0;
  Serial.print("Commande min : ") ; Serial.println(Pos_min);
  Serial.print("Résistance R2 correspondante : ") ; Serial.println(R_POTARD);
  return (R_POTARD);
}

//-------------------------------------------------//
//                                                 //
//                  CYCLE MESURE                   //
//                                                 //
//-------------------------------------------------//

// --- Cycle servo + mesure ---
void runCycle(){
  Serial.println("START"); // signal pour Python

  float angleStep = float(angleMax)/float(numSteps);
  int stepDelay = 1000; // ms

  for(int i=0; i<=numSteps; i++){
    int angle = round(i*angleStep);
    myservo.write(angle);
    delay(stepDelay);

    float sumFlex = 0;
    float sumGraphite = 0;

    for(int m=0; m<numMeasures; m++){
//      if(sensorMode == FLEX_ONLY || sensorMode == BOTH){
        int adcFlex = analogRead(FLEX_PIN);
        float Rflex = R_SHUNT * (1023.0/float(adcFlex) - 1.0);
        sumFlex += Rflex;
//      }
//      if(sensorMode == GRAPHITE_ONLY || sensorMode == BOTH){
        int adcGraphite = analogRead(GRAPHITE_PIN);
        float Vgraphite = adcGraphite * (5.0 / 1024.0);
        float Rgraphite = (1+(R3/R_POTARD))*R1*(VCC/Vgraphite)-R1-R5 ;
        sumGraphite += Rgraphite;
//      }
    }

    float avgFlex = /*(sensorMode == FLEX_ONLY || sensorMode == BOTH) ?*/ sumFlex/float(numMeasures) /*: 0*/;
    float avgGraphite = /*(sensorMode == GRAPHITE_ONLY || sensorMode == BOTH) ?*/ sumGraphite/float(numMeasures) /*: 0*/;

    String Data = String(avgFlex) + ";" + String(avgGraphite) ;
    mySerial.println("D: " + Data);

    Serial.print("Flex --> "); Serial.println(avgFlex); 
    Serial.print("Graph --> "); Serial.println(avgGraphite);     
  }

  // --- Retour ---
  for(int i=numSteps; i>=0; i--){
    int angle = round(i*angleStep);
    myservo.write(angle);
    delay(stepDelay);

    float sumFlex = 0;
    float sumGraphite = 0;

    for(int m=0; m<numMeasures; m++){
      int adcFlex = analogRead(FLEX_PIN);
      float Rflex = R_SHUNT * (1023.0/float(adcFlex) - 1.0);
      sumFlex += Rflex;
      int adcGraphite = analogRead(GRAPHITE_PIN);
      float Vgraphite = adcGraphite * (5.0 / 1024.0);
      float Rgraphite = (1+(R3/R_POTARD))*R1*(VCC/Vgraphite)-R1-R5 ;
      sumGraphite += Rgraphite;
    }

    float avgFlex = sumFlex/float(numMeasures);
    float avgGraphite = sumGraphite/float(numMeasures);

    String Data = String(avgFlex) + ";" + String(avgGraphite) ;
    mySerial.println("D: " + Data);
  }
  Serial.println("END"); // fin cycle
}

//-------------------------------------------------//
//                                                 //
//                      SETUP                      //
//                                                 //
//-------------------------------------------------//

void setup() {
  Serial.begin(9600);

  // Bluetooth
  mySerial.begin(speed_serial); // initialisation de la connexion série (avec le module bluetooth)
  pinMode (rxPin,INPUT);
  pinMode (txPin,OUTPUT);

  // Servo
  myservo.attach(SERVO_PIN);
  myservo.write(0);

  // Potentiomètre
  pinMode (ssMCPin, OUTPUT); //select pin output
  digitalWrite(ssMCPin, HIGH); //SPI chip disabled
  SPI.begin(); 
}

//-------------------------------------------------//
//                                                 //
//                       MAIN                      //
//                                                 //
//-------------------------------------------------//
int resultat; // valeur de la position du potentiomètre pour mettre la valeur de la tension à 2,5V

void loop() {
  // Savoir si une donnée est disponible
  if (mySerial.available() > 0) {

    //Attendre un peu (pour éviter les bugs)
    delay(10);

    int len = mySerial.readBytesUntil('\n', serialRX, sizeof(serialRX) - 1);
    serialRX[len] = '\0';  // fin de chaîne obligatoire
 
    // Afficher ce qu'on a reçu
    //Serial.print("Recu : ");
    //Serial.println(serialRX);
 
    // Case pour faire des actions selon la valeur reçue
    switch (serialRX[0]) {
      case '1':   // Si nombre 1 reçu : calibration
        Serial.println("Calibration lancée");
        resultat = Calibration();
        char buffer[20];
        sprintf(buffer, "C:%d", resultat);
        Serial.println(buffer);
        mySerial.println(buffer);
        break;

      case '2': // Si nombre 2 reçu : lancer la mesure
        Serial.println("Mesure en cours");
        // --- Lancement cycle ---
        if(readyToStart){
          runCycle();
        }
        break;

      case 'A':
        angleMax = atoi(serialRX + 1);
        Serial.print("Nouvelle valeur d'angle : "); Serial.println(angleMax);
        break;

      case 'M':
        numMeasures = atoi(serialRX + 1);
        Serial.print("Nouvelle valeur d'mesure : "); Serial.println(numMeasures);
        break;

      case 'P':
        numSteps = atoi(serialRX + 1);
        Serial.print("Nouvelle valeur d'pas : "); Serial.println(numSteps);
        break;
    }
  }
  delay(5);
}
