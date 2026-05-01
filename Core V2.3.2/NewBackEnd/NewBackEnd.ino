//Re-write of ACS Core firmware to make it simpler, faster. Uses just one mega loop for everything.

#define Version "2.0.5"
#define Hardware "2.3.2-LE"
#define DebugMode 1

//How often to send a status report, in milliseconds
#define STATUS_INTERVAL 15000

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

//Libraries:
  #include <OneWire.h>              //Replacing for WSACS API update...
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
  #include <nvs_flash.h>            //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <ESP32Time.h>            //Version 2.0.6 | Source: https://github.com/fbiego/ESP32Time
  #include <WebSocketsClient.h>     //Version 2.6.1 | Source: https://github.com/Links2004/arduinoWebSockets
  #include <ESP32Ping.h>            //Version 1.6   | Source: https://github.com/marian-craciunescu/ESP32Ping
  #include <ping.h>                 //Version 1.6   | Source: https://github.com/marian-craciunescu/ESP32Ping
  #include "esp_ota_ops.h"          //Version 3.1.1 | Inherent to ESP32 Arduino
  #include <MQTTPubSubClient.h> 
  #include "esp_efuse.h"
  #include "esp_efuse_table.h"

//Objects:
  Adafruit_PN532 nfc(SCKPin, MISOPin, MOSIPin, NFCCS);
  Preferences settings;
  WiFiClientSecure client;
  HardwareSerial Internal(1);
  JsonDocument usbjson;
  HTTPClient http;
  ESP32OTAPull ota;
  ESP32Time rtc;
  WebSocketsClient socket;
  MQTTPubSub::PubSubClient<256> mqtt;
  OneWire ds(TEMP); 

extern "C" bool verifyRollbackLater() {
  //This code is run to verify the OTA before actual setup.
  //Since we are handling OTA verification ourselves, we just return true.
  return true;
}

//SSL Certificate. R12 for make.rit.edu running on Let's Encrypt. Will expire on March 12 2027!
const char *root_ca = R"literal(
-----BEGIN CERTIFICATE-----
MIIFBTCCAu2gAwIBAgIQWgDyEtjUtIDzkkFX6imDBTANBgkqhkiG9w0BAQsFADBP
MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFy
Y2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTAeFw0yNDAzMTMwMDAwMDBa
Fw0yNzAzMTIyMzU5NTlaMDMxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBF
bmNyeXB0MQwwCgYDVQQDEwNSMTMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK
AoIBAQClZ3CN0FaBZBUXYc25BtStGZCMJlA3mBZjklTb2cyEBZPs0+wIG6BgUUNI
fSvHSJaetC3ancgnO1ehn6vw1g7UDjDKb5ux0daknTI+WE41b0VYaHEX/D7YXYKg
L7JRbLAaXbhZzjVlyIuhrxA3/+OcXcJJFzT/jCuLjfC8cSyTDB0FxLrHzarJXnzR
yQH3nAP2/Apd9Np75tt2QnDr9E0i2gB3b9bJXxf92nUupVcM9upctuBzpWjPoXTi
dYJ+EJ/B9aLrAek4sQpEzNPCifVJNYIKNLMc6YjCR06CDgo28EdPivEpBHXazeGa
XP9enZiVuppD0EqiFwUBBDDTMrOPAgMBAAGjgfgwgfUwDgYDVR0PAQH/BAQDAgGG
MB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDATASBgNVHRMBAf8ECDAGAQH/
AgEAMB0GA1UdDgQWBBTnq58PLDOgU9NeT3jIsoQOO9aSMzAfBgNVHSMEGDAWgBR5
tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcBAQQmMCQwIgYIKwYBBQUHMAKG
Fmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwBAgEwJwYD
VR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVuY3Iub3JnLzANBgkqhkiG9w0B
AQsFAAOCAgEAUTdYUqEimzW7TbrOypLqCfL7VOwYf/Q79OH5cHLCZeggfQhDconl
k7Kgh8b0vi+/XuWu7CN8n/UPeg1vo3G+taXirrytthQinAHGwc/UdbOygJa9zuBc
VyqoH3CXTXDInT+8a+c3aEVMJ2St+pSn4ed+WkDp8ijsijvEyFwE47hulW0Ltzjg
9fOV5Pmrg/zxWbRuL+k0DBDHEJennCsAen7c35Pmx7jpmJ/HtgRhcnz0yjSBvyIw
6L1QIupkCv2SBODT/xDD3gfQQyKv6roV4G2EhfEyAsWpmojxjCUCGiyg97FvDtm/
NK2LSc9lybKxB73I2+P2G3CaWpvvpAiHCVu30jW8GCxKdfhsXtnIy2imskQqVZ2m
0Pmxobb28Tucr7xBK7CtwvPrb79os7u2XP3O5f9b/H66GNyRrglRXlrYjI1oGYL/
f4I1n/Sgusda6WvA6C190kxjU15Y12mHU4+BxyR9cx2hhGS9fAjMZKJss28qxvz6
Axu4CaDmRNZpK/pQrXF17yXCXkmEWgvSOEZy6Z9pcbLIVEGckV/iVeq0AOo2pkg9
p4QRIy0tK2diRENLSF2KysFwbY6B26BFeFs3v1sYVRhFW9nLkOrQVporCS0KyZmf
wVD89qSTlnctLcZnIavjKsKUu1nA1iU0yYMdYepKR7lWbnwhdx3ewok=
-----END CERTIFICATE-----
)literal";

//Variables - Inter-Task Communication
bool GamerMode = 1;  //Set to 0 to disable gamer mode, i.e. cycle RGB. Used during boot.
bool RequestReset = 0; //Other tasks can set this to 1 to tell the ResetController to start a restart. 
String ResetReason; //Tells the RestartController why we are restarting.
bool Access = 0; //Used to tell the frontend to enable the access signal to the bus. 
String UID; //Stores the UID of the user currently using the machine.
bool ChangeBeep = 0; //Lets the frontend know to beep.

//Variables - System State
bool Identify = 0; //Set to 1 to play an identification alarm/buzzer.
String State = "UNKNOWN"; //State of the machine, uses the WSACS API standard wording (IDLE, UNLOCKED, ALWAYS_ON, LOCKED_OUT, FAULT, UNKNOWN)
String StateChangeReason; //What caused the state to change
bool PendingApproval = 0; //Set to 1 when we have a card present that hasn't been authed yet, this is used for LED animations. 
bool AccessDenied = 0; //Set to 1 when a card is present but has been denied, for LED animations. 
bool CardPresent = 0; //Used to track if there is a card present in the machine.
bool NFCBroken = 0;   //Set to 1 if we lose the NFC reader, needed since the NFC reader is an external component on these boards. 
String LastState = "UNKNOWN"; //Stores what the state was previously, to detect changes.
String PreservedLastState = "UNKNOWN";
bool LockWhenIdle = 0;
bool RestartWhenUnused = 0;
bool NoNetwork = 1;
bool ScheduledRestart = false; //Used to indicate it is time for a regular restart. 
unsigned long long ScheduledRestartTime = 0; //Used to give the user some breathing room before a shutdown occurs.
bool ImminentShutdown = false; //Used to let the frontend know to play a flashing warning light

//Variables - Config
String SerialNumber;
String Password;
String SSID;
String Server;
String Key;
int MakerspaceID;

//Variables - MQTT Incoming
String AuthResponse; 
bool NewAuth = 0;
String InfoResponse;
bool NewInfo = 0;
String CommandResponse;
bool NewCommand = 0;
bool NewPing = 1;
bool SendPing = 0;
unsigned long long NextPingTime = 0;
bool OTAValid = 0;
bool OTALocked = 0;

//Variables - MQTT Outgoing
String BaseTopic; //Used to store the root topic that all others are appended to.
String Message; //Info for the history tab
bool MessageToSend = 0;
bool LogToSend = 0;
String Log; //Non-message log to send to the server
String LogType;
bool SendAuth = 0;
bool StateChange = 0;
bool ReportConfig = 0;
bool RequestInfo = 0;
bool SendStatus = 0;
unsigned long long NextStatusTime;

//Variables - Inter-Task Communication
bool SealBroken = 0;  //Set to 1 if there is an incorrect OneWire device on the bus. 
bool ReSealBus = 0;
bool OverTemp = 0;    //Set to 1 if there is a device overtemperature on the bus, so we can fault. 
byte liveAddresses[5][8];      //OneWire addresses of what is currently connected, for server reporting.
int liveAddressCount = 0;                //Number of currently connected devices on the bus, for server reporting.
byte deviceCount = 0; //Tracks the number of OneWire devices found on the bus.

//Variables - Inter-Task Communication (Outside Frontend)
bool Button = 0;  //Set to 1 when frontend button pressed.
bool Switch1 = 0;  //Set to 1 when switch 1 of card detect is pressed.
bool Switch2 = 0;  //Set to 1 when switch 2 of the card detect is pressed.

//Variables - Inter-Task Communication (Inside Frontend)
bool NewLED = 0; //Set to 1 when there is new valid data to send to the frontend for the LEDs.
byte Red = 0; //Tracks the red channel light intensity
byte Green = 0; //Tracks the green channel light intensity
byte Blue = 0; //Tracks the blue channel light intensity
bool NewBuzzer = 0; //Set to 1 when there is a new valid buzzer tone to send.
unsigned int Tone = 0; //Set to the tone that the buzzer should play.
bool ResetLED = 0; //Set to 1 to take priority over the LED controller, to indicate the restart is imminent.
bool UnlockedBeep = 0;
bool SingleBeep = 0;

//For handling OneWire devices
struct Device {
  byte address[8];
  byte deviceType;         
  byte highTempLimit;      
  uint32_t classification; 
  float currentTemp;       
  bool isAlarming;         
  bool isOnline;           // <--- Track if it's actually responding
};

Device sensorList[10];

void setup() {
  // put your setup code here, to run once:

  xTaskCreate(OTAWatchdog, "OTAWatchdog", 2048, NULL, 1, NULL);

  Serial.begin(115200);
  Serial.println(F("STARTUP"));
  delay(2000);

  //Start SPIFFS:
  if(!SPIFFS.begin(1)){
    Serial.println(F("SPIFFS Mount Failed!"));
    Serial.flush();
    delay(1000);
    ESP.restart();
  } else{
    Serial.println(F("SPIFFS Mount Done!"));
  }

  //Connect to the frontend, set the buzzer off. In case we crashed while we were writing the buzzer.
  Internal.begin(115200, SERIAL_8N1, TOESP, TOTINY);
  Internal.println("B 0");
  delay(50);
  Internal.flush();
  Internal.println("B 0");
  Internal.println("L 255,0,0"); //Set it solid red until the task takes over. 

  //Create tasks to handle the internal reading, writing to the frontend.
  xTaskCreate(InternalSerial, "InternalSerial", 1024, NULL, 5, NULL);
  xTaskCreate(AVControl, "AVControl", 1024, NULL, 5, NULL);
  xTaskCreate(RestartController, "RestartController", 1024, NULL, 5, NULL);

  //Load settings from memory
  settings.begin("settings", false);
  Password = settings.getString("Password");
  if(Password.equalsIgnoreCase("null")){
    //Use a real NULL password.
    Password = "";
  }
  SSID = settings.getString("SSID");
  if(settings.isKey("SerialNumber")){
    //Use old-style serial number
    SerialNumber = settings.getString("SerialNumber");
    Serial.print(F("Loaded OneWire-based serial number: "));
    Serial.println(SerialNumber);
  } else{
    //Get our eFuse serial number;
    // The ID is 128 bits = 16 bytes
    uint8_t unique_id[16]; 
    
    // ESP_EFUSE_OPTIONAL_UNIQUE_ID is the constant defined in the IDF table
    esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_OPTIONAL_UNIQUE_ID, unique_id, 128);

    if (err == ESP_OK) {
      Serial.print("Serial Number: ");
      for (int i = 0; i < 16; i++) {
        if (unique_id[i] < 0x10) SerialNumber += "0"; // Lead with zero if byte < 16
        SerialNumber += String(unique_id[i], HEX);
      }
      SerialNumber.toUpperCase();
      Serial.print(SerialNumber);
      Serial.println();
    } else {
      Serial.printf("Error reading eFuse: 0x%X\n", err);
      Serial.println("Note: This ID may not exist on original ESP32 (Non-S2/S3) models.");
    }
  }
  Server = settings.getString("Server");
  Key = settings.getString("Key");
  int TimezoneHr;
  if(settings.isKey("Timezone")){
    TimezoneHr = settings.getString("Timezone").toInt();
  } else{
    TimezoneHr = -4; //Hardcoded EST
  }
  rtc.offset = TimezoneHr * 3600;
  //Special handler, since deployed hardware doesn't have a makerspace ID currently.
  if(!settings.isKey("MakerspaceID")){
    while(1){
      delay(1000);
      if(Serial.available()){
        String incoming = Serial.readString();
        incoming.trim();
        MakerspaceID = incoming.toInt();
        settings.putInt("MakerspaceID", MakerspaceID);
        Serial.print(F("Makerspace ID Set to: "));
        Serial.println(MakerspaceID);
        break;
      } else{
        Serial.println(F("ERROR! No Makerspace ID. Need to enter one now to continue..."));
        Serial.print(F("Serial Number: "));
        Serial.println(SerialNumber);
      }
    }
  }
  MakerspaceID = settings.getInt("MakerspaceID");

  Serial.println(F("Got a config for me?"));
  delay(3000);
  CheckforConfig();

  //Connect to the network before we continue.
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, Password);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  Serial.print(F("MAC Address: "));
  Serial.println(WiFi.macAddress());
  if(WiFi.status() != WL_CONNECTED){
    WiFi.reconnect(); //Force a manual connect attempt
    Serial.println(F("No WiFi? Waiting for reconnect"));
    while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(500);
    }
  }

  //If we cared about why we restarted, this'd be the place to handle it.

  //Check for an OTA update, install it if there is one present.
  ota.SetCallback(callback_percent);
  ota.SetConfig(Hardware);
  ota.OverrideDevice("ACS Core");
  ota.EnableSerialDebug();
  int otaresp = ota.CheckForOTAUpdate("https://raw.githubusercontent.com/rit-construct-makerspace/access-control-firmware/refs/heads/main/otadirectory.json", Version);
  Serial.print(F("OTA Response: "));
  Serial.println(errtext(otaresp));

  //If we made it past the OTA, then we are ready for normal operation.
  mqtt.begin(socket); //Enable MQTT on the websocket

  NetworkConnect();

  //Start the machine state
  xTaskCreate(MachineState, "MachineState", 1024, NULL, 5, NULL);

  //Start the NFC reader, make sure it is present.
   uint32_t versiondata;
  pinMode(NFCPWR, OUTPUT);
  pinMode(NFCRST, OUTPUT);
  digitalWrite(NFCPWR, HIGH);
  digitalWrite(NFCRST, HIGH);
  delay(100);
  while(1){
    nfc.begin();
    Serial.println(F("Looking for NFC Reader."));
    versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
      delay(1000);
      Serial.println(F("Didn't find NFC reader. Retrying..."));
    } else{
      Serial.println(F("NFC reader reports OK!"));
      break;
    }
  }

  //We should initialize the OneWire bus here, check for the right devices, etc.
  Serial.println(F("Starting OneWire..."));
  loadInventoryFromFile();
  if(deviceCount == 0){
     Serial.println(F("Inventory empty! Scanning to initialize..."));
     discoverDevices(); // This sets the initial baseline
     saveInventoryToFile(); // Save it so it's not empty next time
  }
  //We should also immediately do a OneWire integrity check.
  Serial.println(F("Checking Bus Integrity..."));
  checkBusHealth();
  updateBusTemperatures();
  refreshLiveAddressBuffer();

  //Going forward, we will check the OneWire bus in a different task to make life easier.
  xTaskCreate(BusManager, "BusManager", 2048, NULL, 5, NULL);

  //Time to loop! 
  while(liveAddressCount == 0){ //Wait for the BusManager to have a valid inventory;
    delay(100); 
  }
  GamerMode = 0; //Disable the startup lighting

}

void loop() {
  // put your main code here, to run repeatedly:

  //Step 0: Call the MQTT updater;
  mqtt.update();

  //Step 1: Check for config changes;
  CheckforConfig();

  //Step 4: Communicate with the server

  //Only do all this if we have a connection
  if(mqtt.isConnected() && !NoNetwork){

    NoNetwork = false;

    JsonDocument outgoing; //Json to construct the outgoing message in

     //Step 4.1: See if we have any outgoing messages, and send them.
    if(MessageToSend){
      //Send a message to the history
      MessageToSend = 0;
      outgoing["auditLog"] = true;
      outgoing["message"] = Message;
      outgoing["category"] = "message";
      String MessagePayload;
      serializeJson(outgoing, MessagePayload);
      outgoing.clear(); //Clear so other sends can use it
      String MessageTopic = BaseTopic + "/log";
      publish(MessageTopic, MessagePayload);
    }
    if(LogToSend){
      //Send a log to the audit logs (not the user-visible history)
      LogToSend = 0;
      outgoing["auditLog"] = false;
      outgoing["message"] = Log;
      outgoing["type"] = LogType;
      String LogPayload;
      serializeJson(outgoing, LogPayload);
      outgoing.clear();
      String LogTopic = BaseTopic + "/log";
      publish(LogTopic, LogPayload);
      LogType = "message"; //Default value unless we say otherwise.
    }
    if(SendAuth){
      //Send an auth request to the server
      SendAuth = 0;
      outgoing["state"] = "UNLOCKED";
      outgoing["cardTagID"] = UID;
      String AuthPayload;
      serializeJson(outgoing, AuthPayload);
      outgoing.clear();
      String AuthTopic = BaseTopic + "/authTo/request";
      publish(AuthTopic, AuthPayload);
    }
    if(StateChange){
      //Send report of a changed state
      StateChange = 0;
      JsonArray stateChannels = outgoing["channels"].to<JsonArray>();
      JsonObject stateObject = stateChannels.createNestedObject();
      stateObject["channelID"] = 0;
      stateObject["fromState"] = PreservedLastState;
      stateObject["toState"] = State;
      stateObject["reason"] = StateChangeReason;
      outgoing["currentCardTag"] = UID;
      String StateChangePayload;
      serializeJson(outgoing, StateChangePayload);
      outgoing.clear();
      String StateChangeTopic = BaseTopic + "/stateChange";
      publish(StateChangeTopic, StateChangePayload);
      //At the end, set change reason to nothing:
      StateChangeReason = "";
    }
    if(ReportConfig){
      //Report the current configuration
      ReportConfig = 0;
      JsonArray configChannels = outgoing["channels"].to<JsonArray>();
      JsonObject configObject = configChannels.createNestedObject();
      configObject["channelID"] = 0;
      configObject["tempDuration"] = 0;
      outgoing["inputMode"] = "INSERT";
      JsonObject configDeployment = outgoing["deployment"].to<JsonObject>();
      configDeployment["SN"] = SerialNumber;
      JsonArray configComponents = configDeployment["components"].to<JsonArray>();
      //Iterate through and add every component on the bus to the components array;
      for(int i = 0; i < liveAddressCount; i++){
        JsonObject deviceObj = configComponents.createNestedObject();
        // Convert the 8-byte address to a Hex String for JSON
        char addrStr[17]; 
        snprintf(addrStr, sizeof(addrStr), "%02X%02X%02X%02X%02X%02X%02X%02X",
        liveAddresses[i][0], liveAddresses[i][1], liveAddresses[i][2], liveAddresses[i][3],
        liveAddresses[i][4], liveAddresses[i][5], liveAddresses[i][6], liveAddresses[i][7]);
        deviceObj["SN"] = String(addrStr);
        for(int j = 0; j < deviceCount; j++) {
          if(memcmp(liveAddresses[i], sensorList[j].address, 8) == 0) {
            // Here we grab the deviceType and other data from the struct
            deviceObj["type"] = sensorList[j].deviceType; 
            deviceObj["identifier"] = sensorList[j].classification; //Server doesn't expect this yet, but we should send it
            break;
          }
        }
      }
      JsonObject flags = outgoing["flags"].to<JsonObject>();
      flags["lockWhenIdle"] = LockWhenIdle;
      flags["restartWhenUnused"] = RestartWhenUnused;
      String FWVer = "ShlugDuino V" + String(Version); 
      outgoing["firmware"] = FWVer;
      String ConfigPayload;
      serializeJson(outgoing, ConfigPayload);
      outgoing.clear();
      String ConfigTopic = BaseTopic + "/config/report";
      publish(ConfigTopic, ConfigPayload);
    }
    if(RequestInfo){
      //Request information from the server
      RequestInfo = 0;
      JsonArray infoFields = outgoing["fields"].to<JsonArray>();
      infoFields.add("TIME");
      if(State == "UNKNOWN"){
        //We don't know what state we should be in, so request it. 
        infoFields.add("STATE");
      }
      String InfoPayload;
      serializeJson(outgoing, InfoPayload);
      outgoing.clear();
      String InfoTopic = BaseTopic + "/info/request";
      publish(InfoTopic, InfoPayload);
    }
    if(SendStatus){
      //Send our current status to the server
      SendStatus = 0;
      JsonArray statusChannels = outgoing["channels"].to<JsonArray>();
      JsonObject statusObject = statusChannels.createNestedObject();
      statusObject["channelID"] = 0;
      statusObject["state"] = State;
      outgoing["currentCardTag"] = UID;
      String StatusPayload;
      serializeJson(outgoing, StatusPayload);
      outgoing.clear();
      String StatusTopic = BaseTopic + "/status";
      publish(StatusTopic, StatusPayload);
    }

    //Step 4.2: If we got a message, mark the OTA as secure.
    if(OTAValid && !OTALocked){
      //We got a message from the server, so we know the OTA is safe to keep.
      OTALocked = 1; //No need to do this again.
      const esp_partition_t *running_partition = esp_ota_get_running_partition();
      esp_ota_img_states_t ota_state;
      esp_ota_get_state_partition(running_partition, &ota_state);
      if(ota_state == ESP_OTA_IMG_PENDING_VERIFY){
        esp_ota_mark_app_valid_cancel_rollback();
        Serial.println(F("OTA update marked valid."));
        Message = "OTA update marked valid.";
        MessageToSend = 1;
      }
    }

    JsonDocument incoming; //Json doucment to parse the incoming
    
    //Step 4.3: Process any incoming messages
    if(NewAuth){
      //Process a response to an auth request.
      NewAuth = 0;
      deserializeJson(incoming, AuthResponse);
      bool IsAuthed = incoming["channels"][0]["approved"].as<bool>();
      String AuthID = incoming["cardTagID"].as<String>();
      PendingApproval = false;
      if(State == "IDLE"){
        if(IsAuthed){
          Serial.println(F("Access Granted!"));
          if(AuthID == UID){
            Serial.println(F("UIDs match. Unlocking."));
            State = "UNLOCKED";
            StateChangeReason = "AUTHED"; 
            UnlockedBeep = true;
          }
        } else{
          Serial.println(F("Access Denied!"));
          if(CardPresent){
            AccessDenied = 1;
          }
        }
      } else{
        Serial.println(F("Ignoring auth due to invalid state."));
      }
    }
    if(NewInfo){
      //Process a response to an info request.
      NewInfo = 0;
      deserializeJson(incoming, InfoResponse);
      //Set the time;
      if(incoming.containsKey("time")){
        unsigned long long millisecondTime = incoming["time"];
        rtc.setTime(millisecondTime/1000);
        Serial.print(F("Time set to: "));
        Serial.println(rtc.getDateTime(true));
      }
      if((incoming.containsKey("state")) && (State == "UNKNOWN")){
        State = incoming["state"][0]["state"] | "UNKNOWN";
         if(State == "UNLOCKED" || State == "ALWAYS_ON"){
          //We shouldn't go into these states
          State = "IDLE";
        }
        StateChangeReason = "COMMANDED";
        SingleBeep = 1;
        if((State == "UNLOCKED") || (State == "ALWAYS_ON")){
          //We do not restart into these states;
          State = "IDLE";
        }
        if(State == "FAULT"){
          //If we used to be faulted, restart in lockout instead.
          State = "LOCKED_OUT";
        }
        Serial.print(F("State set to > "));
        Serial.print(State);
        Serial.println(F(" < on startup."));

        //Always send a status report when we get new info
        SendStatus = 1;
      }
      
    }
    if(NewCommand){
      //Process an incoming command.
      NewCommand = 0;
      deserializeJson(incoming, CommandResponse);
      //State change command
      if(incoming.containsKey("toState")){
        State = incoming["toState"][0]["state"] | "UNKNOWN";
        if(State == "UNLOCKED" && !CardPresent){
          //Don't unlock if no card present
          State = "IDLE";
        }
        StateChangeReason = "COMMANDED";
        SingleBeep = 1;
        Serial.print(F("Commanded state to: "));
        Serial.println(State);
      }
      //Set flags
      if(incoming.containsKey("flags")){
        JsonObject flagObj = incoming["flags"].as<JsonObject>();
        if(flagObj.containsKey("lockWhenIdle")){
          LockWhenIdle = flagObj["lockWhenIdle"].as<bool>();
          Serial.print(F("Server set LockWhenIdle to: "));
          Serial.println(LockWhenIdle);
        }
        if(flagObj.containsKey("restartWhenUnused")){
          RestartWhenUnused = flagObj["restartWhenUnused"].as<bool>();
          Serial.print(F("Server set RestartWhenUnused to: "));
          Serial.println(RestartWhenUnused);
        }
      }
      //Action to do something
      if(incoming.containsKey("action")){
        if(incoming["action"] == "RESTART"){
          Serial.println(F("Server commanded restart!"));
          Serial.flush();
          ResetReason = "Server Ordered";
          RequestReset = true;
        }
        if((incoming["action"] == "SEAL") && SealBroken){
          Serial.println(F("Server commanded bus integrity re-seal."));
          ReSealBus = true;
        }
        if(incoming["action"] == "IDENTIFY"){
          Serial.println(F("Server commanded identify."));
          Identify = !Identify;
        }
        if(incoming["action"] == "SCHEDULED_RESTART"){
          Serial.println(F("Server indicated it is time for a scheduled restart."));
          ScheduledRestart = true;
        }

      }
    }

    //Step 4.4: Send a ping if requested
    if(SendPing){
      String PingTopic = BaseTopic + "/ping";
      publish(PingTopic, "Ping!");
      //Serial.println(F("Ping sent."));
      SendPing = 0;
      NextPingTime = millis64() + 1000;
    }
    
  } else{
    Serial.println(F("No network?"));
    NoNetwork = true;
    NetworkConnect();
  }

  delay(20);
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

uint64_t millis64(){
  //This simple function replaces the 32 bit default millis. Means that overflow now occurs in 290,000 years instead of 50 days
  //Timer runs in microseocnds, so divide by 1000 to get millis.
  return esp_timer_get_time() / 1000;
}

void NetworkConnect(){
  //Check the WiFi first
  retryNetwork:
  if(WiFi.status() != WL_CONNECTED){
    WiFi.reconnect(); //Force a manual connect attempt
    Serial.println(F("No WiFi? Waiting for reconnect"));
    unsigned long long WiFiTime = millis64() + 15000;
    while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(500);
      if(WiFiTime <= millis64()){
        Serial.println(F("Failed to connect to WiFi! Retrying..."));
        goto retryNetwork;
      }
    }
    Serial.println(F(" Connected!"));
  } else{
    Serial.println(F("Already had WiFi connection, skipping to websocket connection."));
  }
  
  //Start our websocket connection
  socket.disconnect();
  //socket.begin("129.21.61.154", 3000, "/mqtt", "mqtt"); //Test broker running on Stephen computer. 
  socket.beginSslWithCA(Server.c_str(), 443, "/mqtt", root_ca, "mqtt");
  socket.setReconnectInterval(2000); //Attempt to reconnect every 2 seconds if we lose connection
  Serial.println(F("Connecting to MQTT Broker"));
  unsigned long long SocketTime = millis64() + 15000;
  while(!mqtt.connect(SerialNumber, SerialNumber, Key)){ //Use serial number as unique ID, username, and key as password.
    Serial.print(".");
    delay(500);
    if(SocketTime <= millis64()){
      Serial.println(F("Failed to connect to websocket! Retrying network altogether..."));
      goto retryNetwork;
    }
  } 
  Serial.println(F(" MQTT Connected!"));

  //Subscribe to all MQTT topics relevant to us;
  BaseTopic = "makerspace/" + String(MakerspaceID) + "/device/" + SerialNumber;
  String SubAuth = BaseTopic + "/authTo/response";
  mqtt.subscribe(SubAuth, 2, [](const String& payload, const size_t size) {
    Serial.print(F("AuthTo Response: "));
    Serial.println(payload);
    AuthResponse = payload;
    NewAuth = 1;
    OTAValid = 1;
  });
  String SubInfo = BaseTopic + "/info/response";
  mqtt.subscribe(SubInfo, 2, [](const String& payload, const size_t size) {
    Serial.print(F("Info Response: "));
    Serial.println(payload);
    InfoResponse = payload;
    NewInfo = 1;
    OTAValid = 1;
  });
  String SubCommand = BaseTopic + "/command";
  mqtt.subscribe(SubCommand, 2, [](const String& payload, const size_t size) {
    Serial.print(F("Command Input: "));
    Serial.println(payload);
    CommandResponse = payload;
    NewCommand = 1;
    OTAValid = 1;
  });
  String SubPing = BaseTopic + "/ping";
  mqtt.subscribe(SubPing, 2, [](const String& payload, const size_t size) {
    //Serial.println(F("Ping Loopback."));
    NewPing = 1;
    OTAValid = 1;
  });

  NoNetwork = false;

  //We should request and report things when we (re)connect
  ReportConfig = 1;
  RequestInfo = 1;
  SendPing = 1;
  NextPingTime = millis64() + 1000;
  LogType = "Network Connected";
  Log = "Network Connected";
  LogToSend = true;
}

void publish(String Topic, String Payload){
  if(Payload != "Ping!"){
    //No point in printing the ping payload constantly
    Serial.print(F("Publishing "));
    Serial.print(Payload);
    Serial.print(F(" to topic "));
    Serial.println(Topic);
  }
  mqtt.publish(Topic, Payload, false, 2); //Send not retained at QoS 2
}

void CheckforConfig(){
  //Called to see if a config JSON has been sent via USB.
  if(Serial.available()){
    Serial.setTimeout(20);
    String USBConfig = Serial.readString();
    JsonDocument USBJson;
    deserializeJson(USBJson, USBConfig);
    String NewSSID = USBJson["SSID"];
    if(USBJson["SSID"].is<String>()){
      Serial.print(F("Set WiFi SSID to: "));
      Serial.println(NewSSID);
      settings.putString("SSID", NewSSID);
    } else{
      Serial.println(F("Kept old WiFi SSID."));
    }
    String NewPassword = USBJson["Password"];
    if(USBJson["Password"].is<String>()){
      Serial.print(F("Set WiFi password to: "));
      Serial.println(NewPassword);
      settings.putString("Password", NewPassword);
    } else{
      Serial.println(F("Kept old WiFi password."));
    }
    String NewServer = USBJson["Server"];
    if(USBJson["Server"].is<String>()){
      Serial.print(F("Set server to: "));
      Serial.println(NewServer);
      settings.putString("Server", NewServer);
    } else{
      Serial.println(F("Kept old server."));
    }
    String NewKey = USBJson["Key"];
    if(USBJson["Key"].is<String>()){
      Serial.println(F("Set a new key (not printed for security)"));
      settings.putString("Key", NewKey);
    } else{
      Serial.println(F("Kept old key."));
    }
    String NewTimezone = USBJson["Timezone"];
    if(USBJson["Timezone"].is<String>()){
      Serial.print(F("Set timezone to: "));
      Serial.println(NewTimezone);
      settings.putString("Timezone", NewTimezone);
    } else{
      Serial.println(F("Kept old timezone."));
    }
    String NewMakerspaceID = USBJson["MakerspaceID"];
    if(USBJson["MakerspaceID"].is<String>()){
      Serial.print(F("Set makerspace ID to: "));
      Serial.println(NewMakerspaceID);
      int tempid = NewMakerspaceID.toInt();
      settings.putInt("MakerspaceID", tempid);
    } else{
      Serial.println(F("Kept old makerspace ID."));
    }
    Serial.println(F("Above settings have been saved to memory. Restart device to apply settings."));
  }
}

void OTAWatchdog(void *pvParameters){
  bool OTATimeout = false;
  while(1){
    delay(10);
    //This task reverts an OTA if not marked valid in 60 seconds.
    if(!OTATimeout && (millis64() >= 60000)){
      OTATimeout = 1;
      const esp_partition_t *running_partition = esp_ota_get_running_partition();
      esp_ota_img_states_t ota_state;
      esp_ota_get_state_partition(running_partition, &ota_state);

      if(ota_state == ESP_OTA_IMG_PENDING_VERIFY){
        Serial.println(F("OTA update timer failed after 60 seconds."));
        Serial.println(F("Reverting firmware."));
        settings.putString("ResetReason","OTA-Revert");
        esp_ota_mark_app_invalid_rollback_and_reboot();
      } else{
        Serial.println(F("OTA update timer passed. Disabling."));
      }
    }
    if(OTATimeout){
      vTaskSuspend(NULL);
    }
  }
}