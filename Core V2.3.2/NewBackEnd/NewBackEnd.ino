#include <FS.h>
#include <SPIFFS.h>

/*

----- Access Control Core ----

For hardware version 2.3.2

By Jim Heaney, RIT SHED Makerspace
February 2025

Licensed CC-BY-NC-SA 4.0

More information; https://github.com/rit-construct-makerspace/access-control-firmware

------

FUNCTIONS:
InternalWrite: Handles writing updated information to the frontend MCU
InternalRead: Handles reading incoming messages from the frontend MCU
ReadCard: Manages the NFC readers
VerifyID: Checks if an installed ID is permitted to use this equipment
StatusReport: Sends status messages regularly as well as for specific events
Temperature: Monitors all temperature sensors for both temperature as well as unique device IDs
LEDControl: Sets the LED color based on system state, handles animations
BuzzerControl: Sets the buzzer based on system state, handles tone sequences
MachineState: Handles the state of the machine and activations based on input flags

*/


//Settings
#define Version 1.2.0
#define Hardware 2.3.2-LE
#define MAX_DEVICES 10 //How many possible temperature sensors to scan for

//Global Variables:
bool TemperatureUpdate; //1 when writing new information, to indicate other devices shouldn't read temperature-related info
uint64_t SerialNumbers[MAX_DEVICES]; //an array of uint64_t serial numbers of the devices. Size of the array is based on MAX_DEVICES 
float SysMaxTemp; //a float of the maximum temperature of the system
bool DebugPrinting; //1 when a thread is writing on debug serial
bool DebugMode; //1 when debug data should be printed
unsigned long TemperatureTime; //How long to delay between temperature measurements

//Libraries:
#include <OneWireESP32.h>         //Version 2.0.2 | Source: https://github.com/junkfix/esp32-ds18b20
#include <Adafruit_PN532.h>       //Version 1.3.4 | Source: https://github.com/adafruit/Adafruit-PN532
#include <ArduinoJson.h>          //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.hpp>        //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
#include <NetworkClientSecure.h>  //Version 3.1.1 | Inherent to ESP32 Arduino
#include <HTTPClient.h>           //Version 3.1.1 | Inherent to ESP32 Arduino
#include <Preferences.h>          //Version 3.1.1 | Inherent to ESP32 Arduino
#include <esp_wifi.h>             //Version 3.1.1 | Inherent to ESP32 Arduino
#include <FS.h>                   //Version 3.1.1 | Inherent to ESP32 Arduino
#include <SPIFFS.h>               //Version 3.1.1 | Inherent to ESP32 Arduino

//Pin Definitions:
const int ETHINT = 13;
const int DB9INT = 37;
const int ETHLEDS = 40;
const int SWTYPE = 2;
const int TEMP = 1;
const int DIP3 = 12;
const int DIP2 = 11;
const int DIP1 = 10;
const int TOTINY = 9;
const int ETHCS = 3;
const int ETHRST = 20;
const int TOESP = 19;
const int NFCPWR = 8;
const int SCKPin = 17;
const int MISOPin = 16;
const int DEBUGLED = 15;
const int MOSIPin = 7;
const int NFCCS = 6;
const int NFCIRQ = 5;
const int NFCRST = 4;

//Objects:
Adafruit_PN532 nfc(SCKPin, MISOPin, MOSIPin, NFCCS);
JsonDocument doc;
Preferences settings;
NetworkClientSecure client;
HardwareSerial Internal(1);
HardwareSerial Debug(0);

void setup(){
	delay(1000);
	Debug.begin(115200);
  DebugMode = 1; //Temporary
  xTaskCreate(Temperature, "Temperature", 2048, NULL, 2, NULL);
}

void loop(){

}