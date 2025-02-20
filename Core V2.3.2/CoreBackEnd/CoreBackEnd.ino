#define Version "V232-BE-250117"
#define HWVer "V2.3.2-LE"

/*

----- ACCESS CONTROL CORE: Backend FIRMWARE -----

For Hardware Version 2.3.2

Written by Jim Heaney, RIT SHED Makerspace

Licensed CC-BY-SA-NC 4.0

Documentation: https://github.com/rit-construct-makerspace/access-control-hardware

This firmware is intended to run on the 32 bit WiFi microcontroller of the Access Control Core. 

Theory of Operation:
V2.3.2 of the Access Control core implemented many improvements discovered in testing previous iterations. One of these changes that manifested
was dividing the Core's duties into a lightweight frontend system, and a blocking, slower backend system. These two would then communicate over
UART to work in unison, with the frontend serving the role of master when needed. 

The frontend handles anything and everything that someone physically standing in front of the machine can perceive. This focus allows an increase
in responsiveness, and will lead to a better user experience. Blocking, long-duration tasks (like sending HTTPS messages) is left to the backend.

The frontend is responsible for the key switch and button on the front interface, the indicator light and buzzer, the card detection switches,
and the switch signal for the connected Access Control Switches. 

The backend is reponsible for managing and reading the NFC reader, gathering diagnostic data on the state of the system, and sending API requests to the server. 

*/

//This version of the code is stable and featured enough that it can be used.

//Libraries:
#include <Adafruit_PN532.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TaskScheduler.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_wifi.h>
#include "FS.h"
#include "SPIFFS.h"

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

//Global Variables
String MachineID = "";
String MachineType = "";
byte ExpectedSwitchType;
bool ExpectInterface;
bool CardPresent = 0;
String CurrentID = "N/A"; //The current ID used on the system.
bool Lockout = 1; //If 1, in lockout mode
bool AlwaysOn = 0;  //If 1, in always on mode
bool Interface = 0; //If 1, send codes to an external interface
unsigned long OnTime = 0; //Notes when a session was started
bool HelpFlashing = 0;
bool FirstStatus = 1;
bool NoGateway = 0;
byte RemoteCount = 0;
float Temperature = 0;
bool StartupDone = 0;
bool CardDetect = 0;
unsigned long ExpireTime = 0;
bool SessionExpired = 0;
String ToPrint;
bool SwitchOn = 0;
unsigned long LastServer = 0; //Stores the last time we talked to the server. 
unsigned long PingTime = 0; //Longest time we should go before checking for server connection, in milliseconds. 
bool NoServer = 0; //If 1, we failed a server connection.
unsigned long ChangeTime = 0; //Tracks the time when a switch was changed, so we can report how long we've been in a (lockout/alwayson) state.
unsigned long LastSessionTime = 0; //How long the last session on the machine was.
String LastID = "N/A"; //Stores the last UID used in the machine
String SSID = ""; //The SSID that the wireless network will connect to. 
String Password = ""; //Stores the WiFi password
String Server = "https://make.rit.edu"; //The server address that API calls are made against
String Key; //Stores the API access key
String Zone; //Stores the area that a machine is deployed in.
int TempLimit; //The temperature above which the system shuts down and sends a warning. 
int Frequency; //How often an update should be sent
DeviceAddress TempAddr1, TempAddr2, TempAddr3;
String EthMAC = ""; //Stores the ethernet MAC
byte State = 0; //Stores the reported state from frontend
byte LastState = 0; //Stores the last reported state from frontend
byte Unlocked = 0;
bool retry = 0; //Tracks if a message has already been attempted to re-send.
bool HelpState = 0; //Tracks if help is currently being requested or not.
String FEVer; //Stores the frontend firmware version, retrieved on startup
bool validid = 0; //1 if the ID inserted is valid based on the internal list.
bool SessionEnd = 0; //1 if the session just ended, so the next status update contains the time the machine was on for.
unsigned long LastTime = 0;// Tracks how long the last session was

//Prototype Functions for Scheduler:
void UpdateInterface(); //Reads the current state of the button and switches on the interface, updates accordingly, sends Help and Status change REST API Calls
void SendStatus(String StatusSource); //Sends status update, pass a String to say the source. 
void TaskStatus(); //Wrapper for SendStatus, since task scheduler does not like a function with an input.
void CheckTemperature(); //Checks the temperature, if too high reports/errors
void CheckForServer(); //Checks for a connection to the server with the REST API "Check" call
void CheckEthernet(); //Checks for an ethernet connection occasionally, to see if we should switch.

//Define tasks for scheduler:
Scheduler ts;
Task TaskCheckTemperature(TASK_SECOND, TASK_FOREVER, &CheckTemperature); //Check the temperature once per second
Task TaskSendStatus(10*TASK_SECOND, TASK_FOREVER, &TaskStatus); //Send a status message every 10 seconds
Task TaskCheckForServer(60*TASK_SECOND, TASK_FOREVER, &CheckForServer); //Check for a server connection once a minute
Task TaskCheckEthernet(10*TASK_SECOND, TASK_FOREVER, &CheckEthernet); //Check for a new ethernet connection every 10 seconds

//Create Objects:
Adafruit_PN532 nfc(SCKPin, MISOPin, MOSIPin, NFCCS);
JsonDocument doc;
Preferences settings;
WiFiClientSecure client;
OneWire oneWire(TEMP);
DallasTemperature tempsensors(&oneWire);
HardwareSerial Internal(1);
HardwareSerial Debug(0);


void setup() {
  // put your setup code here, to run once:

  delay(5000);

  Internal.begin(115200, SERIAL_8N1, TOESP, TOTINY);
  Debug.begin(115200);

  Debug.println(F("Debug Mode Enabled!"));
 
  Debug.println(F("Waiting 5 seconds to see if there are config values to load."));
  Debug.println(F("update?"));
  settings.begin("settings", false);
  byte WaitCount = 0;
  while(WaitCount <= 5){
    WaitCount++;
    delay(1000);
  }
  if(Debug.available() != 0){
    //New message received
    String SerialIn = Debug.readString();
    Debug.println(F("Got: "));
    Debug.println(SerialIn);
    Debug.flush();
    deserializeJson(doc, SerialIn);
    //Check each possible input, see if it is present in the JSON, and if so write that update
    UpdateSetting("SSID");
    UpdateSetting("Password");
    UpdateSetting("Server");
    UpdateSetting("Key");
    UpdateSetting("MachineID");
    UpdateSetting("MachineType");
    UpdateSetting("SwitchType");
    UpdateSetting("Interface");
    UpdateSetting("Zone");
    UpdateSetting("TempLimit");
    UpdateSetting("Frequency");
  }
  //Update all of the settings from the preferences
  SSID = settings.getString("SSID");
  Password = settings.getString("Password");
  Server = settings.getString("Server");
  Key = settings.getString("Key");
  MachineID = settings.getString("MachineID");
  MachineType = settings.getString("MachineType");
  ExpectedSwitchType = settings.getString("SwitchType").toInt();
  ExpectInterface = settings.getString("Interface").toInt();
  Zone = settings.getString("Zone");
  TempLimit = settings.getString("TempLimit").toInt();
  Frequency = settings.getString("Frequency").toInt();

  //Print all of the loaded settings:
  Debug.println(F("Loaded Settings: "));
  Debug.print(F("SSID: ")); Debug.println(SSID);
  Debug.print(F("Password: ")); Debug.println(Password);
  Debug.print(F("Server: ")); Debug.println(Server);
  Debug.print(F("Key: ")); Debug.println(Key);
  Debug.print(F("Machine ID: ")); Debug.println(MachineID);
  Debug.print(F("Machine Type: ")); Debug.println(MachineType);
  Debug.print(F("Expected Switch Type: ")); Debug.println(ExpectedSwitchType);
  Debug.print(F("Expect Interface: ")); Debug.println(ExpectInterface);
  Debug.print(F("Zone: ")); Debug.println(Zone);
  Debug.print(F("Temperature Limit: ")); Debug.println(TempLimit);
  Debug.print(F("Status Report Frequency: ")); Debug.println(Frequency);

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

  //Setup pinmodes
  pinMode(DEBUGLED, OUTPUT);
  pinMode(NFCPWR, OUTPUT);
  pinMode(NFCRST, OUTPUT);

  //Check if the PN532 is there;
  digitalWrite(NFCPWR, HIGH);
  digitalWrite(NFCRST, HIGH);
  delay(100);
  retry:
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Debug.println("Didn't find PN53x board");
    delay(1000);
    goto retry;
  }
  Debug.println(F("Found NFC redaer."));
  digitalWrite(NFCRST, LOW);
  digitalWrite(NFCPWR, LOW);

  Debug.print(F("Wireless MAC: ")); Debug.println(WiFi.macAddress());

  //Check if the right switch type is connected
  //TODO: Not implemented yet!

  //Wireless Initialization:
  Debug.print("Attempting to connect to SSID: ");
  Debug.println(SSID);
  if(Password != "null"){
    WiFi.begin(SSID, Password);
  }
  else{
    Debug.println(F("Using no password."));
    WiFi.begin(SSID);
  }
  //Attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Debug.print(".");
    //Wait 1 second for re-trying
    delay(1000);
  }
  Debug.println("");
  Debug.print("Connected to ");
  Debug.println(SSID);
  Debug.print(F("Local IP: "));
  Debug.println(WiFi.localIP());
  client.setInsecure();

  //Initialize the temperature sensors
  Debug.println(F("Searching for temperature sensors. Should find 2."));
  if(tempsensors.getAddress(TempAddr1, 0)){
    Debug.println(F("Found temperature sensor 1."));
    Debug.print(F("Sensor 1 temperature: "));
    tempsensors.requestTemperatures();
    Debug.println(tempsensors.getTempC(TempAddr1));
  }
  if(tempsensors.getAddress(TempAddr2, 1)){
    Debug.println(F("Found temperature sensor 2."));
    Debug.print(F("Sensor 2 temperature: "));
    tempsensors.requestTemperatures();
    Debug.println(tempsensors.getTempC(TempAddr2));
  }

  //Attach all tasks
  ts.addTask(TaskCheckTemperature);
  TaskCheckTemperature.enable();  //CHecking for temperature causes massive delays to the code exectuting. Turned off for now.
  ts.addTask(TaskSendStatus);
  TaskSendStatus.enable();
  TaskSendStatus.setInterval(Frequency*TASK_SECOND);
  ts.addTask(TaskCheckForServer);
  TaskCheckForServer.enable();

  Internal.print(F("v")); //Check the firmware version of the front end
  delay(10);
  CheckSerial();

  //Transmit a startup message to the server
  CheckTemperature();
  SendStatus("Startup");

  Debug.println(F("Setup Complete!"));
  Internal.print(F("w")); //Tell the frontend to startup

}

void loop() {
  // put your main code here, to run repeatedly:
  ts.execute();

  CheckSerial();

}

void UpdateSetting(String Key){ //Done? 
  //Updates a value in the preferences with a value from a JSON document.
  const char* Temp = doc[Key];
  const char* KeyArray = Key.c_str();
  if(!Temp){
    return;
  }
  Debug.print(F("Updating key ")); Debug.print(KeyArray); Debug.print(F(" with value ")); Debug.println(Temp);
  //Key is present
  settings.putString(Key.c_str(), Temp);
}

void TaskStatus(){ 
  //Wrapper for send status, since task scheduler dislikes functions with variables.
  SendStatus("Scheduled");
}

void CheckTemperature(){
  //Check the temperature, log the highest one, act on it if it exceeds the maximum programmed temperature
  tempsensors.requestTemperatures();
  float Temp1 = tempsensors.getTempC(TempAddr1);
  float Temp2 = tempsensors.getTempC(TempAddr2);
  Temperature = max(Temp1, Temp2);
  if(Temperature > TempLimit){
    //Temperature has exceeded safe limits
    Internal.println("l");
    Debug.println(F("Extreme Temperature Detected. Shutting Down..."));
    SendStatus("Temperature");
  }
}

void CheckForServer(){ 
  //Checks for connection to the server.
  //If we already know we are not connected to the server, attempt to re-connect;
  if(NoServer){
    Debug.println(F("Attempting to re-connect to WiFi..."));
    Debug.println(F("REMINDER: If WiFi network is in range but we cannot connect, this will block indefinitely..."));
    WiFi.reconnect();
  }

  //Start by checking the last time we had connection to the server;
  if((millis() - LastServer) >= 20000){
    //No connection in over 20 seconds. Check for a connection;
    HTTPClient http;
    String ServerPath = Server + "/api/check/" + MachineID;
    Debug.print("GET To: ");
    Debug.println(ServerPath);
    retry = 0;
    retryCheck:
    http.begin(client, ServerPath);
    int httpCode = http.GET();
    Debug.print(F("HTTP Response: "));
    Debug.println(httpCode);
    http.end();
    if(httpCode == 200){
      LastServer = millis();
      Internal.println("c");
      NoServer = 0;
      return;
    } else{
      //No connection to the server seemingly.
      if(!retry){
        retry = 1;
        Debug.println("Attempting to re-send check message...");
        goto retryCheck;
      }
      Debug.println(F("No Server Connection?"));
      NoServer = 1;
      Internal.println("i");
    }
  }
}

void CheckEthernet(){ //Shelved for now
}

void CheckSerial(){ //Done?
  //Checks for new internal messages, handles accordingly.
  if(Internal.available()){
    readagain:
    //New serial message in buffer
    String incoming = Internal.readStringUntil('\n');
    incoming.trim();
    Debug.print(F("Received Message: "));
    Debug.println(incoming);
    //Process the message;
    switch (incoming.charAt(0)){
      case 'r':
        //Card was removed
        LastTime = millis() - OnTime;
        SendStatus("Card Removed");
        validid = 0;
        CurrentID = "N/A";
      case 'a':
        //Authentication request
        AuthRequest();
      break;
      case 'p':
        //Card is present
        ValidateCard();
      break;
      case 's':
        //State
        State = incoming.charAt(2)-48;
        if(State != LastState){
          LastState = State;
          SendStatus("State Change");
        }
        ChangeTime = millis();
      break;
      case 'u':
        //Unlocked status
        Unlocked = incoming.charAt(2)-48;
        ChangeTime = millis();
      break;
      case 'h':
        //Help status
        HelpState = incoming.charAt(2)-48;
        break;  
      case 'v':
        //Version report
        FEVer = incoming;
        FEVer.remove(0, 2); //Remove the preamble

    }
    if(Internal.available() > 0){
      //Make sure it is not the same message.
      if(Internal.peek() == incoming.charAt(0)){
        //Same message character, flush the buffer.
        while(Internal.available()){
          Internal.read();
        }
      }
      else{
        //Read the new message
        goto readagain;
      }
    }
  }
}

void AuthRequest(){ //Done?
  //Handles authentication requests

  //First, activate the NFC reader
  byte RetryCardCount = 0;
  RetryCard:
  digitalWrite(NFCPWR, HIGH);
  delay(10);
  digitalWrite(NFCRST, HIGH);
  delay(10);
  nfc.wakeup();
  nfc.setPassiveActivationRetries(0xFF);
  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0, 0 };	// Buffer to store the returned UID
  uint8_t uidLength;				// Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength,500);
  if(success){
    Debug.println(F("Found an NFC card!"));
    if(uidLength == 7){
      CurrentID = "";
      Debug.println(F("ID is proper length."));
      Serial.print(F("UID Value: "));
      for (uint8_t i=0; i < uidLength; i++){
        Debug.print(uid[i], HEX);
        Debug.print(" ");
        CurrentID += String(uid[i], HEX);
      }
      Debug.println("");
      //WIP: check if ID is valid based on internal list
      Debug.println(F("Searching for ID in the SPIFFS file"));
      File file = SPIFFS.open("/validids.txt", "r");
      if(!file){
        Debug.println(F("File failed to open!"));
      }
      while(file.available()){
        String tempString = file.readStringUntil('\r');
        tempString.trim();
        Debug.print(F("Checking '"));
        Debug.print(tempString);
        Debug.println("'");
        if(CurrentID.equalsIgnoreCase(tempString)){
          validid = 1;
          Debug.println("ID Found in Valid List");
          Internal.println("q");
          break;
        } else{
          validid = 0;
        }
      }
      if(validid == 0){
        Debug.println("ID Not Found in Valid List.");
      }
      file.close();

      //Send auth request
      Debug.println(F("Sending authentication request to server..."));
      HTTPClient http;
      String ServerPath = Server + "/api/auth?type=" + MachineType + "&machine=" + MachineID + "&zone=" + Zone + "&needswelcome=1&id=" + CurrentID; 
      Debug.print(F("Sending GET to: ")); Debug.println(ServerPath);
      retry = 0;
      retryAuth:
      http.begin(client, ServerPath);
      int httpCode = http.GET();
      if(httpCode < 200 || httpCode > 299){
        //Bad HTTP Code
        Debug.print(F("Got Unexpected HTTP Code: ")); Serial.println(httpCode);
        if(!retry){
          retry = 1;
          Debug.println(F("Attempging to re-send auth request..."));
          goto retryAuth;
        }
        Internal.println("d");
        Debug.println(F("Auth message failed again. Checking for server..."));
        CheckForServer();
        return;
      }
      else{
        //Correct HTTP code
        String payload = http.getString();
        Debug.println(F("Received the following payload: "));
        Debug.println(payload);
        doc.clear();
        deserializeJson(doc, payload);
        if((doc["UID"] == CurrentID)){
          //follow up; it is possible to quickly swap cards on a slow internet connection and have the system approve the card without re-checking it. Investigate async or interrupt checks for card removal.
          Debug.println(F("UIDs Match!"));
          if(doc["Allowed"] == 1){
            Debug.println(F("Authorization Granted."));
            OnTime = millis();
            if(validid == 0){
              Debug.println(F("ID was approved but not in the file. Adding..."));
              File file = SPIFFS.open("/validids.txt",FILE_APPEND);
              if(!file.print(CurrentID)){
               Debug.println(F("ERROR: Unable to write to Valid ID File!")); 
              }
              file.println();
              Internal.println("q");
              file.close();
            }
            Internal.println("a");
            SendStatus("SessionStart");
          }
          if(doc["Allowed"] == 0){
            //Access denied;
            Debug.println(F("Authorization Denied"));
            Internal.print("d");
          }
        }
      }
    }
    else{
      Debug.print(F("Wrong UID Length. Ignoring."));
      Internal.print("d");
    }
  }
  else{
    Debug.println(F("Card Read Failed."));
    RetryCardCount++;
    if(RetryCardCount < 5){
      delay(100);
      digitalWrite(NFCRST, LOW);
      delay(10);
      goto RetryCard;
    }
    RetryCardCount = 0;
    Internal.println("d");
  }
  Debug.println(F("Card read complete. Shutting down NFC..."));
  digitalWrite(NFCRST, LOW);
  digitalWrite(NFCPWR, LOW);
}

void SendStatus(String StatusSource = "Scheduled"){ //Done?
  doc.clear();
  doc["Type"] = "Status";
  doc["Machine"] = MachineID;
  doc["MachineType"] = MachineType;
  doc["Zone"] = Zone;
  doc["Temp"] = String(Temperature, 3);
  doc["Help"] = String(HelpState);
  doc["BEVer"] = Version;
  doc["FEVer"] = FEVer;
  doc["HWVer"] = HWVer;
  Internal.println("r");
  delay(10);
  CheckSerial();
  unsigned long SessionTime = 0;
  Debug.print("State: "); Debug.println(State);
  Debug.print("Unlocked: "); Debug.println(Unlocked);
  if(State == 0){
    //In normal mode
    if(Unlocked){
      doc["State"] = "Active";
      doc["UID"] = CurrentID;
      SessionTime = millis() - OnTime;
    } else{
      doc["State"] = "Idle";
      doc["UID"] = CurrentID;
      SessionTime = LastTime;
    }
  }
  if(State == 1){
    //Locked on
    doc["State"] = "AlwaysOn";
    SessionTime = 0;
    doc["UID"] = CurrentID;
  }
  if(State == 2){
    //Locked off
    doc["State"] = "Lockout";
    SessionTime = 0;
    doc["UID"] = CurrentID;
  }
  doc["Time"] = String(SessionTime / 1000);
  doc["Source"] = StatusSource;
  doc["Frequency"] = String(Frequency);
  doc["Key"] = Key;
  String Payload;
  serializeJson(doc, Payload);
  Debug.print(F("Sending status update: "));
  Debug.println(Payload);
  HTTPClient http;
  String ServerPath = Server + "/api/status";
  Debug.print("To: ");
  Debug.println(ServerPath);
  retry = 0;
  statusRetry:
  http.begin(client, ServerPath);
  http.addHeader("Content-Type","application/json");
  int httpCode = http.PUT(Payload);
  Debug.print(F("HTTP Response: "));
  Debug.println(httpCode);
  http.end();
  if(httpCode == 200){
    LastServer = millis();
    Internal.println("c");
  } else{
    if(!retry){
      Debug.println(F("Failed to send status. Retrying..."));
      retry = 1;
      goto statusRetry;
    }
    Debug.println(F("Failed status twice. Checking server..."));
    CheckForServer();
  }
}

void ValidateCard(){
  //Allows a card to be verified without doing an access check. Only works if the user has put their card into the machine for an auth request before.
  //First, activate the NFC reader
  Debug.println(F("Performing non-authentication verification of card against internal records."));
  byte RetryCardCount = 0;
  RetryCard:
  digitalWrite(NFCPWR, HIGH);
  delay(10);
  digitalWrite(NFCRST, HIGH);
  delay(10);
  nfc.wakeup();
  nfc.setPassiveActivationRetries(0xFF);
  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0, 0 };	// Buffer to store the returned UID
  uint8_t uidLength;				// Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength,500);
  if(success){
    Debug.println(F("Found an NFC card!"));
    if(uidLength == 7){
      CurrentID = "";
      Debug.println(F("ID is proper length."));
      Serial.print(F("UID Value: "));
      for (uint8_t i=0; i < uidLength; i++){
        Debug.print(uid[i], HEX);
        Debug.print(" ");
        CurrentID += String(uid[i], HEX);
      }
      Debug.println("");
      //WIP: check if ID is valid based on internal list
      Debug.println(F("Searching for ID in the SPIFFS file"));
      File file = SPIFFS.open("/validids.txt", "r");
      if(!file){
        Debug.println(F("File failed to open!"));
      }
      while(file.available()){
        String tempString = file.readStringUntil('\r');
        tempString.trim();
        Debug.print(F("Checking '"));
        Debug.print(tempString);
        Debug.println("'");
        if(CurrentID.equalsIgnoreCase(tempString)){
          validid = 1;
          Debug.println("ID Found in Valid List");
          Internal.println("q");
          break;
        } else{
          validid = 0;
        }
      }
      if(validid == 0){
        Debug.println("ID Not Found in Valid List.");
      }
      file.close();
    }
  } else{
    RetryCardCount++;
    if(RetryCardCount <= 5){
      delay(50);
      goto RetryCard;
    }
  }
}