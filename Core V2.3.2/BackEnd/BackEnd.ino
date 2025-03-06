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
USBConfig: Allows programatic changing of settings over USB

*/


//Settings
#define Version "1.2.0"
#define Hardware "2.3.2-LE"
#define MAX_DEVICES 10 //How many possible temperature sensors to scan for
#define OTA_URL "https://github.com/rit-construct-makerspace/access-control-firmware/releases/download/OTA/otadirectory.json"
#define TemperatureTime 5000 //How long to delay between temperature measurements, in milliseconds
#define FEPollRate 10000 //How long, in milliseconds, to go between an all-values poll of the frontend (in addition to event-based)
#define LEDFlashTime 150 //Time in milliseconds between aniimation steps of the LED when flashing or similar. 
#define LEDBlinkTime 5000 //Time in milliseconds between animation stepf of an LED when doing a slower blink indication.
#define BuzzerNoteTime 250 //Time in milliseconds between different tones
#define GMTOffset -18000 //Offset from GMT to local time, for Eastern time that's -1800 seconds, or -5 hours. 
#define DSTOffset 3600 //How much to offset time in daylight savings, usually 3600 seconds, or 1 hour.
#define NTP1 "pool.ntp.org" //The primary NTP server to check time against on startup.
#define NTP2 "time.nist.gov" //The secondary NTP server to check time against on startup.
#define KEYSWITCH_DEBOUNCE 150 //time in milliseconds between checks of the key switch, to help prevent rapid state changes.
#define InternalReadDebug 1 //Set to 0 to disable debug messages from the internal read, since it ends up spamming the terminal.

//Global Variables:
bool TemperatureUpdate;                  //1 when writing new information, to indicate other devices shouldn't read temperature-related info
uint64_t SerialNumbers[MAX_DEVICES];     //an array of uint64_t serial numbers of the devices. Size of the array is based on MAX_DEVICES 
float SysMaxTemp;                        //a float of the maximum temperature of the system
bool DebugPrinting;                      //1 when a thread is writing on debug serial
bool DebugMode = 1;                      //1 when debug data should be printed, always starts at 1. WARNING: Will plain-text print sensitive information!
byte NetworkMode = 1;                    //0 means WiFi only, 1 means WiFi or Ethernet, 2 means Ethernet only. TODO ethernet not implemented
String SecurityCode = "Shlug";           //Stores the password that needs to be verified before a JSON of new settings is loaded
String SSID = "";                        //The SSID that the wireless network will connect to.
String Password = "";                    //Stores the WiFi password
String Server = "https://make.rit.edu";  //The server address that API calls are made against
String Key;                              //Stores the API access key
String Zone;                             //Stores the area that a machine is deployed in.
int TempLimit;                           //The temperature above which the system shuts down and sends a warning.
int Frequency;                           //How often an update should be sent
String MachineID;                        //Unique identifier of the machine
String MachineType;                      //The kind of machine it is
byte ExpectedSwitchType;                 //The kind of swtich that should be attached on the analog detector.
bool NeedsWelcome;                       //1 if the system requires a sign-in before use. 
bool Button;                             //state of the frontend button.
bool Switch1;                            //State of card detect switch 1
bool Switch2;                            //state of card detect switch 2
bool Key1;                               //state of keyswitch input 1
bool Key2;                               //state of keyswitch input 2
String UID;                              //UID of the card detected in the system
bool CardPresent;                        //1 if a card has been detected and successfuly read.
bool ReadFailed;                         //set to 1 if a card was not read properly.
bool SendMessage;                        //set to 1 to indicate a message needs to be sent to the server
String Message;                          //String containing error message to report via API
bool ReadyToSend;                        //set to 1 to indicate "Message" is ready to send.
bool RegularStatus;                      //Used to indicate it is time to send the regular status message.
bool TemperatureStatus;                  //Set to 1 to indicate to send a temperature status report.
bool StartupStatus;                      //Set to 1 to indicate to send a report that the system has powered on and completed startup..
bool StartStatus;                        //Set to 1 to indicate to send a report that a session has started (card inserted and approved).
bool EndStatus;                          //Set to 1 to indicate to send a report that a session has ended (card removed).
bool ChangeStatus;                       //Set to 1 to iindicate to send a report that the machine's state has changed, outside of a session start/end.
String FEVer;                            //Stores the version number of the frontend.
unsigned long LastServer;                //Tracks the last time we had a good communication from the server
bool CheckNetwork;                       //Flag to indicate repeat network communication failures. Needs investigating.
unsigned long SessionTime;               //How long a user has been using a machine for
bool LogoffSent;                         //Flag to indicate that the system has sent the message to end a session, so data can be cleared.
String State = "Lockout";                //Plaintext indication of the state of the system for status reports. Idle, Unlocked, AlwayOn, or Lockout.
bool CardVerified;                       //Flag set to 1 once the results of the ID check are complete and CardStatus is valid.
bool CardStatus;                         //Set to 1 if a card is authorized to use the machine, 0 if not,
bool InternalVerified;                   //Set to 1 when the card has been verified against the internal list, and InternalStatus is valid.
bool InternalStatus;                     //Set to 1 if the card is authorized to use the machine based on the internal list.
bool PollCheck;                          //Flag indicating it is time for a regular frontend poll
bool NewLED;                             //Flag that there is new LED information to send
byte Red;                                //Red channel of LED
byte Green;                              //Green channel of LED
byte Blue;                               //Blue channel of LED
bool NewBuzzer;                          //Flag that there is new buzzer to send
int Tone;                                //Frequency of buzzer to play
bool Switch;                             //Value of the switch to sent to frontend
bool DebugLED;                           //Turns on/off the debugLED on the frontend
bool NoNetwork;                          //Indicates inability to connect to the server
bool TemperatureFault;                   //1 Indicates an overtemperature condition
bool NFCFault;                           //Flag indicates a failure to interface with the NFC reader.
bool CardUnread;                         //A card is preesnt in the machine but hasn't been read yet. 
bool VerifiedBeep;                       //Flag, set to 1 to sound a beep when a card is verified. Lets staff knw it is valid to use their key when the machine is in a non-idle state.
bool ReadError;                          //Flag, set 1 when we fail to read a card to flash lights/buzzers.

//Libraries:
#include <OneWireESP32.h>         //Version 2.0.2 | Source: https://github.com/junkfix/esp32-ds18b20
#include <Adafruit_PN532.h>       //Version 1.3.4 | Source: https://github.com/adafruit/Adafruit-PN532
#include <ArduinoJson.h>          //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.hpp>        //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
//WARNING: The original ESP32-OTA-Pull will not work with this code. Use the forked version linked below!
#include <ESP32OTAPull.h>         //Version 1.0.1 | Source: https://github.com/JimSHED/ESP32-OTA-Pull-GitHub
#include <WiFiClientSecure.h>     //Version 3.1.1 | Inherent to ESP32 Arduino
#include <HTTPClient.h>           //Version 3.1.1 | Inherent to ESP32 Arduino
#include <Preferences.h>          //Version 3.1.1 | Inherent to ESP32 Arduino
#include <esp_wifi.h>             //Version 3.1.1 | Inherent to ESP32 Arduino
#include <FS.h>                   //Version 3.1.1 | Inherent to ESP32 Arduino
#include <SPIFFS.h>               //Version 3.1.1 | Inherent to ESP32 Arduino
#include <time.h>                 //Version 3.1.1 | Inherent to ESP32 Arduino
#include <HTTPClient.h>           //Version 3.1.1 | Inherent to ESP32 Arduino
#include <Update.h>               //Version 3.1.1 | Inherent to ESP32 Arduino
#include <WiFi.h>                 //Version 3.1.1 | Inherent to ESP32 Arduino

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
Preferences settings;
WiFiClientSecure client;
HardwareSerial Internal(1);
HardwareSerial Debug(0);
JsonDocument usbjson;
HTTPClient http;

//Mutexes:
SemaphoreHandle_t DebugMutex; //Reserves the USB serial output, priamrily for debugging purposes.
SemaphoreHandle_t NetworkMutex; //Reserves the network connection
SemaphoreHandle_t OneWireMutex; //Reserves the OneWire connection. Currently only used for temperature measurement, eventually will measure system integrity.
SemaphoreHandle_t StateMutex; //Reserves the State string, since it takes a long time to change.

void setup(){
	delay(100);
  
  //Create mutexes:
  DebugMutex = xSemaphoreCreateMutex();
  NetworkMutex = xSemaphoreCreateMutex();
  OneWireMutex = xSemaphoreCreateMutex();
  StateMutex = xSemaphoreCreateMutex();

  Internal.begin(115200, SERIAL_8N1, TOESP, TOTINY);

  delay(5000); 

  //Set the LEDs blue for startup.
  Internal.println("L 0,0,255");

	Debug.begin(115200);
  Debug.println(F("STARTUP"));

  //First, load all settings from memory
  settings.begin("settings", false);
  SecurityCode = settings.getString("SecurityCode");
  if(SecurityCode == NULL){
    Debug.println(F("CAN'T FIND SETTINGS - FRESH INSTALL?"));
    Debug.println(F("HOLDING FOR UPDATE FOREVER..."));
    xTaskCreate(USBConfig, "USBConfig", 4096, NULL, 2, NULL);
    //Nuke the rest of this process - we can't do anything without our config.
    vTaskSuspend(NULL);
  }
  Password = settings.getString("Password");
  SSID = settings.getString("SSID");
  Server = settings.getString("Server");
  Key = settings.getString("Key");
  MachineID = settings.getString("MachineID");
  MachineType = settings.getString("MachineType");
  ExpectedSwitchType = settings.getString("SwitchType").toInt();
  Zone = settings.getString("Zone");
  TempLimit = settings.getString("TempLimit").toInt();
  Frequency = settings.getString("Frequency").toInt();
  NetworkMode = settings.getString("NetworkMode").toInt();
  NeedsWelcome = settings.getString("NeedsWelcome").toInt();
  DebugMode = settings.getString("DebugMode").toInt();

  if(NetworkMode != 2){
    if (DebugMode) {
      Debug.print("Attempting to connect to SSID: ");
      Debug.println(SSID);
    }
    //Wireless Initialization:
    if (Password != "null") {
      WiFi.mode(WIFI_STA);
      WiFi.begin(SSID, Password);
    } else {
      if (DebugMode) {
        Debug.println(F("Using no password."));
      }
      WiFi.begin(SSID);
    }

    //Maybe will fix refuse to connect issue?
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);

    if(DebugMode){
      Debug.print(F("Wireless MAC: ")); Debug.println(WiFi.macAddress());
      Debug.print(F("Ethernet MAC: ")); Debug.println(F("No ethernet on this board."));
    }


    //Attempt to connect to Wifi network:
    while (WiFi.status() != WL_CONNECTED) {
      Debug.print(".");
      //Add another dot every second...
      delay(1000);
    }
    if (DebugMode) {
      Debug.println("");
      Debug.print("Connected to ");
      Debug.println(SSID);
      Debug.print(F("Local IP: "));
      Debug.println(WiFi.localIP());
    }
  }

  client.setInsecure();


  //Then, check for an OTA update.
  if(DebugMode){
    Debug.println(F("Checking for OTA Updates..."));
    Debug.println(F("If any are found, will install immediately."));
  }
  ESP32OTAPull ota;
  ota.SetCallback(callback_percent);
  ota.SetConfig(Hardware);
  int otaresp = ota.CheckForOTAUpdate(OTA_URL, Version);
  if(DebugMode){
    Debug.print(F("OTA Response Code: ")); Debug.println(otaresp);
    Debug.println(errtext(otaresp));
    Debug.println(F("We're still here, so there must not have been an update."));
  }

  //Sync the time against an NTP Server
  //configTime(GMTOffset, DSTOffset, NTP1, NTP2);

  //Check to make sure we can connect to the NFC reader and it isn't damaged.
  pinMode(NFCPWR, OUTPUT);
  pinMode(NFCRST, OUTPUT);
  digitalWrite(NFCPWR, HIGH);
  digitalWrite(NFCRST, HIGH);
  delay(100);
  bool StartupFault;
  retry : 
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Debug.println("Didn't find PN532 board. Trying again...");
    delay(250);
    if(StartupFault){
      Internal.println(F("L 255,0,0"));
    } else{
      Internal.println(F("L 0,0,0"));
    }
    Internal.flush();
    StartupFault =! StartupFault;
    goto retry;
  }
  digitalWrite(NFCRST, LOW);
  digitalWrite(NFCPWR, LOW);
  Debug.println(F("NFC Setup"));

  //Load up the SPIFFS file of verified IDs, format/create if it is not there.
  if(!SPIFFS.begin(1)){ //Format SPIFFS if fails
    Serial.println("SPIFFS Mount Failed");
    while(1);
  }
  if(SPIFFS.exists("/validids.txt")){
    Debug.println(F("Valid ID List already exists."));
  } else{
    Debug.println(F("No Valid ID List file found. Creating..."));
    File file = SPIFFS.open("/validids.txt", "w");
    if(!file){
      Debug.println(F("ERROR: Unable to make file."));
    }
    file.print(F("Valid IDs:"));
    file.println();
    file.close();
  }

  /*
  TODO:
  There is a problem with the tasks below. Enabling too many of them causes WiFi to not work, and not all tasks are started properly.
  For example, when turning on down to LEDControl it works fine, but turning on BuzzerControl and MachineState doesn't create the MachineState task
  I do not know what is causing this and I'm not getting any error messages I can see. 
  Printing task usage shows 97% idle and most of the tasks, but not all of them.
  Stupid simple next step is to just start combining tasks and see what happens. No need to split into this many really.
  */

  //Lastly, create all tasks and begin operating normally.
  vTaskSuspendAll();
  xTaskCreate(RegularReport, "RegularReport", 1024, NULL, 6, NULL);
  xTaskCreate(Temperature, "Temperature", 2048, NULL, 5, NULL);
  xTaskCreate(USBConfig, "USBConfig", 2048, NULL, 5, NULL);
  xTaskCreate(InternalRead, "InternalRead", 2048, NULL, 5, NULL);
  xTaskCreate(ReadCard, "ReadCard", 2048, NULL, 5, NULL);
  xTaskCreate(StatusReport, "StatusReport", 5000, NULL, 2, NULL);
  xTaskCreate(VerifyID, "VerifyID", 4096, NULL, 1, NULL);
  xTaskCreate(InternalWrite, "InternalWrite", 2048, NULL, 5, NULL);
  xTaskCreate(LEDControl, "LEDControl", 1024, NULL, 5, NULL);
  xTaskCreate(BuzzerControl, "BuzzerControl", 1024, NULL, 5, NULL);
  xTaskCreate(MachineState, "MachineState", 2048, NULL, 5, NULL);
  xTaskCreate(NetworkCheck, "NetworkCheck", 4096, NULL, 3, NULL);
  //xTaskCreate(TimeManager, "TimeManager", 4096, NULL, 4, NULL);
  xTaskResumeAll();

  StartupStatus = 1;
  Debug.println(F("Setup done."));
}

void loop(){
  delay(50000);
}

void callback_percent(int offset, int totallength) {
  //Used to display percentage of OTA installation
  static int prev_percent = -1;
  int percent = 100 * offset / totallength;
  if (percent != prev_percent && DebugMode) {
    Debug.printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
    prev_percent = percent;
  }
}

const char *errtext(int code) {
  //Deciphers OTA code response
  switch (code) {
    case ESP32OTAPull::UPDATE_AVAILABLE:
      return "An update is available but wasn't installed";
    case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
      return "No profile matches";
    case ESP32OTAPull::NO_UPDATE_AVAILABLE:
      return "Profile matched, but update not applicable";
    case ESP32OTAPull::UPDATE_OK:
      return "An update was done, but no reboot";
    case ESP32OTAPull::HTTP_FAILED:
      return "HTTP GET failure";
    case ESP32OTAPull::WRITE_ERROR:
      return "Write error";
    case ESP32OTAPull::JSON_PROBLEM:
      return "Invalid JSON";
    case ESP32OTAPull::OTA_UPDATE_FAIL:
      return "Update fail (no OTA partition?)";
    default:
      if (code > 0)
        return "Unexpected HTTP response code";
      break;
  }
  return "Unknown error";
}