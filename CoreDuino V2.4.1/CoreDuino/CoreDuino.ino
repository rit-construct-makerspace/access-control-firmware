//ACS V2.4.1 Hardware, running CoreDuino code.
//This variant is a reduced version of the CoreDuino running hardware 3.0.0, to allow old hardware to be used as sign-in readers. It should not meant to be used for actual Accsss Control readers, although it should work for that purpose.

//DUE TO LIMITED MEMORY:
//Had to disable //Serial and USB CDC to get TLS working.

#define Version "2.1.9"
#define Hardware "2.4.1-LE"

//How often you send a status message, in milliseconds
#define STATUS_INTERVAL 15000 

//Pin Definitions:
  #define ACCESS 41
  #define NFCCS 6
  #define DET1 16
  #define DET2 15
  #define INTERRUPT 40
  #define SCKPin 10
  #define MOSIPin 8
  #define MISOPin 11
  #define LED 1
  #define IRQ 7
  #define PDOWN 2
  #define BUZZER 13
  #define BUTTON 0

//Libraries:
  #include <ArduinoJson.h>          //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
  #include <ArduinoJson.hpp>        //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
  //WARNING: The original ESP32-OTA-Pull will not work with this code. Use the forked version linked below!
  //#include <ESP32OTAPull.h>         //Version 1.0.1 | Source: https://github.com/JimSHED/ESP32-OTA-Pull-GitHub
  //V2.2.0: Testing new OTA library we will have locally;
  #include "ESP32OTAPullSecure.h"
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
  #include <SPI.h>
  #include <mfrc630.h>
  #include <Adafruit_NeoPixel.h>
  #include "USB.h"
  #include <esp_system.h>
  #include <esp_mac.h>
  #include "esp_efuse.h"
  #include "esp_efuse_table.h"
  #include <mbedtls/md.h>          //Inherent to ESP32
  #include <stdio.h>
  #include <esp_crc.h>  // ESP32 built-in CRC header

//Objects:
  Preferences settings;
  WiFiClientSecure client;
  JsonDocument ConfigJson;
  HTTPClient http;
  ESP32OTAPull ota;
  ESP32Time rtc;
  WebSocketsClient socket;
  MQTTPubSub::PubSubClient<1024> mqtt;
  Adafruit_NeoPixel CBI(1, LED, NEO_RGB + NEO_KHZ800);

extern "C" bool verifyRollbackLater() {
  //This code is run to verify the OTA before actual setup.
  //Since we are handling OTA verification ourselves, we just return true.
  return true;
}

String RootCert; //Stores the root certificate loaded from SPIFFS

//Variables - Inter-Task Communication
bool GamerMode = 1;  //Set to 0 to disable gamer mode, i.e. cycle RGB. Used during boot.
bool RequestReset = 0; //Other tasks can set this to 1 to tell the ResetController to start a restart. 
String ResetReason; //Tells the RestartController why we are restarting.
String UID; //Stores the UID of the user currently using the machine.
String FoundUID = ""; //Stores the last-found UID
bool ChangeBeep = 0; //Lets the frontend know to beep.
bool FaultBeep = 0; //We use the 3 beep normally for fault to indicate cannot welcome/auth due to no network, to differentiate from welcome/auth denied.

//Variables - System State
String HWVer = Hardware; //Hardware version, loaded from memory for use in OTA.
bool Identify = 0; //Set to 1 to play an identification alarm/buzzer.
String InputMode = "INSERT"; //Stores how we ingest cards. 
bool CardPresent = 0; //Used to track if there is a card present in the machine.
bool RestartWhenUnused = 0;
bool WelcomeMode = 1; //If 1, we are acting as a welcome reader and not a normal reader.
bool NoNetwork = 1;
bool ScheduledRestart = false; //Used to indicate it is time for a regular restart. 
unsigned long long ScheduledRestartTime = 0; //Used to give the user some breathing room before a shutdown occurs.
String TapUID; //Stores the UID between cycles for comparison when in tap mode.
bool UserWelcomed = 0;
unsigned long long NextStatusTime = 0;
bool AccessDenied = false; //Lights/sounds trigger when a user is not welcome.
bool TLSFalseAlarm = false; //Tracks if we have had repeated bad TLS connections. It may be a heap issue, and we should restart instead of continuing to try. 

//Variables - Config
String SerialNumber;
String Password;
String SSID = "";
String Server;
String Key;
int MakerspaceID;

//Variables - MQTT Incoming
String InfoResponse;
bool NewInfo = 0;
String CommandResponse;
bool NewCommand = 0;
bool NewPing = 1;
bool SendPing = 0;
unsigned long long NextPingTime = 0;
bool NewWelcome = 0;
String WelcomeResponse;
bool WelcomingPending = 0;

//Variables - MQTT Outgoing
String BaseTopic; //Used to store the root topic that all others are appended to.
String Message; //Info for the history tab
bool MessageToSend = 0;
bool LogToSend = 0;
String Log; //Non-message log to send to the server
String LogType;
bool ReportConfig = 0;
bool RequestInfo = 0;
bool SendStatus = 0;
bool SendWelcome = 0;

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

void setup() {
  // put your setup code here, to run once:

  //In case we crashed, immediately turn off buzzer and set LED red;
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);
  CBI.begin();
  CBI.setBrightness(30);
  CBI.setPixelColor(0, 255, 0, 0);
  CBI.show();

  pinMode(BUTTON, INPUT); 

  pinMode(ACCESS, OUTPUT);
  pinMode(INTERRUPT, INPUT_PULLUP);

  //Serial.begin(115200);

  //Serial.println(F("STARTUP"));
  //Serial.flush();
  delay(500);

  xTaskCreate(AVControl, "AVControl", 1024, NULL, 5, NULL);
  xTaskCreate(RestartController, "RestartController", 1024, NULL, 5, NULL);

  //Start SPIFFS:
  if(!SPIFFS.begin(1)){
    //Serial.println(F("SPIFFS Mount Failed!"));
    delay(1000);
    ESP.restart();
  }

  //Load the TLS cert from SPIFFS
  File file = SPIFFS.open("/cert.txt", FILE_READ);
  if(!file){
    //Serial.println(F("No cert found in SPIFFS!"));
    RootCert = "Nothing here!";
  } else{
    RootCert = "";
    while(file.available()){
      RootCert += (char)file.read();
    }
    //Serial.println(F("Loaded Root Cert:"));
    //Serial.println(RootCert);
  }

  //Load settings from memory

  //Get our Serial number;
  // The ID is 128 bits = 16 bytes
  uint8_t unique_id[16]; 
  
  // ESP_EFUSE_OPTIONAL_UNIQUE_ID is the constant defined in the IDF table
  esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_OPTIONAL_UNIQUE_ID, unique_id, 128);

  if (err == ESP_OK) {
    //Serial.print("//Serial Number: ");
    for (int i = 0; i < 16; i++) {
      if (unique_id[i] < 0x10) SerialNumber += "0"; // Lead with zero if byte < 16
    SerialNumber += String(unique_id[i], HEX);
    }
    SerialNumber.toUpperCase();
    //Serial.print(SerialNumber);
    //Serial.println();
  } else {
    //Serial.printf("Error reading eFuse: 0x%X\n", err);
    //Serial.println("Note: This ID may not exist on original ESP32 (Non-S2/S3) models.");
  }

  //Get our MAC address for printing, in V3.0.0 hardware this is our base MAC
  //Serial.print(F("WiFi MAC Address: "));
  //Serial.println(getBaseMacAddress());

  settings.begin("settings", false);

  //Serial.println(F("Have a config for me? I'll wait a second..."));
  delay(1000);
  //CheckforConfig();

  if(!settings.isKey("Server")){
    //We don't have a valid config?
    while(1){
      //Serial.println(F("Missing config. Please provide a JSON with the following keys: "));
      //Serial.println(F("WiFi SSID, WiFi Password, Server, Server Key, Timezone, MakerspaceID"));
      //Serial.print(F("Device WiFi MAC Address: "));
      //Serial.println(getBaseMacAddress());
      //Serial.print(F("Device //Serial Number: "));
      //Serial.println(SerialNumber);
      //CheckforConfig();
      delay(1000);
    }
  }

  if(!settings.isKey("HWVer")){
    //HWVer is new in 2.2.0, so firmware can run in similar hardware.
    //Assume any deployed hardware is 3.0.0
    settings.putString("HWVer", Hardware);
  }
  HWVer = settings.getString("HWVer");

  Server = settings.getString("Server");

  Password = settings.getString("Password");
  if(Password.equalsIgnoreCase("null")){
    //Use a real NULL password.
    Password = "";
  }
  SSID = settings.getString("SSID");
  Server = settings.getString("Server");
  Key = settings.getString("Key");
  int TimezoneHr;
  if(settings.isKey("Timezone")){
    TimezoneHr = settings.getString("Timezone").toInt();
  } else{
    TimezoneHr = -4; //Hardcoded EST
  }
  rtc.offset = TimezoneHr * 3600;
  MakerspaceID = settings.getString("MakerspaceID").toInt();

  //Serial.println(F("Settings loaded."));
  //Serial.flush();

  //Serial.println(F("Started Tasks."));
  //Serial.flush();

  //Start SPI here, in case we want to use Ethernet in the future. 
  //SCK, MISO, MOSI, SS
  pinMode(NFCCS, OUTPUT);
  SPI.begin(SCKPin, MISOPin, MOSIPin, -1);

  //Serial.println(F("Started SPI."));
  //Serial.flush();

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, Password);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  unsigned long WiFiStart = millis64();

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();  //Force a manual connect attempt
    //Serial.println(F("Waiting for first WiFi connect"));
    while (WiFi.status() != WL_CONNECTED && millis64() - WiFiStart < 15000) {
      //Serial.print(".");
      delay(500);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    //Serial.println(F("WiFi connected."));

    delay(500);

    // --- OTA LOGIC STARTS HERE ---
    // We only verify and check for updates if we are actually online.
    //Serial.println(F("Checking for OTA..."));

    // 1. Configure all OTA settings first
    //ota.Enable//SerialDebug();
    //We use the same cert on our server as Github does.
    ota.SetCACert(RootCert.c_str());
    ota.SetCallback(callback_percent);
    ota.SetConfig(HWVer.c_str()); 
    ota.OverrideDevice("ACS Core");

    // 2. Verify the current firmware can reach the JSON (or rollback)
    const char* jsonUrl = "https://raw.githubusercontent.com/rit-construct-makerspace/access-control-firmware/refs/heads/main/otadirectory.json";
    bool isValid = ota.VerifyOrRevert(jsonUrl, Version);

    // 3. If validation succeeded, check for a new update
    if (isValid) {
      int otaresp = ota.CheckForOTAUpdate(jsonUrl, Version);
      //Serial.print(F("OTA Response: "));
      //Serial.println(otaresp);
    }
    // --- OTA LOGIC ENDS HERE ---

  } else {
    // Device is offline. We skip OTA checks entirely to avoid false rollbacks.
    //Serial.println(F("WiFi failed to connect after timeout! Booting offline."));
  }

  // If we made it past the OTA (or skipped it because offline),
  // then we are ready for normal operation

  mqtt.begin(socket); //Enable MQTT on the websocket

  NetworkConnect();
  
  //If we cared about why we restarted, this'd be the place to handle it.

  //Start the NFC reader, make sure it is working as expected. 
  mfrc630_AN1102_recommended_registers(MFRC630_PROTO_ISO14443A_106_MILLER_MANCHESTER);
  mfrc630_write_reg(0x28, 0x8E);
  mfrc630_write_reg(0x29, 0x15);
  mfrc630_write_reg(0x2A, 0x11);
  mfrc630_write_reg(0x2B, 0x06);

  //Time to loop!
  xTaskCreate(MachineState, "MachineState", 2048, NULL, 5, NULL);
  GamerMode = 0; //Disable the startup lighting

}

void loop() {
  // put your main code here, to run repeatedly:

  delay(10);

  //Step 0: Call the MQTT updater;
  mqtt.update();

  //Check for a config JSON:
  //CheckforConfig();

  //Step 4: Communicate with the server

  //Only do all this if we have a connection
  if(mqtt.isConnected() && !NoNetwork){

    if(NoNetwork){
      NoNetwork = false;
    }

    JsonDocument outgoing; //Json to construct the outgoing message in

     //Step 4.1: See if we have any outgoing messages, and send them.

    if(MessageToSend){
      //Send a message to the history
      MessageToSend = 0;
      outgoing["auditLog"] = true; //Print in the history
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
      outgoing["auditLog"] = false; //Don't print in the history
      outgoing["message"] = Log;
      outgoing["type"] = LogType;
      String LogPayload;
      serializeJson(outgoing, LogPayload);
      outgoing.clear();
      String LogTopic = BaseTopic + "/log";
      publish(LogTopic, LogPayload);
      LogType = "message"; //Default value unless we say otherwise.
    }
    if(ReportConfig){
      //Report the current configuration
      ReportConfig = 0;
      outgoing["inputMode"] = InputMode;
      JsonObject configDeployment = outgoing["deployment"].to<JsonObject>();
      configDeployment["SN"] = SerialNumber;
      JsonArray configComponents = configDeployment["components"].to<JsonArray>();
      //Iterate through and add every component on the bus to the components array;
      JsonObject flags = outgoing["flags"].to<JsonObject>();
      flags["restartWhenUnused"] = RestartWhenUnused;
      flags["welcoming"] = WelcomeMode;
      String FWVer = "CoreDuino " + String(Version);
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
      infoFields.add("FLAGS"); //Check our flags, mostly for welcominginterface.
      String InfoPayload;
      serializeJson(outgoing, InfoPayload);
      outgoing.clear();
      String InfoTopic = BaseTopic + "/info/request";
      publish(InfoTopic, InfoPayload);
    }
    if(SendStatus){
      //Send our current status to the server, we do not send it if we do not know our state. 
      SendStatus = 0;
      JsonArray statusChannels = outgoing["channels"].to<JsonArray>();
      outgoing["currentCardTag"] = UID;
      String StatusPayload;
      serializeJson(outgoing, StatusPayload);
      outgoing.clear();
      String StatusTopic = BaseTopic + "/status";
      publish(StatusTopic, StatusPayload);
    }
    if(SendWelcome){
      //Send a welcome message to the server
      SendWelcome = 0;
      outgoing["cardTagID"] = UID;
      String WelcomePayload;
      serializeJson(outgoing, WelcomePayload);
      outgoing.clear();
      String WelcomeTopic = BaseTopic + "/welcome/request";
      publish(WelcomeTopic, WelcomePayload);
    }

    JsonDocument incoming; //Json doucment to parse the incoming
    
    //Step 4.3: Process any incoming messages

    if(NewInfo){
      //Process a response to an info request.
      NewInfo = 0;
      deserializeJson(incoming, InfoResponse);
      //Set the time;
      if(incoming.containsKey("time")){
        unsigned long long millisecondTime = incoming["time"];
        rtc.setTime(millisecondTime/1000);
        //Serial.print(F("Time set to: "));
        //Serial.println(rtc.getDateTime(true));
      }
      //Set flags:
      if(incoming.containsKey("flags")){
        JsonObject flagObj = incoming["flags"].as<JsonObject>();
        if(flagObj.containsKey("restartWhenUnused")){
          RestartWhenUnused = flagObj["restartWhenUnused"].as<bool>();
          //Serial.print(F("Server set RestartWhenUnused to: "));
          //Serial.println(RestartWhenUnused);
        }
        if(flagObj.containsKey("welcoming")){
          WelcomeMode = true; //We are always a welcome reader!
          if(WelcomeMode != flagObj["welcoming"].as<bool>()){
            WelcomeMode = flagObj["welcoming"].as<bool>();
            if(WelcomeMode){
            //Serial.println(F("Server flag set to enter welcoming mode."));
            } else{
              //We should never not be in welcome mode?
              Message = "WARNING: This device can only be operated as a sign in reader. Do not attempt to set to a normal access reader. Disregarding command.";
              MessageToSend = true;
              WelcomeMode = true;
            }
          }
        }
      }
      ReportConfig = 1; //Once we get some info, we should send our configuration.
      SendStatus = 1; //Once we get some info, we should send our status.
    }
    if(NewCommand){
      //Process an incoming command.
      NewCommand = 0;
      deserializeJson(incoming, CommandResponse);
      //Set flags
      if(incoming.containsKey("flags")){
        JsonObject flagObj = incoming["flags"].as<JsonObject>();
        if(flagObj.containsKey("restartWhenUnused")){
          RestartWhenUnused = flagObj["restartWhenUnused"].as<bool>();
          //Serial.print(F("Server set RestartWhenUnused to: "));
          //Serial.println(RestartWhenUnused);
        }
        if(flagObj.containsKey("welcoming")){
          WelcomeMode = true;
          if(WelcomeMode != flagObj["welcoming"].as<bool>()){
            WelcomeMode = flagObj["welcoming"].as<bool>();
            if(WelcomeMode){
              //Serial.println(F("Server flag set to enter welcoming mode."));
            } else{
              //We should never not be in welcome mode?
              Message = "WARNING: This device can only be operated as a sign in reader. Do not attempt to set to a normal access reader. Disregarding command.";
              MessageToSend = true;
              WelcomeMode = true;
            }
          }
        }
      }
      //Action to do something
      if(incoming.containsKey("action")){
        if(incoming["action"] == "RESTART"){
          //Serial.println(F("Server commanded restart!"));
          //Serial.flush();
          ResetReason = "Server Ordered";
          RequestReset = true;
        }
        if((incoming["action"] == "SEAL")){
          //Serial.println(F("Server commanded bus integrity re-seal."));
        }
        if(incoming["action"] == "IDENTIFY"){
          //Serial.println(F("Server commanded identify."));
          Identify = !Identify;
          if(!Identify){
            //Play a single beep to end the identify command.
            SingleBeep = true;
          }
        }
        if(incoming["action"] == "SCHEDULED_RESTART"){
          //Serial.println(F("Server indicated it is time for a scheduled restart."));
          ScheduledRestart = true;
        }

      }
    }
    if(NewWelcome){
      //Response to welcoming a user
      NewWelcome = 0;
      WelcomingPending = 0;
      deserializeJson(incoming, WelcomeResponse);
      bool IsWelcomed = incoming["welcomed"];
      String WelcomeID = incoming["cardTagID"];
      String WelcomeReason = incoming["reason"];
      if(IsWelcomed){
        //User was welcomed into the space properly.
        //Serial.println(F("User welcomed!"));
        if(UID == WelcomeID){
          //The user's card is still here, so beep and light up.
          UserWelcomed = 1;
        } else{
          //Serial.println(F("But their card isn't here anymore, so we will skip the lights/sounds."));
        }
      } else{
        //User was denied entry into the space.
        //Serial.print(F("User denied! Reason: "));
        //Serial.println(WelcomeReason);
        AccessDenied = 1; //Act like we denied the user access
      }
    }
    
    //Step 4.4: Send a ping if requested
    if(SendPing){
      String PingTopic = BaseTopic + "/ping";
      publish(PingTopic, "Ping!");
      ////Serial.println(F("Ping sent."));
      SendPing = 0;
      NextPingTime = millis64() + 1000;
    }
    
  } else{
    //Serial.println(F("No network?"));
    NoNetwork = true;
    NetworkConnect();
  }

}

void callback_percent(int offset, int totallength) {
  //Used to display percentage of OTA installation
  static int prev_percent = -1;
  int percent = 100 * offset / totallength;
  if (percent != prev_percent) {
    //Serial.printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
    prev_percent = percent;
  }
}


uint64_t millis64(){
  //This simple function replaces the 32 bit default millis. Means that overflow now occurs in 290,000 years instead of 50 days
  //Timer runs in microseocnds, so divide by 1000 to get millis.
  return esp_timer_get_time() / 1000;
}

void NetworkConnect() {
// 1. Check the WiFi first
retryNetwork:
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();  //Force a manual connect attempt
    //Serial.println(F("No WiFi? Waiting for reconnect"));
    unsigned long long WiFiTime = millis64() + 15000;
    while (WiFi.status() != WL_CONNECTED) {
      //Serial.print(".");
      delay(500);
      if (WiFiTime <= millis64()) {
        //Serial.println(F("Failed to connect to WiFi! Retrying..."));
        goto retryNetwork;
      }
    }
    //Serial.println(F(" Connected!"));
  } else {
    //Serial.println(F("Already had WiFi connection."));
  }

  //Before we test the TLS, let's make sure our clock is right
  //The mbedTLS library may be checking if the certs are at a valid date.
  if(rtc.getYear() <= 2025){
    //The RTC is not set!
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    int retryCount = 0;

    // Wait up to 10 seconds for the time to sync from the NTP server
    while (!getLocalTime(&timeinfo) && retryCount < 10) {
      //Serial.print(".");
      delay(1000);
      retryCount++;
    }

    if (retryCount < 10) {
      //Serial.println(F("\nTime synced successfully!"));

      // Grab the current Unix epoch time from the system
      time_t now;
      time(&now);

      // Push the synced time into your existing ESP32Time rtc object
      rtc.setTime(now);

      //Serial.print(F("Current RTC Time: "));
      //Serial.println(rtc.getDateTime(true));
    } else {
      //Serial.println(F("\nFailed to sync time. TLS test may still fail."));
    }
  }

  // 2. Test TLS Certificate before touching the websocket
  //Serial.println(F("Testing TLS connection..."));
  //Scope block to destroy networkclientsecure when done with it.
  {
    NetworkClientSecure networkclient;
    networkclient.setCACert(RootCert.c_str());  // Set the current known root cert

    if (networkclient.connect(Server.c_str(), 443)) {
      //Serial.println(F("TLS connection successful. Cert is valid."));
      networkclient.stop();  // CRITICAL: Free the TLS buffer so the websocket has heap to run!
    } else {
      //Serial.println(F("TLS connection failed..."));

      // Is the server alive?
      if (!Ping.ping(Server.c_str())) {
        //Serial.println(F("Cannot ping the server. No network?"));
        NoNetwork = true;
        return;
      }

      //Serial.println(F("Server is online. Bad TLS cert?"));
      //Serial.println(F("Getting new TLS certs from server over plain HTTP."));

      networkclient.stop();  // Ensure the secure client is completely shut down to free heap

      // Use your existing HTTPClient to fetch the cert over plain HTTP
      String url = "http://" + Server + "/api/rootCA";
      http.begin(url);
      http.addHeader("shlug-sn", SerialNumber);

      int httpCode = http.GET();
      String TLSPayload = "";

      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          TLSPayload = http.getString();
          //Serial.println(F("Successfully downloaded new cert payload."));
        } else {
          //Serial.print(F("HTTP GET failed, error code: "));
          //Serial.println(httpCode);
          http.end();  // Clean up
          NoNetwork = true;
          return;
        }
      } else {
        //Serial.print(F("HTTP GET failed, error: "));
        //Serial.println(http.errorToString(httpCode).c_str());
        http.end();  // Clean up
        NoNetwork = true;
        return;
      }

      http.end();  // Always free the HTTPClient resources when done

      //Parse the JSON payload
      JsonDocument TLSJson;
      deserializeJson(TLSJson, TLSPayload);

      //Before we accept the new cert, we should check the SHA-256
      String SHATLS = TLSJson["sha"];
      String NewCert = TLSJson["cert"];

      //Serial.print(F("JSON Hash:       "));
      //Serial.println(SHATLS);
      //Serial.print(F("Calculated Hash: "));
      //Serial.println(getSHA256(SerialNumber + ":" + Key + ":" + NewCert));

      if (SHATLS.equalsIgnoreCase(getSHA256(SerialNumber + ":" + Key + ":" + NewCert))) {
        //Serial.println(F("TLS cert was verified."));

        NewCert.replace("\\n", "\n");
        NewCert.replace("\r", "");
        NewCert.replace("\"", "");
        NewCert.trim();
        NewCert += "\n";

        if (RootCert.equals(NewCert)) {
          //Serial.println(F("The new cert is the same as the old cert? False alarm."));
          if(TLSFalseAlarm){
            //This is the second false alarm in a row! Let's restart to clear cache.
            RequestReset = true;
            while(1){
              delay(100);
            }
          } else{
            //First time having an isue, may be a fluke.
            TLSFalseAlarm = true;
          }
          LogToSend = true;
          LogType = "network";
          Log = "Attempt to load identical cert";
          goto retryNetwork;
        } else {
            SPIFFS.remove("/cert.txt");
            File file = SPIFFS.open("/cert.txt", FILE_WRITE);
          if (file.print(NewCert)) {
            file.close();
            RootCert = NewCert;
            //Serial.println(F("New cert saved. Regular operation can now resume."));
            //Serial.println(RootCert);
            //Serial.flush();
            delay(10);
            goto retryNetwork;  // Restart the function with the new cert
          } else {
            NoNetwork = true;
            //Serial.println(F("Unknown error, could not write new cert to file?"));
            goto retryNetwork;
          }
        }
      } else {
        NoNetwork = true;
        //Serial.println(F("CRITICAL ERROR: ATTEMPT WAS MADE TO LOAD BAD TLS CERTS!"));
        Message = "Attmpted to load cert with bad hash?";
        MessageToSend = true;
        delay(1000);
        goto retryNetwork;
      }
    }
  }

  // 3. Start our websocket connection
  // At this point, HTTP and networkclient are stopped, heap is free, and RootCert is verified valid.
  socket.disconnect();
  const char* global_ca_pointer = RootCert.c_str();
  socket.beginSslWithCA(Server.c_str(), 443, "/mqtt", global_ca_pointer, "mqtt");

  //Give the socket some time to stabilize:
  unsigned long long wsTimeout = millis64() + 5000;
  while (!socket.isConnected() && millis64() <= wsTimeout) {
    socket.loop();
    delay(2);
  }

  if (!socket.isConnected()) {
    //Serial.println(F("Websocket connection failed despite valid certs. Retrying..."));
    socket.disconnect();
    goto retryNetwork;
  }

  // 4. Connect to MQTT
  socket.setReconnectInterval(2000);  //Attempt to reconnect every 2 seconds if we lose connection
  //Serial.println(F("Connecting to MQTT Broker"));
  unsigned long long SocketTime = millis64() + 15000;

  while (!mqtt.connect(SerialNumber, SerialNumber, Key)) {
    //Serial.print(".");
    delay(500);
    if (SocketTime <= millis64()) {
      //Serial.println(F("Failed to connect to MQTT broker! Retrying network altogether..."));
      goto retryNetwork;
    }
  }
  //Serial.println(F(" MQTT Connected!"));
  TLSFalseAlarm = false; //We were able to connect, clear any false alarms.

  // Subscribe to all MQTT topics relevant to us;
  BaseTopic = "makerspace/device/" + SerialNumber;
  String SubInfo = BaseTopic + "/info/response";
  mqtt.subscribe(SubInfo, 2, [](const String& payload, const size_t size) {
    //Serial.print(F("Info Response: "));
    //Serial.println(payload);
    InfoResponse = payload;
    NewInfo = 1;
  });

  String SubCommand = BaseTopic + "/command";
  mqtt.subscribe(SubCommand, 2, [](const String& payload, const size_t size) {
    //Serial.print(F("Command Input: "));
    //Serial.println(payload);
    CommandResponse = payload;
    NewCommand = 1;
  });
  String SubWelcome = BaseTopic + "/welcome/response";
  mqtt.subscribe(SubWelcome, 2, [](const String& payload, const size_t size) {
    //Serial.print(F("Welcome Response: "));
    //Serial.println(payload);
    WelcomeResponse = payload;
    NewWelcome = 1;
  });
  String SubPing = BaseTopic + "/ping";
  mqtt.subscribe(SubPing, 2, [](const String& payload, const size_t size) {
    NewPing = 1;
  });

  NoNetwork = false;

  //We should request and report things when we (re)connect
  ReportConfig = 1;
  RequestInfo = 1;
  SendPing = 1;
  NextPingTime = millis64() + 1000;

  //Don't overwrite more important logs, like getting a new cert.
  if (!LogToSend) {
    LogType = "network";
    Log = "Network Connected";
    LogToSend = true;
  }
}

void publish(String Topic, String Payload){
  if(Payload != "Ping!"){
    //No point in printing the ping payload constantly
    //Serial.print(F("Publishing "));
    //Serial.print(Payload);
    //Serial.print(F(" to topic "));
    //Serial.println(Topic);
  }
  mqtt.publish(Topic, Payload, false, 2); //Send not retained at QoS 2
}

// Implement the HAL functions on an Arduino compatible system.
void mfrc630_SPI_transfer(const uint8_t* tx, uint8_t* rx, uint16_t len) {
  for (uint16_t i=0; i < len; i++){
    rx[i] = SPI.transfer(tx[i]);
  }
}

// Select the chip and start an SPI transaction.
void mfrc630_SPI_select() {
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));  // gain control of SPI bus
  digitalWrite(NFCCS, LOW);
}

// Unselect the chip and end the transaction.
void mfrc630_SPI_unselect() {
  digitalWrite(NFCCS, HIGH);
  SPI.endTransaction();    // release the SPI bus
}

/*

String readCDCString(uint32_t timeout = 20) {
  String result = "";
  unsigned long long deadline = millis64() + timeout;
  while (millis64() < deadline) {
    while (//Serial.available() > 0) {
      result += (char)//Serial.read();
    }
    delay(1);
  }
  return result;
}


void CheckforConfig(){
  //Called to see if a config JSON has been sent via USB.
  if(Serial.available()){
    String USBConfig = readCDCString(20);
    JsonDocument ConfigJson;
    deserializeJson(ConfigJson, USBConfig);
    String NewSSID = ConfigJson["SSID"];
    if(ConfigJson["SSID"].is<String>()){
      //Serial.print(F("Set WiFi SSID to: "));
      //Serial.println(NewSSID);
      settings.putString("SSID", NewSSID);
    } else{
      //Serial.println(F("Kept old WiFi SSID."));
    }
    String NewPassword = ConfigJson["Password"];
    if(ConfigJson["Password"].is<String>()){
      //Serial.print(F("Set WiFi password to: "));
      //Serial.println(NewPassword);
      settings.putString("Password", NewPassword);
    } else{
      //Serial.println(F("Kept old WiFi password."));
    }
    String NewServer = ConfigJson["Server"];
    if(ConfigJson["Server"].is<String>()){
      //Serial.print(F("Set server to: "));
      //Serial.println(NewServer);
      settings.putString("Server", NewServer);
    } else{
      //Serial.println(F("Kept old server."));
    }
    String NewKey = ConfigJson["Key"];
    if(ConfigJson["Key"].is<String>()){
      //Serial.println(F("Set a new key (not printed for security)"));
      settings.putString("Key", NewKey);
    } else{
      //Serial.println(F("Kept old key."));
    }
    String NewTimezone = ConfigJson["Timezone"];
    if(ConfigJson["Timezone"].is<String>()){
      //Serial.print(F("Set timezone to: "));
      //Serial.println(NewTimezone);
      settings.putString("Timezone", NewTimezone);
    } else{
      //Serial.println(F("Kept old timezone."));
    }
    String NewMakerspaceID = ConfigJson["MakerspaceID"];
    if(ConfigJson["MakerspaceID"].is<String>()){
      //Serial.print(F("Set makerspace ID to: "));
      //Serial.println(NewMakerspaceID);
      settings.putString("MakerspaceID", NewMakerspaceID);
    } else{
      //Serial.println(F("Kept old makerspace ID."));
    }
    String NewChannelCount = ConfigJson["ChannelCount"];
    if(ConfigJson["ChannelCount"].is<String>()){
      //Serial.print(F("Set Channel Count to: "));
      //Serial.println(NewChannelCount);
      settings.putString("ChannelCount", NewChannelCount);
    } else{
      //Serial.println(F("Kept old ChannelCount."));
    }
    String NewInputMode = ConfigJson["InputMode"];
    if(ConfigJson["InputMode"].is<String>()){
      //Serial.print(F("Set InputMode to: "));
      //Serial.println(NewInputMode);
      settings.putString("InputMode", NewInputMode);
    } else{
      //Serial.println(F("Kept old InputMode"));
    }
    String NewStationName = ConfigJson["StationName"];
    if(ConfigJson["StationName"].is<String>()){
      //Serial.print(F("Set Station Name to: "));
      //Serial.println(NewStationName);
      settings.putString("StationName", NewStationName);
    } else{
      //Serial.println(F("Kept old StationName"));
    }
    int NewMakerspaceNumber = ConfigJson["MakerspaceNumber"];
    if(ConfigJson["MakerspaceNumber"].is<int>()){
      //Serial.println(F("Set makerspace number to: "));
      //Serial.println(NewMakerspaceNumber);
      settings.putInt("SpaceNum", NewMakerspaceNumber);
    } else{
      //Serial.println(F("Kept old MakerspaceNumber."));
    }
    String NewInterruptResponse = ConfigJson["InterruptResponse"];
    if(ConfigJson["InterruptResponse"].is<String>()){
      //Serial.print(F("Set interrupt response to: "));
      //Serial.println(NewInterruptResponse);
      settings.putString("IntResp", NewInterruptResponse);
    } else{
      //Serial.println(F("Kept old InterruptResponse."));
    }
    if (ConfigJson["TapDuration"].is<JsonArray>()) {
      JsonArray durations = ConfigJson["TapDuration"].as<JsonArray>();

      if (durations.size() == 4) {
        //Serial.print(F("Set Tap Durations (seconds) to: ["));
        for (int i = 0; i < 4; i++) {
          uint32_t dur = durations[i].as<uint32_t>();
          
          // Unique key for each channel (e.g. "TapDur0", "TapDur1"...)
          // Note: ESP32 Preferences keys must be 15 characters or less
          String key = "TapDur" + String(i);
          settings.putUInt(key.c_str(), dur);

          //Serial.print(dur);
          if (i < 3) //Serial.print(F(", "));
        }
        //Serial.println(F("]"));
      } else {
        //Serial.println(F("Error: TapDuration must contain exactly 4 values. Kept old values."));
      }
    } else {
      //Serial.println(F("Kept old TapDuration."));
    }
    //Serial.println(F("Above settings have been saved to memory. Restart device to apply settings."));
  }
}

*/

String getBaseMacAddress() {
  uint8_t baseMac[6];
  char macStr[18]; 

  // We use (esp_mac_type_t) to force compatibility with the interface constant
  // If ESP_IF_WIFI_STA still fails, you can try 0 (which is the index for STA)
  if (esp_read_mac(baseMac, (esp_mac_type_t)ESP_IF_WIFI_STA) == ESP_OK) {
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             baseMac[0], baseMac[1], baseMac[2], 
             baseMac[3], baseMac[4], baseMac[5]);
    return String(macStr);
  } else {
    return String("00:00:00:00:00:00");
  }
}



String getSHA256(String input) {
  // Create a buffer to hold the 32-byte (256-bit) hash output
  byte shaResult[32];
  
  // Initialize the mbedTLS message digest context
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  
  // Provide the input string and its length to the hash function
  mbedtls_md_update(&ctx, (const unsigned char*) input.c_str(), input.length());
  
  // Finalize the hash computation and store it in shaResult
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
  
  // Convert the 32-byte binary hash into a readable Hex String
  String hashStr = "";
  for(int i=0; i<32; i++) {
    if(shaResult[i] < 16) {
      hashStr += "0"; // Add leading zero for single-digit hex values
    }
    hashStr += String(shaResult[i], HEX);
  }
  
  return hashStr;
}
