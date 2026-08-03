//ACS V3.0.0 Hardware, running CoreDuino code.

#define Version "2.2.0"
#define Hardware "3.0.0"

//How often you send a status message, in milliseconds
#define STATUS_INTERVAL 15000 

//Pin Definitions:
  #define SCREEN4 35
  #define IODIR1 36
  #define ETHCS 37
  #define ETHRST 38
  #define GPIO2 39
  #define GPIO1 40
  #define DEBUGLED 41
  #define IODIR2 42
  #define TX 39
  #define RX 40
  #define IODIR4 45
  #define ACCESS 46
  #define SCREEN5 48
  #define SCREEN6 34
  #define NFCCS 33
  #define DET1 47
  #define DET2 26
  #define INTERRUPT 21
  #define ONEWIRE 18
  #define SCREEN1 17
  #define SCREEN2 16
  #define SCREEN3 15
  #define ETHINT 14
  #define SCKPin 13
  #define MOSIPin 12
  #define MISOPin 11
  #define GPIO3 10
  #define SDA 9
  #define VARIANT 8
  #define LED 7
  #define IRQ 6
  #define IODIR3 5
  #define PDOWN 4
  #define SCL 3
  #define GPIO4 2
  #define BUZZER 1
  #define BUTTON 0

//Libraries:
  #include <OneWire.h>              //Replacing for WSACS API update...
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
  #include "esp32s3/rom/rtc.h"      //Version 3.1.1 | Inherent to ESP32 Arduino
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
  #include "SparkFun_LIS2DH12.h" //Click here to get the library: http://librarymanager/All#SparkFun_LIS2DH12
  #include <Wire.h>
  #include <mbedtls/md.h>          //Inherent to ESP32
  #include <stdio.h>
  #include <esp_crc.h>  // ESP32 built-in CRC header

  #include "Device.h" //Struct definition in a header so it can be used in multiple places and in function calls

//Objects:
  Preferences settings;
  WiFiClientSecure client;
  JsonDocument ConfigJson;
  HTTPClient http;
  ESP32OTAPull ota;
  ESP32Time rtc;
  WebSocketsClient socket;
  MQTTPubSub::PubSubClient<1024> mqtt;
  OneWire ds(ONEWIRE); 
  Adafruit_NeoPixel CBI(1, LED, NEO_RGB + NEO_KHZ800);
  SPARKFUN_LIS2DH12 accel;  
  //USBCDC Serial;

extern "C" bool verifyRollbackLater() {
  //This code is run to verify the OTA before actual setup.
  //Since we are handling OTA verification ourselves, we just return true.
  return true;
}

NetworkClientSecure networkclient;

String RootCert; //Stores the root certificate loaded from SPIFFS

//Variables - Inter-Task Communication
bool GamerMode = 1;  //Set to 0 to disable gamer mode, i.e. cycle RGB. Used during boot.
bool RequestReset = 0; //Other tasks can set this to 1 to tell the ResetController to start a restart. 
String ResetReason; //Tells the RestartController why we are restarting.
bool Access = 0; //Used to tell the frontend to enable the access signal to the bus. 
String UID; //Stores the UID of the user currently using the machine.
String FoundUID = ""; //Stores the last-found UID
bool ChangeBeep = 0; //Lets the frontend know to beep.
bool FaultBeep = 0; //We use the 3 beep normally for fault to indicate cannot welcome/auth due to no network, to differentiate from welcome/auth denied.

//Variables - Hours
JsonDocument HoursDoc;      // Global document to hold the latest hours
int MakerspaceNumber = 36;  // number from the makerspace's URL. We need to hard-code this for now.

//Variables - System State
String HWVer = "0.0.0"; //Hardware version, loaded from memory for use in OTA.
bool Identify = 0; //Set to 1 to play an identification alarm/buzzer.
String InputMode = "INSERT"; //Stores how we ingest cards.
String DefaultInputMode = "INSERT"; //Stores how we should ingest cards, when not in welcome mode.
bool PendingApproval = 0; //Set to 1 when we have a card present that hasn't been authed yet, this is used for LED animations. 
bool AccessDenied = 0; //Set to 1 when a card is present but has been denied, for LED animations. 
bool CardPresent = 0; //Used to track if there is a card present in the machine.
bool LockWhenIdle = 0;
bool RestartWhenUnused = 0;
bool WelcomeMode = 0; //If 1, we are acting as a welcome reader and not a normal reader.
bool NoNetwork = 1;
bool ScheduledRestart = false; //Used to indicate it is time for a regular restart. 
unsigned long long ScheduledRestartTime = 0; //Used to give the user some breathing room before a shutdown occurs.
bool ImminentShutdown = false; //Used to let the frontend know to play a flashing warning light
String TapUID; //Stores the UID between cycles for comparison when in tap mode.
bool UserWelcomed = 0;
unsigned long long NextStatusTime = 0;

//Variables - Config
String SerialNumber;
String Password;
String SSID = "";
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
bool SendAuth = 0;
bool StateChange = 0;
bool ReportConfig = 0;
bool RequestInfo = 0;
bool SendStatus = 0;
bool SendWelcome = 0;

Device sensorList[10];

//Variables - Inter-Task Communication
bool ConfigOneWire = 0; //Flag to see if we should apply a config to the attached onewire device
bool SealBroken = 0;  //Set to 1 if there is an incorrect OneWire device on the bus. 
bool ReSealBus = 0;
bool OverTemp = 0;    //Set to 1 if there is a device overtemperature on the bus, so we can fault. 
byte liveAddresses[5][8];      //OneWire addresses of what is currently connected, for server reporting.
int liveAddressCount = 0;                //Number of currently connected devices on the bus, for server reporting.
byte deviceCount = 0; //Tracks the number of OneWire devices found on the bus.

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

//Variables - Per-channel tracking (4 channels)
byte ChannelCount = 0; //Tracks how many channels are actually present, based on OneWire detection.
bool ChannelAccess[4] = {0, 0, 0, 0};
String State[4] = {"UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN"};
String LastState[4] = {"UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN"}; //What state we used to be in, used for state change detect.
String StateChangeReason[4];
String AuthReason[4];
String PreservedLastState[4] = {"UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN"}; //What state we used to be in, but kept until we send it to the server.
unsigned long TapDuration[4] = {0, 0, 0, 0}; //How long a temporary tap is kept valid for.
unsigned long long CurrentTapExpires[4] = {0, 0, 0, 0}; //When each channel's timer decays.
volatile unsigned long HobbsSeconds[4] = {0, 0, 0, 0}; //Tracks how long the equipment has been running for.

//Interrupt Response Mode:
//The device can respond to an interrupt in a few different ways;
//1: "FAULT" - Immediately put the device into a fault state.
  //This is the normal operation of the interrupt pin
//2: "LOCK_TEMP" - Immediately set all channels to locked, but return to IDLE once we are no longer interrupted.
//3: "IDLE" - Put any unlocked channels into an idle state.
  //This lets the interrupt pin be used more like a "log out" button.
//4: "MESSAGE" - Simply notify the server a fauly occurred, but don't do anything.
  //Useful for situtations where interrupt is used to convey info, but not necessarily shut down access.
String InterruptResponse = "FAULT";
bool IsInterrupted = false; //Tracks if we are in a maintained interrupt mode, so we do not constantly re-assert states.
byte InterruptCount = 0; //Counts how many times we read an interrupt as we cycle, for debouncing.

//Variables related to any connected screen;
bool UpdateScreen = false;
String FaultReason = "";
String HMIMachineName[4] = {"","","",""};
String HMIMakerspace;
String HMIDeviceName;
String HMIRole;
String StationName;
String motd;

void IRAM_ATTR onTimerCallback(void* arg); //IRAM task for the Hobbs counter

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

  //Set up GPIO all as outputs
  //IODIR HIGH sets level shifter to output mode
  pinMode(IODIR1, OUTPUT);
  digitalWrite(IODIR1, HIGH);
  pinMode(IODIR2, OUTPUT);
  digitalWrite(IODIR2, HIGH);
  pinMode(IODIR3, OUTPUT);
  digitalWrite(IODIR3, HIGH);
  pinMode(IODIR4, OUTPUT);
  digitalWrite(IODIR4, HIGH);
  pinMode(GPIO1, OUTPUT);
  pinMode(GPIO2, OUTPUT);
  pinMode(GPIO3, OUTPUT);
  pinMode(GPIO4, OUTPUT);

  Serial.begin(115200);
  Serial0.begin(115200, SERIAL_8N1, 44, 43);

  Serial.println(F("STARTUP"));
  Serial.flush();
  delay(500);

  sendStartup("Starting Tasks...");

  xTaskCreate(AVControl, "AVControl", 2048, NULL, 5, NULL);
  xTaskCreate(RestartController, "RestartController", 2048, NULL, 5, NULL);

  //Start i2C
  Wire.begin(SDA, SCL);
  accel.begin();
  accel.setScale(LIS2DH12_2g);
  accel.setDataRate(LIS2DH12_ODR_10Hz);

  //Start SPIFFS:
  if(!SPIFFS.begin(1)){
    Serial.println(F("SPIFFS Mount Failed!"));
    delay(1000);
    ESP.restart();
  }

  //Load the TLS cert from SPIFFS
  File file = SPIFFS.open("/cert.txt", FILE_READ);
  if(!file){
    Serial.println(F("No cert found in SPIFFS!"));
    RootCert = "Nothing here!";
  } else{
    RootCert = "";
    while(file.available()){
      RootCert += (char)file.read();
    }
  }

  //Load settings from memory

  //Get our serial number;
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

  //Get our MAC address for printing, in V3.0.0 hardware this is our base MAC
  Serial.print(F("WiFi MAC Address: "));
  Serial.println(getBaseMacAddress());

  settings.begin("settings", false);

  Serial.println(F("Have a config for me? I'll wait a second..."));
  delay(1000);
  CheckforConfig();

  if(!settings.isKey("Server")){
    //We don't have a valid config?
    sendStartup("ERROR: Missing Config!");
    while(1){
      Serial.println(F("Missing config. Please provide a JSON with the following keys: "));
      Serial.println(F("WiFi SSID, WiFi Password, Server, Server Key, Timezone, MakerspaceID"));
      Serial.print(F("Device WiFi MAC Address: "));
      Serial.println(getBaseMacAddress());
      Serial.print(F("Device Serial Number: "));
      Serial.println(SerialNumber);
      CheckforConfig();
      delay(1000);
    }
  }

  if(!settings.isKey("HWVer")){
    //HWVer is new in 2.2.0, so firmware can run in similar hardware.
    //Assume any deployed hardware is 3.0.0
    settings.putString("HWVer", Hardware);
  }
  HWVer = settings.getString("HWVer");

  if(!settings.isKey("ChannelCount")){
    //ChannelCount is new in 2.1.4, set to 1 if no value
    settings.putString("ChannelCount", "1");
  }
  ChannelCount = settings.getString("ChannelCount").toInt();

  if(!settings.isKey("TapDur0")){
    //Tap Duration is new in 2.1.4, set to 0 if no value.
    settings.putUInt("TapDur0", 0);
    settings.putUInt("TapDur1", 0);
    settings.putUInt("TapDur2", 0);
    settings.putUInt("TapDur3", 0);
  }
  for(int i = 0; i < 4; i++){
    String key = "TapDur" + String(i);
    TapDuration[i] = settings.getUInt(key.c_str());
  }

  if(!settings.isKey("InputMode")){
    //InputMode is new in 2.1.4, set to "INSERT" if no value.
    settings.putString("InputMode", "INSERT");
  }
  DefaultInputMode = settings.getString("InputMode");
  InputMode = DefaultInputMode;

  if(!settings.isKey("IntResp")){
    //Interrupt Response is new in 2.1.4, set to "FAULT" if no value.
    settings.putString("IntResp", "FAULT");
  }
  InterruptResponse = settings.getString("IntResp");

  if(!settings.isKey("StationName")){
    //StationName is new in 2.1.4, set to "Generic ACS" if no value.
    settings.putString("StationName", "Generic MakeACS");
  }
  StationName = settings.getString("StationName");

  if(!settings.isKey("SpaceNum")){
    //MakerspaceNumber (SpaceNum) is new in 2.1.4, set to 36 (Atrium Makerspace) if no value.
    settings.putInt("SpaceNum", 36);
  }
  MakerspaceNumber = settings.getInt("SpaceNum");

  //Get the reset reason;
  if(!settings.isKey("ResetReason")){
    //We don't know why we reset?

  } else{
    ResetReason = settings.getString("ResetReason");
    settings.remove("ResetReason"); //So we know we read it.
  }

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

  Serial.println(F("Settings loaded."));
  Serial.flush();

  sendStartup("Settings Loaded.");

  Serial.println(F("Started Tasks."));
  Serial.flush();

  //Start SPI here, in case we want to use Ethernet in the future. 
  //SCK, MISO, MOSI, SS
  pinMode(NFCCS, OUTPUT);
  SPI.begin(SCKPin, MISOPin, MOSIPin, -1);

  Serial.println(F("Started SPI."));
  Serial.flush();

  sendStartup("Starting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, Password);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  unsigned long WiFiStart = millis64();

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();  //Force a manual connect attempt
    Serial.println(F("Waiting for first WiFi connect"));
    while (WiFi.status() != WL_CONNECTED && millis64() - WiFiStart < 15000) {
      Serial.print(".");
      delay(500);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi connected."));
    sendStartup("WiFi Started.");

    // Also get rid of "No NET" on screen
    JsonDocument NoNetStart;
    NoNetStart["noNetwork"] = false;
    String NoNetToSend;
    serializeJson(NoNetStart, NoNetToSend);
    Serial0.println(NoNetToSend);

    delay(500);

    // --- OTA LOGIC STARTS HERE ---
    // We only verify and check for updates if we are actually online.
    sendStartup("Checking for OTA...");
    Serial.println(F("Checking for OTA..."));

    // 1. Configure all OTA settings first
    ota.EnableSerialDebug();
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
      Serial.print(F("OTA Response: "));
      Serial.println(errtext(otaresp));
    }
    // --- OTA LOGIC ENDS HERE ---

  } else {
    // Device is offline. We skip OTA checks entirely to avoid false rollbacks.
    Serial.println(F("WiFi failed to connect after timeout! Booting offline."));
    sendStartup("WiFi failed to start?");
  }

  // If we made it past the OTA (or skipped it because offline),
  // then we are ready for normal operation.
  sendStartup("Connecting MQTT...");

  mqtt.begin(socket); //Enable MQTT on the websocket

  NetworkConnect();
  
  //If we cared about why we restarted, this'd be the place to handle it.

  //Start the NFC reader, make sure it is working as expected. 
  mfrc630_AN1102_recommended_registers(MFRC630_PROTO_ISO14443A_106_MILLER_MANCHESTER);
  mfrc630_write_reg(0x28, 0x8E);
  mfrc630_write_reg(0x29, 0x15);
  mfrc630_write_reg(0x2A, 0x11);
  mfrc630_write_reg(0x2B, 0x06);

  //We should initialize the OneWire bus here, check for the right devices, etc.
  Serial.println(F("Starting OneWire..."));
  sendStartup("Starting OneWire...");
  discoverDevices();
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

  //Initialize a precise timer for the Hobbs Timer
  Serial.println(F("Starting Critical Timer for Hobbs Time..."));
  const esp_timer_create_args_t timer_args = {
    .callback = &onTimerCallback,  // The function to run
    .arg = NULL,                   // Arguments passed to the function (optional)
    .name = "one_second_timer"     // Name for debugging
  };
  esp_timer_handle_t periodic_timer;
  err = esp_timer_create(&timer_args, &periodic_timer);
  if (err == ESP_OK) {
    //Start the timer to repeat every 1,000,000 microseconds (1 second)
    esp_timer_start_periodic(periodic_timer, 1000000);
    Serial.println("Timer started successfully!");
  } else {
    Serial.printf("Timer creation failed with error: %d\n", err);
  }

  //Going forward, we will check the OneWire bus in a different task to make life easier.
  xTaskCreate(BusManager, "BusManager", 4096, NULL, 5, NULL);

  //Time to loop!
  xTaskCreate(MachineState, "MachineState", 4096, NULL, 5, NULL);
  GamerMode = 0; //Disable the startup lighting

  xTaskCreate(SceenController, "ScreenController", 4096, NULL, 5, NULL);

}

void loop() {
  // put your main code here, to run repeatedly:

  delay(10);

  //Step 0: Call the MQTT updater;
  mqtt.update();

  //Check for a config JSON:
  CheckforConfig();

  //Step 4: Communicate with the server

  //Only do all this if we have a connection
  if(mqtt.isConnected() && !NoNetwork){

    if(NoNetwork){
      NoNetwork = false;
      UpdateScreen = true;
    }

    JsonDocument outgoing; //Json to construct the outgoing message in

     //Step 4.1: See if we have any outgoing messages, and send them.

     //temp disabled for testing

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
      if(!WelcomeMode){
        //We don't report state change when we are in welcoming.
        JsonArray stateChannels = outgoing["channels"].to<JsonArray>();
        for( int i = 0; i < ChannelCount; i++){
          if(State[i] != PreservedLastState[i]){
            JsonObject stateObject = stateChannels.createNestedObject();
            stateObject["channelID"] = i;
            stateObject["fromState"] = PreservedLastState[i];
            stateObject["toState"] = State[i];
            //The server doesn't recognize the "LOCK_TEMP" state change reason
            //So we replace if with "LOCAL":
            if(StateChangeReason[i] == "LOCK_TEMP"){
              stateObject["reason"] = "LOCAL";
            } else{
              stateObject["reason"] = StateChangeReason[i];
            }
            //Update the preserved last state;
            PreservedLastState[i] = State[i];
          }
        }
        outgoing["currentCardTag"] = UID;
        String StateChangePayload;
        serializeJson(outgoing, StateChangePayload);
        outgoing.clear();
        String StateChangeTopic = BaseTopic + "/stateChange";
        publish(StateChangeTopic, StateChangePayload);
        //At the end, set change reason to nothing:
      }
    }
    if(ReportConfig){
      //Report the current configuration
      ReportConfig = 0;
      JsonArray configChannels = outgoing["channels"].to<JsonArray>();
      for(int i = 0; i < ChannelCount; i++){
        JsonObject configObject = configChannels.createNestedObject();
        configObject["channelID"] = i;
        configObject["tempDuration"] = TapDuration[i];
      }
      outgoing["inputMode"] = InputMode;
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
            // Here we grab the deviceMode and other data from the struct
            deviceObj["type"] = sensorList[j].deviceMode; 
            deviceObj["identifier"] = sensorList[j].deviceID; //Server doesn't expect this yet, but we should send it
            break;
          }
        }
      }
      JsonObject flags = outgoing["flags"].to<JsonObject>();
      flags["lockWhenIdle"] = LockWhenIdle;
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
      //Check if any of the states or HobbsTimers are unknown;
      bool AskForStates = false;
      bool AskForHobbs = false;
      for(int i = 0; i < ChannelCount; i++){
        if(State[i] == "UNKNOWN"){
          AskForStates = true;
        }
        if(HobbsSeconds[i] == 0){
          AskForHobbs = true;
        }
      }
      if(AskForStates){
        //We don't know what state we should be in, so request it. 
        infoFields.add("STATE");
      }
      if(AskForHobbs){
        //We do not know what the Hobbs timer should be at, let's request that.
        infoFields.add("HOBBS_TIME");
      }
      infoFields.add("FLAGS"); //Check our flags, mostly for welcoming
      infoFields.add("HMI"); //Request human-readable info for any attached interface.
      String InfoPayload;
      serializeJson(outgoing, InfoPayload);
      outgoing.clear();
      String InfoTopic = BaseTopic + "/info/request";
      publish(InfoTopic, InfoPayload);
    }
    if(SendStatus && !anyChannelIs("UNKNOWN")){
      //Send our current status to the server, we do not send it if we do not know our state. 
      SendStatus = 0;
      JsonArray statusChannels = outgoing["channels"].to<JsonArray>();
      if(!WelcomeMode){
        //We don't send this in welcoming mode
        for(int i = 0; i < ChannelCount; i++){
          JsonObject statusObject = statusChannels.createNestedObject();
          statusObject["channelID"] = i;
          statusObject["state"] = State[i];
          statusObject["hobbsTime"] = HobbsSeconds[i];
        }
      }
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

    if(NewAuth){
      //Process a response to an auth request.
      NewAuth = 0;
      deserializeJson(incoming, AuthResponse);
      String AuthID = incoming["cardTagID"].as<String>();
      PendingApproval = false;
      bool SendUnlockedBeep = false;
      bool SendAccessDenied = false;
      for(JsonVariant v : incoming["channels"].as<JsonArray>()){
        int ch = v["channelID"] | 0;
        if(ch >= 0 && ch < ChannelCount){
          bool IsAuthed = v["approved"].as<bool>();
          AuthReason[ch] = v["reason"].as<String>();
          
          if(State[ch] == "IDLE" || (State[ch] == "UNLOCKED" && InputMode == "TEMP_PRESENT")){ //Unlock only if idle, or re-up unlocked channels if in tap-present mode.
            if(IsAuthed){
              Serial.println(F("Access Granted!"));
              if(AuthID == UID){
                Serial.println(F("UIDs match. Unlocking."));
                State[ch] = "UNLOCKED";
                StateChangeReason[ch] = "AUTHED"; 
                SendUnlockedBeep = true;
                if(InputMode == "TEMP_PRESENT"){
                  //Add all the times now;
                  CurrentTapExpires[ch] = TapDuration[ch] * 1000 + millis64();
                }
              }
            } else{
              Serial.println(F("Access Denied!"));
              if(CardPresent){
                SendAccessDenied = 1;
              }
            }
          } else{
            Serial.println(F("Ignoring auth due to improper state."));
          }
        }
      }
      if(SendUnlockedBeep){
        //We do it this way so we don't trigger the beep 4 times
        UnlockedBeep = true;
      }
      if(SendAccessDenied){
        AccessDenied = true;
      }
      UpdateScreen = true;
    }
    if(NewInfo){
      //Process a response to an info request.
      NewInfo = 0;
      deserializeJson(incoming, InfoResponse);
      //Set the state;
      if (incoming["state"].is<JsonArray>()) {
        for (JsonObject item : incoming["state"].as<JsonArray>()) {
          int id = item["id"] | -1; // Default to -1 if missing
          
          // Bounds check to avoid crashing the MCU with array out-of-bounds
          if (id >= 0 && id < ChannelCount) {
            State[id] = item["state"].as<String>();
            StateChangeReason[id] = "COMMANDED";
            if(State[id] == "FAULT"){
              //We don't go back to a fault state;
              State[id] = "LOCKED_OUT";
            }
            if (State[id] == "UNLOCKED" || State[id] == "ALWAYS_ON") {
              //We don't go back to an unlocked state;
              State[id] = "IDLE";
            }
          }
        }
        SingleBeep = 1;
      }

      // Process the "hobbsTime" array
      if (incoming["hobbsTime"].is<JsonArray>()) {
        for (JsonObject item : incoming["hobbsTime"].as<JsonArray>()) {
          // Notice this uses "channelID" instead of "id"
          int ch = item["channelID"] | -1; 
          
          if (ch >= 0 && ch < ChannelCount) {
            HobbsSeconds[ch] = item["hobbsTime"].as<unsigned long>(); 
          }
        }
      }
      //Process the HMI info:
      if(incoming.containsKey("hmi")){
        HMIRole = incoming["hmi"]["role"].as<String>();
        HMIDeviceName = incoming["hmi"]["deviceName"].as<String>();
        HMIMakerspace = incoming["hmi"]["makerspace"].as<String>();
        JsonArray channels = incoming["hmi"]["channels"];
        for (JsonObject channel : channels){
          int channelID = channel["channelID"];
          HMIMachineName[channelID] = channel["pairedEntity"].as<String>();
        }
      }
      //Set the time;
      if(incoming.containsKey("time")){
        unsigned long long millisecondTime = incoming["time"];
        rtc.setTime(millisecondTime/1000);
        Serial.print(F("Time set to: "));
        Serial.println(rtc.getDateTime(true));
      }
      //Set flags:
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
        if(flagObj.containsKey("welcoming")){
          if(WelcomeMode != flagObj["welcoming"].as<bool>()){
            WelcomeMode = flagObj["welcoming"].as<bool>();
            if(WelcomeMode){
            Serial.println(F("Server flag set to enter welcoming mode."));
            } else{
              Serial.println(F("Server flag unset for welcoming mode. Entering state 'UNKNOWN'"));
              WelcomeMode = false;
              for(int i = 0; i < ChannelCount; i++){
                State[i] = "UNKNOWN";
                StateChangeReason[i] = "SERVER_COMMANDED";
              }
              //We should ask what state we should be in
              RequestInfo = 1;
            }
          }
        }
      }
      if(incoming.containsKey("hobbsTime")){
        for(int i = 0; i < ChannelCount; i++){
          HobbsSeconds[i] = incoming["hobbsTime"][i]["hobbsTime"];
          Serial.print(F("Hobbs timer for channel "));
          Serial.print(i);
          Serial.print(F(" set to: "));
          Serial.print(HobbsSeconds[i]);
          Serial.println(F(" seconds."));
        }
      }
      ReportConfig = 1; //Once we get some info, we should send our configuration.
      SendStatus = 1; //Once we get some info, we should send our status.
      UpdateScreen = true;
    }
    if(NewCommand){
      //Process an incoming command.
      NewCommand = 0;
      deserializeJson(incoming, CommandResponse);
      //State change command
      if(incoming["toState"].is<JsonArray>()){
        JsonArray toStateArray = incoming["toState"].as<JsonArray>();
        for (JsonVariant v : toStateArray) {
          int ch = v["id"] | -1;
          if (ch >= 0 && ch < ChannelCount) {
            State[ch] = v["state"] | "UNKNOWN";
            if(State[ch] == "UNLOCKED" && !CardPresent){
              State[ch] = "IDLE";
            }
            StateChangeReason[ch] = "COMMANDED";
          }
        }
        SingleBeep = 1;
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
        if(flagObj.containsKey("welcoming")){
          if(WelcomeMode != flagObj["welcoming"].as<bool>()){
            WelcomeMode = flagObj["welcoming"].as<bool>();
            if(WelcomeMode){
            Serial.println(F("Server flag set to enter welcoming mode."));
            } else{
              Serial.println(F("Server flag unset for welcoming mode. Entering state 'UNKNOWN'"));
              for(int i = 0; i < ChannelCount; i++){
                State[i] = "UNKNOWN";
                StateChangeReason[i] = "SERVER_COMMANDED";
              }
              //We should ask what state we should be in
              RequestInfo = 1;
            }
          }
        }
      }
      //Set HobbsTime
      if(incoming.containsKey("hobbsTime")){
        JsonArray hobbsTimeArray = incoming["hobbsTime"].as<JsonArray>();
        for (JsonVariant v : hobbsTimeArray) {
          int ch = v["hobbsTime"] | 0;
          if (ch >= 0 && ch < ChannelCount) {
            HobbsSeconds[ch] = v["channelID"] | 0;
            Serial.print(F("Hobbs timer for channel "));
            Serial.print(ch);
            Serial.print(F(" set to: "));
            Serial.print(HobbsSeconds[ch]);
            Serial.println(F(" seconds."));
          }
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
          if(!Identify){
            //Play a single beep to end the identify command.
            SingleBeep = true;
          }
        }
        if(incoming["action"] == "SCHEDULED_RESTART"){
          Serial.println(F("Server indicated it is time for a scheduled restart."));
          ScheduledRestart = true;
        }

      }
      UpdateScreen = true;
    }
    if(NewWelcome){
      //Response to welcoming a user
      NewWelcome = 0;
      deserializeJson(incoming, WelcomeResponse);
      bool IsWelcomed = incoming["welcomed"];
      String WelcomeID = incoming["cardTagID"];
      String WelcomeReason = incoming["reason"];
      if(IsWelcomed){
        //User was welcomed into the space properly.
        Serial.println(F("User welcomed!"));
        if(UID == WelcomeID){
          //The user's card is still here, so beep and light up.
          UserWelcomed = 1;
        } else{
          Serial.println(F("But their card isn't here anymore, so we will skip the lights/sounds."));
        }
      } else{
        //User was denied entry into the space.
        Serial.print(F("User denied! Reason: "));
        Serial.println(WelcomeReason);
        AccessDenied = 1; //Act like we denied the user access
      }
      UpdateScreen = true;
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
    UpdateScreen = true;
    NetworkConnect();
  }

}

void callback_percent(int offset, int totallength) {
  //Used to display percentage of OTA installation

  static int prev_percent = -1;
  int percent = 100 * offset / totallength;
  if (percent != prev_percent) {
    Serial.printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
    prev_percent = percent;
    //We should also send it to any attached screen;
    JsonDocument CoreOTA;
    CoreOTA["coreOta"] = percent;
    String CoreOTAString;
    serializeJson(CoreOTA, CoreOTAString);
    Serial0.println(CoreOTAString);
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
  byte TLSRetryCount = 0; //Tracks how many failed TLS attempts we had in a row.
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
  const char* global_ca_pointer = RootCert.c_str();
  socket.beginSslWithCA(Server.c_str(), 443, "/mqtt", global_ca_pointer, "mqtt");
  //Give the socket some time to stabilize:
  unsigned long long wsTimeout = millis64() + 5000;
  while(!socket.isConnected() && millis64() <= wsTimeout){
    socket.loop();
    delay(2);
  }
  //Did the socket work?
  if(!socket.isConnected()){
    Serial.println(F("Websocket connection failed..."));
    socket.disconnect();
    //Is the server alive?
    if(!Ping.ping(Server.c_str())){
      //Server is not responding?
      Serial.println(F("Cannot ping the server. No network?"));
      NoNetwork = true;
      return;
    } else{
      Serial.println(F("Server is online. Bad TLS cert?"));
      Serial.println(F("Trying TLS certs again to make sure..."));
      if(TLSRetryCount <=5){
        Serial.print(F("That was attempt: "));
        Serial.print(TLSRetryCount);
        Serial.println(F("/5 attempts before we get new certs."));
        delay(1000);
        TLSRetryCount++;
        goto retryNetwork;
      }
      Serial.println(F("Getting new TLS certs from server."));
      networkclient.setInsecure();
      networkclient.connect(Server.c_str(), 443);
      
      networkclient.print("GET /api/rootCA HTTP/1.1\r\n");
      networkclient.print("Host: ");
      networkclient.print(Server.c_str());
      networkclient.print("\r\n");

      networkclient.print("shlug-sn: ");
      networkclient.print(SerialNumber.c_str());
      networkclient.print("\r\n");
      
      networkclient.print("Connection: close\r\n");
      
      // End of headers boundary
      networkclient.print("\r\n");

      while (networkclient.connected()) {
        String line = networkclient.readStringUntil('\n');
        if (line == "\r") {
          Serial.println("Headers received, body:");
          break;
        }
      }
      unsigned long timeout = millis();
      while (networkclient.available() == 0) {
        if (millis() - timeout > 5000) { // 5 second timeout
          Serial.println("!!! Client Timeout awaiting body! !!!");
          networkclient.stop();
          return;
        }
        delay(10); 
        NoNetwork = true;
        goto retryNetwork;
      }

      // The body is a JSON, let's capture it in a string.
      String TLSPayload;
      while(networkclient.available()){
        char c = networkclient.read();
        TLSPayload += c;
      }
      
      Serial.println(TLSPayload);
      networkclient.stop(); // Always close the socket when finished!

      //Parse the JSON payload
      JsonDocument TLSJson;
      deserializeJson(TLSJson, TLSPayload);
      //Before we accept the new cert, we should check the SHA-256
      String SHATLS = TLSJson["sha"];
      String NewCert = TLSJson["cert"];
      //The SHA is the hash of "[SerialNumber]:[Password]:[Cert]""
      Serial.print(F("JSON Hash:       ")); Serial.println(SHATLS);
      Serial.print(F("Calculated Hash: ")); Serial.println(getSHA256(SerialNumber + ":" + Key + ":" + NewCert));
      if(SHATLS.equalsIgnoreCase(getSHA256(SerialNumber + ":" + Key + ":" + NewCert))){
       //The hashes match!
       Serial.println(F("TLS cert was verified. Saving to memory..."));
       SPIFFS.remove("/cert.txt");
       File file = SPIFFS.open("/cert.txt", FILE_WRITE);
       //Need to change the written /n to an actual newline, clean up any other oddities in the file:
       NewCert.replace("\\n","\n");
       NewCert.replace("\r","");
       NewCert.replace("\"","");
       NewCert.trim();
       NewCert += "\n";
       if(file.print(NewCert)){
        file.close();
        RootCert = NewCert;
        Serial.println(F("New cert has been saved. Regular operation can now resume."));
        Serial.println(F("Our new cert is:"));
        Serial.println(RootCert);
        Serial.flush();
        delay(10);
        goto retryNetwork;
       } else{
        NoNetwork = true;
        Serial.println(F("Unknown error, could not write new cert to file?"));
        goto retryNetwork;
       }

      } else{
        //The hashes did not match, potental attack in progress!
        NoNetwork = true;
        FaultReason = "TLS hash does not match!";
        Serial.println(F("CRITICAL ERROR: ATTEMPT WAS MADE TO LOAD BAD TLS CERTS!"));
        //Message = "Attmpted to load cert with bad hash?";
        //MessageToSend = true;
        delay(1000);
        goto retryNetwork;
      }
    }
  } //If not this, the connection worked and we can continue.
  socket.setReconnectInterval(2000); //Attempt to reconnect every 2 seconds if we lose connection
  Serial.println(F("Connecting to MQTT Broker"));
  unsigned long long SocketTime = millis64() + 15000;
  while(!mqtt.connect(SerialNumber, SerialNumber, Key)){ //Use serial number as unique ID, username, and key as password.
    Serial.print(".");
    socket.loop();
    delay(500);
    if(SocketTime <= millis64()){
      Serial.println(F("Failed to connect to websocket! Retrying network altogether..."));
      goto retryNetwork;
    }
  } 
  Serial.println(F(" MQTT Connected!"));
  NoNetwork = false;

  //Subscribe to all MQTT topics relevant to us;
  BaseTopic = "makerspace/device/" + SerialNumber;
  String SubAuth = BaseTopic + "/authTo/response";
  mqtt.subscribe(SubAuth, 2, [](const String& payload, const size_t size) {
    Serial.print(F("AuthTo Response: "));
    Serial.println(payload);
    AuthResponse = payload;
    NewAuth = 1;
  });
  String SubInfo = BaseTopic + "/info/response";
  mqtt.subscribe(SubInfo, 2, [](const String& payload, const size_t size) {
    Serial.print(F("Info Response: "));
    Serial.println(payload);
    InfoResponse = payload;
    NewInfo = 1;
  });
  String SubCommand = BaseTopic + "/command";
  mqtt.subscribe(SubCommand, 2, [](const String& payload, const size_t size) {
    Serial.print(F("Command Input: "));
    Serial.println(payload);
    CommandResponse = payload;
    NewCommand = 1;
  });
  String SubWelcome = BaseTopic + "/welcome/response";
  mqtt.subscribe(SubWelcome, 2, [](const String& payload, const size_t size) {
    Serial.print(F("Welcome Response: "));
    Serial.println(payload);
    WelcomeResponse = payload;
    NewWelcome = 1;
  });
  String SubPing = BaseTopic + "/ping";
  mqtt.subscribe(SubPing, 2, [](const String& payload, const size_t size) {
    //Serial.println(F("Ping Loopback."));
    NewPing = 1;
  });

  NoNetwork = false;
  UpdateScreen = true;

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

String readCDCString(uint32_t timeout = 20) {
  String result = "";
  unsigned long long deadline = millis64() + timeout;
  while (millis64() < deadline) {
    while (Serial.available() > 0) {
      result += (char)Serial.read();
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
    //Is this a Core Config JSON, or a OneWire Config JSON?
    if(ConfigJson["Type"] == "OneWire"){
      //This is a onewire json, let's tell the bus manager to handle it.
      ConfigOneWire = true;
      return;
    }
    String NewSSID = ConfigJson["SSID"];
    if(ConfigJson["SSID"].is<String>()){
      Serial.print(F("Set WiFi SSID to: "));
      Serial.println(NewSSID);
      settings.putString("SSID", NewSSID);
    } else{
      Serial.println(F("Kept old WiFi SSID."));
    }
    String NewPassword = ConfigJson["Password"];
    if(ConfigJson["Password"].is<String>()){
      Serial.print(F("Set WiFi password to: "));
      Serial.println(NewPassword);
      settings.putString("Password", NewPassword);
    } else{
      Serial.println(F("Kept old WiFi password."));
    }
    String NewServer = ConfigJson["Server"];
    if(ConfigJson["Server"].is<String>()){
      Serial.print(F("Set server to: "));
      Serial.println(NewServer);
      settings.putString("Server", NewServer);
    } else{
      Serial.println(F("Kept old server."));
    }
    String NewKey = ConfigJson["Key"];
    if(ConfigJson["Key"].is<String>()){
      Serial.println(F("Set a new key (not printed for security)"));
      settings.putString("Key", NewKey);
    } else{
      Serial.println(F("Kept old key."));
    }
    String NewTimezone = ConfigJson["Timezone"];
    if(ConfigJson["Timezone"].is<String>()){
      Serial.print(F("Set timezone to: "));
      Serial.println(NewTimezone);
      settings.putString("Timezone", NewTimezone);
    } else{
      Serial.println(F("Kept old timezone."));
    }
    String NewMakerspaceID = ConfigJson["MakerspaceID"];
    if(ConfigJson["MakerspaceID"].is<String>()){
      Serial.print(F("Set makerspace ID to: "));
      Serial.println(NewMakerspaceID);
      settings.putString("MakerspaceID", NewMakerspaceID);
    } else{
      Serial.println(F("Kept old makerspace ID."));
    }
    String NewChannelCount = ConfigJson["ChannelCount"];
    if(ConfigJson["ChannelCount"].is<String>()){
      Serial.print(F("Set Channel Count to: "));
      Serial.println(NewChannelCount);
      settings.putString("ChannelCount", NewChannelCount);
    } else{
      Serial.println(F("Kept old ChannelCount."));
    }
    String NewInputMode = ConfigJson["InputMode"];
    if(ConfigJson["InputMode"].is<String>()){
      Serial.print(F("Set InputMode to: "));
      Serial.println(NewInputMode);
      settings.putString("InputMode", NewInputMode);
    } else{
      Serial.println(F("Kept old InputMode"));
    }
    String NewStationName = ConfigJson["StationName"];
    if(ConfigJson["StationName"].is<String>()){
      Serial.print(F("Set Station Name to: "));
      Serial.println(NewStationName);
      settings.putString("StationName", NewStationName);
    } else{
      Serial.println(F("Kept old StationName"));
    }
    int NewMakerspaceNumber = ConfigJson["MakerspaceNumber"];
    if(ConfigJson["MakerspaceNumber"].is<int>()){
      Serial.println(F("Set makerspace number to: "));
      Serial.println(NewMakerspaceNumber);
      settings.putInt("SpaceNum", NewMakerspaceNumber);
    } else{
      Serial.println(F("Kept old MakerspaceNumber."));
    }
    String NewInterruptResponse = ConfigJson["InterruptResponse"];
    if(ConfigJson["InterruptResponse"].is<String>()){
      Serial.print(F("Set interrupt response to: "));
      Serial.println(NewInterruptResponse);
      settings.putString("IntResp", NewInterruptResponse);
    } else{
      Serial.println(F("Kept old InterruptResponse."));
    }
    if (ConfigJson["TapDuration"].is<JsonArray>()) {
      JsonArray durations = ConfigJson["TapDuration"].as<JsonArray>();

      if (durations.size() == 4) {
        Serial.print(F("Set Tap Durations (seconds) to: ["));
        for (int i = 0; i < 4; i++) {
          uint32_t dur = durations[i].as<uint32_t>();
          
          // Unique key for each channel (e.g. "TapDur0", "TapDur1"...)
          // Note: ESP32 Preferences keys must be 15 characters or less
          String key = "TapDur" + String(i);
          settings.putUInt(key.c_str(), dur);

          Serial.print(dur);
          if (i < 3) Serial.print(F(", "));
        }
        Serial.println(F("]"));
      } else {
        Serial.println(F("Error: TapDuration must contain exactly 4 values. Kept old values."));
      }
    } else {
      Serial.println(F("Kept old TapDuration."));
    }
    Serial.println(F("Above settings have been saved to memory. Restart device to apply settings."));
  }
}

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

void sendStartup(String Message){
  JsonDocument Startup;
  Startup["startupMessage"] = Message;
  String StartMessageString;
  serializeJson(Startup, StartMessageString);
  Serial0.println(StartMessageString);
  Serial0.flush();
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

void IRAM_ATTR onTimerCallback(void* arg) {
  //This is called in an ISR to increment the Hobbs timer very precisely!
  for(int i = 0; i < ChannelCount; i++){
    if(ChannelAccess[i]){
      HobbsSeconds[i]++;
    }
  }
}

