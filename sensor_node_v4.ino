/**
 * Sensor Node Program
 * CITAS Dryad 2018
 * 
 * Dependencies
 *  Libraries
 *    - DallasTemperature
 *    - DHT
 *    - OneWire
 *  Boards
 *    - Adafruit SAMD
*/

/***********************/
/*      libraries      */
/***********************/
#include "Adafruit_SleepyDog.h"
#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>
#include <string.h>
#include <stdio.h>

// Sensor node specific libraries
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Debugging settings
#define DEBUG_MSGS_ON
#if defined(DEBUG_MSGS_ON)
#define DBG_PRINT(x)    Serial.print(x)
#define DBG_PRINTLN(x)  Serial.println(x)

#else
#define DBG_PRINT(x)    NULL;
#define DBG_PRINTLN(x)  NULL;

#endif

/*************************/
/*       Definitions     */
/*************************/
// Identifier
#define ID_SEN_NODE      7

// Sensor pins
#define PIN_SOIL_TEMP     12
#define PIN_HUM_AIR_TEMP  6
#define PIN_PH            A0
#define PIN_LIGHT         A3
#define PIN_MOISTURE      A2

// Sensor value offsets
#define OFFSET_PH         0.00 // To be calibrated
#define OFFSET_TEMP       0.00 // To be calibrated
#define MOISTURE_WET      343
#define MOISTURE_DRY      673  

// Adafruit FONA
#define FONA_RX           9
#define FONA_TX           8
#define FONA_RST          4
#define FONA_RI           7
#define LED               13

#define DEBUG_MODE

// Battery pin definition
#define PIN_VBAT          A7

#define RESET             5
int dtrPin = RESET;

// We default to using software serial. If you want to use hardware serial
// (because softserial isnt supported) comment out the following lines 
// and uncomment the HardwareSerial line
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

//Hardware serial is also possible!
//HardwareSerial *fonaSerial = &Serial1;

// Use this for FONA 800 and 808s
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

// Packet struct
struct tDataPayload_t{
  int uNodeId;
  int uVBatt;
  float uPH;
  float uLight;
  float uTempAir;
  float uTempSoil;
  float uHumidity;
  float uMoisture;
  char uTimestamp[23];
};

/*************************/
/* Function Declarations */
/*************************/
void setup();
void loop();

/* Global Variable Declarations */
char ecceTestNumber[20] = "09672416754";
//test number
char csTestNumber[20] = "21584515";
//prod number
//char csTestNumber[20] = "21587283";

OneWire soilTempWire(PIN_SOIL_TEMP);
DallasTemperature _soilTemp(&soilTempWire);

DHT _humAirTemp (PIN_HUM_AIR_TEMP, DHT22);

struct tDataPayload_t _tDataPayload;

uint8_t type;

void setup() {
  // put your setup code here, to run once:
  //while (!Serial); 
  
  Serial.begin(115200);
  
  pinMode(dtrPin, OUTPUT);
  digitalWrite(dtrPin, LOW);
  delay(3000);
  // FONA Initialization
  Serial.println("Initializing....(May take 3 seconds)");

  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    Serial.println("Couldn't find FONA");
    while(1);
  }
  type = fona.type();
  Serial.println("FONA is OK");
  Serial.print("Found ");

  // Assign LED pin mode to Output
  pinMode(LED, OUTPUT);

  // Display sensor ID
  DBG_PRINT("Self address @"); DBG_PRINTLN(ID_SEN_NODE);
  
  delay(3000);

  Serial.println("starting program");
  digitalWrite(LED, HIGH); // Turn LED on to specify the board is awake

  // put your main code here, to run repeatedly:

  Serial.println("Getting parameters...");
  getParameters();
  delay(1000);
  
  Serial.println("Sending Values...");
  sendParameters(ecceTestNumber, csTestNumber, _tDataPayload);
  delay(20000);
  
  setFonaPowerDownMode();
  
  // Put microcontroller to sleep
  // Set to approximately 30 minutes
  int number_of_sleeper_loops = 225; //time between taking a reading is 8 * 8 seconds approx 1 minute.
  for (int i = 0; i < number_of_sleeper_loops; i++) {
    Watchdog.sleep(8000);
  }

  Watchdog.disable();

  // Finally demonstrate the watchdog resetting by enabling it for a shorter
  // period of time and waiting a long time without a reset.  Notice you can
  // pass a _maximum_ countdown time (in milliseconds) to the enable call.
  // The library will try to use that value as the countdown, but it might
  // pick a smaller value if the hardware doesn't support it.  The actual
  // countdown value will be returned so you can see what it is.
  int countdownMS = Watchdog.enable(-4000);
  delay(5000);
  
 
}

void loop() {
  //
  // There is nothing here because the Feather 32U4 resets after setup due to
  // watchdog reset.
  //
}

void getParameters() {
  // enable network time sync for the timestamp
  if (!fona.enableNetworkTimeSync(true))
    Serial.println("Failed to enable");
  // read time
  char buffer[23];
  fona.getTime(buffer, 23);

  // get battery charge in mV
  int vbatt;
  if (! fona.getBattVoltage(&vbatt)) {
    Serial.println("Failed to read Batt");
  }
  
  _soilTemp.requestTemperatures();

  _tDataPayload.uNodeId        = ID_SEN_NODE;
  _tDataPayload.uVBatt          = vbatt;
  _tDataPayload.uPH            = (analogRead(PIN_PH) / 100.00) + OFFSET_PH;
  _tDataPayload.uLight         = analogRead(PIN_LIGHT) * 10;
  _tDataPayload.uTempAir       = _humAirTemp.readTemperature();
  _tDataPayload.uTempSoil      = _soilTemp.getTempCByIndex(0) + OFFSET_TEMP;
  _tDataPayload.uHumidity      = _humAirTemp.readHumidity();
  

  //-------------------------------------------------------------
  // Get median Moisture reading
  //-------------------------------------------------------------
   int moisture_value[10];
   for (int i=0; i <= 10; i++) {
      //moisture_value[i] = analogRead(PIN_MOISTURE);
      moisture_value[i] = map(analogRead(PIN_MOISTURE), MOISTURE_WET, MOISTURE_DRY, 0, 100);
   }

   //perform bubble sort
   int out, in, swapper;
    for(out = 0 ; out <= 10; out++) {
      for(in = out; in < 10; in++)  {
        if( moisture_value[in] > moisture_value[in+1] ) {
          // swap them:
          swapper = moisture_value[in];
          moisture_value[in] = moisture_value[in+1];
          moisture_value[in+1] = swapper;
        }
      }
    }

   // get median
   _tDataPayload.uMoisture   = moisture_value[5];
  
  strcpy(_tDataPayload.uTimestamp, buffer);
}

void sendParameters(char sNumECCE, char sNumCS, struct tDataPayload_t _tDataPayload){
  char nodeID[3]        = "";
  char battValue[7]       = "";
  char phValue[7]       = "";
  char lightValue[10]    = "";
  char airtempValue[7]  = "";   
  char humidityValue[7] = "";
  char soiltempValue[7] = "";
  char moistureValue[5] = "";
  char message[200]     = "";

  // Convert values to char
  //sprintf(nodeID, "%d", _tDataPayload.uNodeId);
  sprintf(nodeID, "%d", ID_SEN_NODE );
  sprintf(battValue, "%d", _tDataPayload.uVBatt);
  dtostrf(_tDataPayload.uPH, 3, 2, phValue);
  dtostrf(_tDataPayload.uLight, 3, 2, lightValue);
  dtostrf(_tDataPayload.uTempAir, 3, 2, airtempValue);
  dtostrf(_tDataPayload.uHumidity, 3, 2, humidityValue);
  dtostrf(_tDataPayload.uTempSoil, 3, 2, soiltempValue);
  dtostrf(_tDataPayload.uMoisture, 3, 2, moistureValue);
  
  strcat(message, "POST ");
  strcat(message, "7");
  strcat(message, " vBatt ");
  strcat(message, battValue);
  strcat(message, " PH ");
  strcat(message, phValue);
  strcat(message, " LI ");
  strcat(message, lightValue);
  strcat(message, " AT ");
  strcat(message, airtempValue);
  strcat(message, " HU ");
  strcat(message, humidityValue);
  strcat(message, " ST ");
  strcat(message, soiltempValue);
  strcat(message, " MO ");
  strcat(message, moistureValue);
  strcat(message, " ");
  strcat(message, _tDataPayload.uTimestamp);
  
  Serial.println(message);

  // Send Values through SMS
  if (!fona.sendSMS(csTestNumber, message)) {
    Serial.println("Sending Failed");
  } else {
    Serial.println("Value Sent to citas.tugon!");
  }

  delay(20000);

  if (!fona.sendSMS(ecceTestNumber, message)) {
    Serial.println("Sending Failed");
  } else {
    Serial.println("Value Sent to ECCE back-up!");
  }
}




//
// Power down fona
//

void setFonaPowerDownMode(void) {
 digitalWrite(dtrPin, HIGH);
 delay(200);
  digitalWrite(dtrPin, LOW);
  delay(2000);
  digitalWrite(dtrPin, HIGH);
  delay(3000);

  
  Serial.println("powerdown fona");
  //fonaSS.begin(4800);
  //fonaSS.println(F("AT+CFUN=0")); //Sets SIM800 module to minimum functionality mode
  digitalWrite(LED, LOW);
}


void setFonaWakeUpMode(void){
  
  Serial.println("initiating wake");
  digitalWrite(dtrPin, HIGH);
  delay(200);
  digitalWrite(dtrPin, LOW);
  delay(2000);
  digitalWrite(dtrPin, HIGH);
  delay(10000);


//  if (! fona.begin(*fonaSerial)) {
//     Serial.println(F("Couldn't find FONA")); }

  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    Serial.println("Couldn't find FONA");
    while(1);
  }
  type = fona.type();
  Serial.println("FONA is OK");
  Serial.print("Found ");

  
  //fonaSS.begin(4800);
  //fonaSS.println(F("AT+CFUN=1")); //Sets SIM800 module to full functionality mode
  digitalWrite(LED, HIGH);
}
