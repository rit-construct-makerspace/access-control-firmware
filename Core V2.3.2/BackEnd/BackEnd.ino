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
#define Version "1.2.6"
#define Hardware "2.3.2-LE"
#define MAX_DEVICES 10 //How many possible temperature sensors to scan for
#define OTA_URL "https://github.com/rit-construct-makerspace/access-control-firmware/releases/latest/download/otadirectory.json"
#define TemperatureTime 5000 //How long to delay between temperature measurements, in milliseconds
#define FEPollRate 10000 //How long, in milliseconds, to go between an all-values poll of the frontend (in addition to event-based)
#define LEDFlashTime 150 //Time in milliseconds between aniimation steps of the LED when flashing or similar. 
#define LEDBlinkTime 3000 //Time in milliseconds between animation stepf of an LED when doing a slower blink indication.
#define BuzzerNoteTime 250 //Time in milliseconds between different tones
#define KEYSWITCH_DEBOUNCE 150 //time in milliseconds between checks of the key switch, to help prevent rapid state changes.
#define InternalReadDebug 0 //Set to 0 to disable debug messages from the internal read, since it ends up spamming the terminal.

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
bool WritingMessage;                     //set to 1 to indicate a message is being written, for eventual sending to the server.
String Message;                          //String containing error message to report via API
bool ReadyToSend;                        //set to 1 to indicate "Message" is ready to send.
bool RegularStatus;                      //Used to indicate it is time to send the regular status message.
bool TemperatureStatus;                  //Set to 1 to indicate to send a temperature status report.
bool StartupStatus;                      //Set to 1 to indicate to send a report that the system has powered on and completed startup..
bool StartStatus;                        //Set to 1 to indicate to send a report that a session has started (card inserted and approved).
bool EndStatus;                          //Set to 1 to indicate to send a report that a session has ended (card removed).
bool ChangeStatus;                       //Set to 1 to iindicate to send a report that the machine's state has changed, outside of a session start/end.
String FEVer;                            //Stores the version number of the frontend.
uint64_t LastServer;                     //Tracks the last time we had a good communication from the server
bool CheckNetwork;                       //Flag to indicate repeat network communication failures. Needs investigating.
uint64_t SessionStart;                   //Time in millis when a session started.
uint64_t SessionTime;                    //How long a user has been using a machine for
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
bool Fault;                              //Flag indicates a failure of some sort with the system other than temperature. 
bool CardUnread;                         //A card is preesnt in the machine but hasn't been read yet. 
bool VerifiedBeep;                       //Flag, set to 1 to sound a beep when a card is verified. Lets staff knw it is valid to use their key when the machine is in a non-idle state.
bool ReadError;                          //Flag, set 1 when we fail to read a card to flash lights/buzzers.
bool NoBuzzer;                           //Flag, set to 1 to mute the buzzer.

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
#include <Update.h>               //Version 3.1.1 | Inherent to ESP32 Arduino
#include <WiFi.h>                 //Version 3.1.1 | Inherent to ESP32 Arduino
#include "esp_timer.h"            //Version 3.1.1 | Inherent to ESP32 Arduino
#include "esp32s2/rom/rtc.h"      //Version 3.1.1 | Inherent to ESP32 Arduino

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
JsonDocument usbjson;
HTTPClient http;
ESP32OTAPull ota;
TaskHandle_t xHandle;
TaskHandle_t xHandle2;

//Mutexes:
SemaphoreHandle_t DebugMutex; //Reserves the USB serial output, priamrily for debugging purposes.
SemaphoreHandle_t NetworkMutex; //Reserves the network connection
SemaphoreHandle_t OneWireMutex; //Reserves the OneWire connection. Currently only used for temperature measurement, eventually will measure system integrity.
SemaphoreHandle_t StateMutex; //Reserves the State string, since it takes a long time to change.
SemaphoreHandle_t MessageMutex; //Reserves the ability to send a message.

void setup(){
	delay(100);
  
  //Create mutexes:
  DebugMutex = xSemaphoreCreateMutex();
  NetworkMutex = xSemaphoreCreateMutex();
  OneWireMutex = xSemaphoreCreateMutex();
  StateMutex = xSemaphoreCreateMutex();
  MessageMutex = xSemaphoreCreateMutex();

  Internal.begin(115200, SERIAL_8N1, TOESP, TOTINY);

  xTaskCreate(GamerMode, "GamerMode", 2048, NULL, 5, &xHandle2);

  delay(5000); 

	Serial.begin(115200);
  Serial.println(F("STARTUP"));

  //First, load all settings from memory
  settings.begin("settings", false);
  SecurityCode = settings.getString("SecurityCode");
  if(SecurityCode == NULL){
    Serial.println(F("CAN'T FIND SETTINGS - FRESH INSTALL?"));
    Serial.println(F("HOLDING FOR UPDATE FOREVER..."));
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
  NoBuzzer = settings.getString("NoBuzzer").toInt();

  if(NetworkMode != 2){
    if (DebugMode) {
      Serial.print("Attempting to connect to SSID: ");
        Serial.println(SSID);
    }
    //Wireless Initialization:
    if (Password != "null") {
      WiFi.mode(WIFI_STA);
      WiFi.begin(SSID, Password);
    } else {
      if (DebugMode) {
        Serial.println(F("Using no password."));
      }
      WiFi.begin(SSID);
    }

    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);

    if(DebugMode){
      Serial.print(F("Wireless MAC: ")); Serial.println(WiFi.macAddress());
      Serial.print(F("Ethernet MAC: ")); Serial.println(F("No ethernet on this board."));
    }


    //Attempt to connect to Wifi network:
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      //Add another dot every second...
      delay(1000);
    }
    if (DebugMode) {
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(SSID);
      Serial.print(F("Local IP: "));
      Serial.println(WiFi.localIP());
    }
  }

  client.setInsecure();
  http.setReuse(true);

  //Then, check for an OTA update.
  if(DebugMode){
    Serial.println(F("Checking for OTA Updates..."));
    Serial.println(F("If any are found, will install immediately."));
  }
  ota.SetCallback(callback_percent);
  ota.SetConfig(Hardware);
  ota.OverrideDevice("ACS Core");
  if(DebugMode){
    ota.EnableSerialDebug();
  }
  int otaresp = ota.CheckForOTAUpdate(OTA_URL, Version);
  if(DebugMode){
    Serial.print(F("OTA Response Code: ")); Serial.println(otaresp);
    Serial.println(errtext(otaresp));
    Serial.println(F("We're still here, so there must not have been an update."));
  }
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
    Serial.println("Didn't find PN532 board. Trying again...");
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
  if(DebugMode){
    Serial.println(F("NFC Setup")); 
  }
  //Load up the SPIFFS file of verified IDs, format/create if it is not there.
  if(!SPIFFS.begin(1)){ //Format SPIFFS if fails
    Serial.println("SPIFFS Mount Failed");
    while(1);
  }
  if(SPIFFS.exists("/validids.txt")){
    Serial.println(F("Valid ID List already exists."));
  } else{
    Serial.println(F("No Valid ID List file found. Creating..."));
    File file = SPIFFS.open("/validids.txt", "w");
    if(!file){
      Serial.println(F("ERROR: Unable to make file."));
    }
    file.print(F("Valid IDs:"));
    file.println();
    file.close();
  }

  //Set the inital state
  //If the ESP32 performed a controlled restart, such as to install an OTA update or try to fix a wireless issue, 
  if(DebugMode){
    Serial.print(F("Reset Reason: "));
    print_reset_reason(rtc_get_reset_reason(0));
  }
  if(rtc_get_reset_reason(0) != POWERON_RESET){
    //Reset for a reason other than power on reset.
    State = settings.getString("LastState");
  }
  if(State == NULL){
    State = "Lockout";
  }

  //Disable the startup lights
  vTaskDelete(xHandle2);

  //Lastly, create all tasks and begin operating normally.
  vTaskSuspendAll();
  xTaskCreate(RegularReport, "RegularReport", 1024, NULL, 6, NULL);
  xTaskCreate(Temperature, "Temperature", 2048, NULL, 5, NULL);
  xTaskCreate(USBConfig, "USBConfig", 2048, NULL, 5, NULL);
  xTaskCreate(InternalRead, "InternalRead", 1500, NULL, 5, NULL);
  xTaskCreate(ReadCard, "ReadCard", 2048, NULL, 5, NULL);
  xTaskCreate(StatusReport, "StatusReport", 5000, NULL, 2, NULL);
  xTaskCreate(VerifyID, "VerifyID", 4096, NULL, 1, NULL);
  //InternalWrite has a handle to allow it to be suspended before a restart.
  xTaskCreate(InternalWrite, "InternalWrite", 2048, NULL, 5, &xHandle);
  xTaskCreate(LEDControl, "LEDControl", 1024, NULL, 5, NULL);
  xTaskCreate(BuzzerControl, "BuzzerControl", 1024, NULL, 5, NULL);
  xTaskCreate(MachineState, "MachineState", 1024, NULL, 5, NULL);
  xTaskCreate(NetworkCheck, "NetworkCheck", 4096, NULL, 3, NULL);
  xTaskCreate(MessageReport, "MessageReport", 2048, NULL, 3, NULL);
  xTaskResumeAll();

  StartupStatus = 1;
  Serial.println(F("Setup done."));
}

void loop(){
  //No longer need the Arduino loop, get rid of it.
  vTaskDelete(NULL);
}



void callback_percent(int offset, int totallength) {
  //Used to display percentage of OTA installation
  static int prev_percent = -1;
  int percent = 100 * offset / totallength;
  if (percent != prev_percent && DebugMode) {
    Serial.printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
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

void print_reset_reason(int reason) {
  switch (reason) {
    case 1:  Serial.println("POWERON_RESET"); break;          /**<1,  Vbat power on reset*/
    case 3:  Serial.println("SW_RESET"); break;               /**<3,  Software reset digital core*/
    case 4:  Serial.println("OWDT_RESET"); break;             /**<4,  Legacy watch dog reset digital core*/
    case 5:  Serial.println("DEEPSLEEP_RESET"); break;        /**<5,  Deep Sleep reset digital core*/
    case 6:  Serial.println("SDIO_RESET"); break;             /**<6,  Reset by SLC module, reset digital core*/
    case 7:  Serial.println("TG0WDT_SYS_RESET"); break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8:  Serial.println("TG1WDT_SYS_RESET"); break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9:  Serial.println("RTCWDT_SYS_RESET"); break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10: Serial.println("INTRUSION_RESET"); break;        /**<10, Instrusion tested to reset CPU*/
    case 11: Serial.println("TGWDT_CPU_RESET"); break;        /**<11, Time Group reset CPU*/
    case 12: Serial.println("SW_CPU_RESET"); break;           /**<12, Software reset CPU*/
    case 13: Serial.println("RTCWDT_CPU_RESET"); break;       /**<13, RTC Watch dog Reset CPU*/
    case 14: Serial.println("EXT_CPU_RESET"); break;          /**<14, for APP CPU, reset by PRO CPU*/
    case 15: Serial.println("RTCWDT_BROWN_OUT_RESET"); break; /**<15, Reset when the vdd voltage is not stable*/
    case 16: Serial.println("RTCWDT_RTC_RESET"); break;       /**<16, RTC Watch dog reset digital core and rtc module*/
    default: Serial.println("NO_MEAN");
  }
}

uint64_t millis64(){
  //This simple function replaces the 32 bit default millis. Means that overflow now occurs in 290,000 years instead of 50 days
  //Timer runs in microseocnds, so divide by 1000 to get millis.
  return esp_timer_get_time() / 1000;
}

void ReadyMessage(String ToSend){
  //This function can be used to pass a message to the "MessageReport" task.
  while(WritingMessage){
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  WritingMessage = 1;
  xSemaphoreTake(MessageMutex, portMAX_DELAY); 
  //If we have the mutex, and we have WritingMessage, we can write our message and set the flag.
  Message = ToSend;
  ReadyToSend = 1;
  xSemaphoreGive(MessageMutex);
}

void GamerMode(void *pvParameters){
  //This task simply blinks the front LED red-green-blue, aka gamer mode, to indicate startup in progress.
  while(1){
    Internal.println("L 255,0,0");
    delay(300);
    Internal.println("L 0,255,0");
    delay(300);
    Internal.println("L 0,0,255");
    delay(300);
  }
}