/*

----- Access Control System CYD Firmware -----

  This firmware running on an ESP32 "Cheap Yellow Display" (E32R28T in our case) allows
  the screen to show ACS state. A standard UART communication allows the Core to tell
  the screen what should be displayed.

  The program is split into a few main tasks;
  * screen manager (running in loop)- preps information for the screen, informs the server what to show based on communication with the Core
  * screen server - handles what is actually displayed, sprites, animations, etc.

*/

#define FW_VER "1.0.0"

//There are 2 possible hardwares this can run on;

//If the MCU type is an ESP32-S3, this is a Freenove FNK0104A
#ifdef CONFIG_IDF_TARGET_ESP32S3
  #define HARDWARE_TYPE "FNK0104A"
  #define HARDWARE_MFGR "Freenove"
  #define HAS_WIFI true
  #define HARDWARE_DIMMABLE true
  #define DISP_TYPE "IPS"
  #define DISP_SIZE "2.8 Inch"
  #define DISP_RESOLUTION "240 x 320"
  #define SD_MMC_CMD 40
  #define SD_MMC_CLK 38
  #define SD_MMC_D0 39
  #define SD_MMC_D1 41
  #define SD_MMC_D2 48
  #define SD_MMC_D3 47
  #define I2S_MCLK 4
  #define I2S_LRCK 7
  #define I2S_SCLK 5
  #define I2S_DOUT 6 
  #define I2S_DIN 8
  #define I2C_SDA 16
  #define I2C_SCL 15
  //We should also change the user setup file based on this here.

//If the MCU type is ESP32, then this is an E32R28T from DIYMalls
#elif defined(ESP32)
  #define HARDWARE_TYPE "E32R28T"
  #define HARDWARE_MFGR "DIYmalls"
  //We should also change the user setup file based on this here.

#endif

#include <TFT_eSPI.h> 
#include <SPI.h>
#include "Free_Fonts.h"
#include <ArduinoJson.h>          //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.hpp>        //Version 7.3.0 | Source: https://github.com/bblanchon/ArduinoJson
#include "esp32s3/rom/rtc.h"      //Version 3.1.1 | Inherent to ESP32 Arduino
#include <ESP32Time.h>
#include "FS.h"
#include "SD_MMC.h"
#include <rom/crc.h>              // ESP32 ROM hardware CRC functions
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include <esp_system.h>
#include <esp_mac.h>
#include "mbedtls/md.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <TJpg_Decoder.h>
#include "Audio.h"

// Global Variables standardized to camelCase
byte currentScene = 0; //Stores what scene we should be showing.
bool noNetwork = true; //If set to true, shows a no network warning over any other scene.
String acsState = "UNKNOWN"; // Renamed from State to avoid ambiguity
String mode = "INSERT";
byte setRotation = 5;
bool isWelcoming = 0;
byte channels = 0; //How many access controller channels we expect to display info on
String startupMessage = "Waiting for Core..."; //Used to indicate state during startup
String motd = ""; 
String faultMessage = "";
String filedScene = "";
String lastFiledScene = "";
bool forceUpdateScreen = false;

volatile int otaProgressPercent = 0;

bool hasSpeaker = false;
bool sceneStorage = false;
String sdCardType;
String sdCardCapacity;

ESP32Time rtc;
Audio audio;

byte volume = 12; // 0 to 21

bool isDenied = 0; //1 if we are denying an access attempt
String deniedReason;
bool identify = false;
bool restartFlag = false;
String url = "make.rit.edu";

// Function prototypes to ensure clean compilation
void screenServer(void *pvParameters); // Assuming this is defined in your second file
uint64_t millis64();
byte decideScene();
String getSerialNumber();
String getBaseMacAddress();
void addDirectoryHashesToJson(JsonArray arrayTarget, const char* directoryPath);
void performSecureOta(const String& jsonPayload);
void sendLog(String message, bool toHistory);

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200, SERIAL_8N1, 44, 43);
  
  xTaskCreate(screenServer, "ScreenServer", 50000, NULL, 4, NULL);
  
  rtc.offset = -4 * 3600; //Default to EST
  
  //Initialize the SD card...
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0, SD_MMC_D1, SD_MMC_D2, SD_MMC_D3);
  if(!SD_MMC.begin()){
    Serial.println(F("SD Card Mount Failed!"));
    sdCardType = "NO CARD";
    sdCardCapacity = "0 MB";
  } else {
    sceneStorage = true;
    
    //Determine the card type
    uint8_t cardType = SD_MMC.cardType();
    Serial.print(F("SD Card Type: "));
    if(cardType == CARD_NONE){
      Serial.println(F("NO CARD FOUND!"));
      sdCardType = "NO CARD";
    }
    else if(cardType == CARD_MMC){
      Serial.println(F("MMC"));
      sdCardType = "MMC";
    }
    else if(cardType == CARD_SD){
      Serial.println(F("SD"));
      sdCardType = "SD";
    }
    else if(cardType == CARD_SDHC){
      Serial.println(F("SDHC"));
      sdCardType = "SDHC";
    }
    else{
      Serial.println(F("UNKNOWN!"));
      sdCardType = "UNKNOWN";
    }
    
    //Determine the card capacity
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.print(F("Card Size: "));
    Serial.print(cardSize);
    Serial.println(F(" MB."));
    sdCardCapacity = String(cardSize) + " MB";
    
    //Does the Card have audio files?
    if(SD_MMC.exists("/audio") && sdCardType != "NO CARD"){
      hasSpeaker = true;
      Serial.println(F("Audio folder found!"));
    }
  }

  //Start Audio player
  //Pending testing
  //xTaskCreate(audioPlayer, "AudioPlayer", 10000, NULL, 5, NULL);

}

void loop() {
  delay(10);

  //First check for anything incoming
  if(Serial0.available()){
    Serial0.setTimeout(5);
    String incoming = Serial0.readString();
    incoming.trim();
    Serial.print(F("Got input: "));
    Serial.println(incoming);
    
    JsonDocument input;
    deserializeJson(input, incoming);

    // Iterate through JSON, keys updated to standard camelCase
    if(input.containsKey("time")){
        unsigned long long millisecondTime = input["time"];
        rtc.setTime(millisecondTime/1000);
        Serial.print(F("Time set to: "));
        Serial.println(rtc.getDateTime(true));
    }
    
    acsState = input["state"] | acsState;
    Serial.print(F("State: ")); Serial.println(acsState);
    
    noNetwork = input["noNetwork"] | noNetwork;
    Serial.print("NoNetwork: "); Serial.println(noNetwork);
    
    isWelcoming = input["welcoming"] | isWelcoming;
    Serial.print("Welcoming: "); Serial.println(isWelcoming);
    
    channels = input["channels"] | channels;
    Serial.print("Channels: "); Serial.println(channels);
    
    mode = input["mode"] | mode;
    Serial.print("Mode: "); Serial.println(mode);
    
    setRotation = input["setRotation"] | setRotation;
    Serial.print("SetRotation: "); Serial.println(setRotation);
    
    startupMessage = input["startupMessage"] | startupMessage;
    Serial.print("Startup Message: "); Serial.println(startupMessage);
    
    isDenied = input["denied"] | isDenied;
    Serial.print("Denied: "); Serial.println(isDenied);

    url = input["url"] | url;
    Serial.print("Website URL: "); Serial.println(url);
    
    deniedReason = input["deniedReason"] | deniedReason;
    Serial.print("Denied Reason: "); Serial.println(deniedReason);

    //There are some denied reasons that we should expand on a bit to make more sense to the user;
    if(deniedReason == "EQUIPMENT_TRAINING"){
      deniedReason = "Missing equipment-specific training. Please visit " + url;
    }
    if(deniedReason == "ROOM_TRAINING" || deniedReason == "MAKERSPACE_TRAINING"){
      deniedReason = "Missing makerspace-wide training. Please visit " + url;
    }
    if(deniedReason == "WELCOME"){
      deniedReason = "Please sign in at the front desk.";
    }
    if(deniedReason == "UNPAIRED"){
      deniedReason = "This device is not paired to an equipment?";
    }
    if(deniedReason == "UNKNOWN_USER"){
      deniedReason = "   Card not recognized.    Check in with staff.";
    }
    if(deniedReason == "ACTIVE_HOLD" || deniedReason == "ACTIVE_RESTRICTION"){
      deniedReason = "There is a restriction on your account, please speak with staff.";
    }
    if(deniedReason == "ARCHIVED"){
      deniedReason = "Your account is archived (Are you an alumni?). Talk to staff for more information.";
    }
    if(deniedReason == "MISSING_SIGN_OFF"){
      deniedReason = "Missing equipment sign-off. Speak to staff to complete.";
    }
    
    motd = input["motd"] | motd;
    Serial.print("MOTD: "); Serial.println(motd);
    
    faultMessage = input["faultMessage"] | faultMessage;
    Serial.print("Fault Message: "); Serial.println(faultMessage);
    
    filedScene = input["filedScene"] | filedScene;
    Serial.print("Filed Scene: "); Serial.println(filedScene);

    identify = input["identify"] | identify;
    Serial.print("Identify: "); Serial.println(identify);

    if (input.containsKey("info")) {
      // 1. Prepare the response document
      JsonDocument infoDoc;
      infoDoc.to<JsonObject>(); 
      
      // 2. Safely get the array from the input
      JsonArray infoArray = input["info"].as<JsonArray>();

      if (infoArray.isNull()) {
        Serial.println("{\"error\": \"'info' key is not a valid array\"}");
        return; 
      }

      // 3. Iterate through array items, expecting standard camelCase requests
      for (JsonVariant item : infoArray) {
        String key = item.as<String>();
        key.trim(); 

        if (key == "serialNumber")            infoDoc["serialNumber"] = getSerialNumber();
        else if (key == "macAddress")         infoDoc["macAddress"] = getBaseMacAddress();
        else if (key == "hasWifi")            infoDoc["hasWifi"] = HAS_WIFI;
        else if (key == "hardware")           infoDoc["hardware"] = HARDWARE_TYPE;
        else if (key == "manufacturer")       infoDoc["manufacturer"] = HARDWARE_MFGR;
        else if (key == "displayType")        infoDoc["displayType"] = DISP_TYPE;
        else if (key == "displaySize")        infoDoc["displaySize"] = DISP_SIZE;
        else if (key == "displayResolution")  infoDoc["displayResolution"] = DISP_RESOLUTION;
        else if (key == "dimmable")           infoDoc["dimmable"] = HARDWARE_DIMMABLE;
        else if (key == "speakerPresent")     infoDoc["speakerPresent"] = hasSpeaker;
        else if (key == "sceneStorage")       infoDoc["sceneStorage"] = sceneStorage;
        else if (key == "sdCardType")         infoDoc["sdCardType"] = sdCardType;
        else if (key == "sdCardCapacity")     infoDoc["sdCardCapacity"] = sdCardCapacity;
        else if (key == "firmware")           infoDoc["firmware"] = FW_VER;
        
        // Array Responses
        else if (key == "listScenes") {
          JsonArray scenesArray = infoDoc.createNestedArray("listScenes");
          addDirectoryHashesToJson(scenesArray, "/scenes");
        }
        else if (key == "listAudio") {
          JsonArray audioArray = infoDoc.createNestedArray("listAudio");
          addDirectoryHashesToJson(audioArray, "/audio");
        }
      }

      // 4. Send the payload back
      String infoResponse;
      serializeJson(infoDoc, infoResponse);
      Serial.println(infoResponse);
    }

    if (input.containsKey("ota")){
      // Expected keys are wifi, password, url, and hash.
      // Ex: {"ota":{"wifi":"RIT-WiFi","password":"","url":"...","hash":"..."}}
      currentScene = 9; //Force Screen OTA Scene
      String otaObjectString;
      serializeJson(input["ota"], otaObjectString);
      performSecureOta(otaObjectString);
    }

    if(input.containsKey("coreOta")){
      if(currentScene != 9){
        currentScene = 10; //Force Core OTA Screen
        otaProgressPercent = input["coreOta"];
      }
    }

    if(input.containsKey("command")){
      //execute a command of some sort
      if(input["command"] == "restart"){
        startupMessage = "Executing Restart...";
        currentScene = 0;
        restartFlag = true;
      }
    }

    //Next let's make some decisions based on our current situation
    currentScene = decideScene();
  }
}

uint64_t millis64(){
  return esp_timer_get_time() / 1000;
}

byte decideScene(){
  if(currentScene == 9 || currentScene == 10){
    return currentScene;
  }
  if(identify){
    return 11;
  }
  if(startupMessage != ""){
    return 0;
  }
  if(isWelcoming){
    return 5;
  }
  if(acsState == "FAULT"){
    return 7;
  }
  if(channels == 0){
    startupMessage = "No channels defined!";
    return 0;
  }
  if(filedScene != ""){
    return 8;
  }
  if(channels == 1){
    if(isDenied){
      return 3;
    }
    if(acsState == "IDLE"){
      return 1;
    }
    if(acsState == "ALWAYS_ON" || acsState == "UNLOCKED"){
      return 2;
    }
    if(acsState == "LOCKED_OUT"){
      return 6;
    }
  } else {
    startupMessage = "Multi-channel screen not supported!";
    return 0;
  }
  return 0; // Fallback return
}

String getSerialNumber(){
  String serialNumber;
  uint8_t unique_id[16]; 
  esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_OPTIONAL_UNIQUE_ID, unique_id, 128);

  if (err == ESP_OK) {
    Serial.print("Serial Number: ");
    for (int i = 0; i < 16; i++) {
      if (unique_id[i] < 0x10) serialNumber += "0"; 
      serialNumber += String(unique_id[i], HEX);
    }
    serialNumber.toUpperCase();
  } else {
    serialNumber = getBaseMacAddress();
  }
  return serialNumber;
}

String getBaseMacAddress() {
  uint8_t baseMac[6];
  char macStr[18]; 

  if (esp_read_mac(baseMac, (esp_mac_type_t)ESP_MAC_WIFI_STA) == ESP_OK) {
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             baseMac[0], baseMac[1], baseMac[2], 
             baseMac[3], baseMac[4], baseMac[5]);
    return String(macStr);
  } else {
    return String("00:00:00:00:00:00");
  }
}

void addDirectoryHashesToJson(JsonArray arrayTarget, const char* directoryPath) {
  fs::File dir = SD_MMC.open(directoryPath);
  
  if (!dir || !dir.isDirectory()) {
    JsonObject errorObj = arrayTarget.createNestedObject();
    errorObj["error"] = "Directory not found";
    return;
  }

  fs::File file = dir.openNextFile();
  
  while (file) {
    if (!file.isDirectory()) {
      uint32_t checksum = 0; 
      uint8_t buffer[512]; 
      size_t bytesRead;
      while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0) {
        checksum = crc32_le(checksum, buffer, bytesRead);
      }

      const char* fullPath = file.name();
      const char* fileName = strrchr(fullPath, '/');
      if (fileName != nullptr) {
        fileName++; 
      } else {
        fileName = fullPath;
      }

      JsonObject fileObj = arrayTarget.createNestedObject();
      fileObj["title"] = fileName; // Standardized JSON key
      
      char hexHash[9]; 
      sprintf(hexHash, "%08X", checksum); 
      fileObj["hash"] = hexHash; // Standardized JSON key
    }
    
    file.close(); 
    file = dir.openNextFile(); 
  }
}

void performSecureOta(const String& jsonPayload) {
    otaProgressPercent = 0; 

    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, jsonPayload);

    if (error) {
        Serial.printf("JSON Parse failed: %s\n", error.c_str());
        currentScene = 0; 
        startupMessage = "Screen OTA Failed! (Bad JSON)";
        sendLog("Screen OTA Failed! (Bad JSON)", true);
        return;
    }

    // JSON keys standardized
    const char* ssid = doc["wifi"];
    const char* password = doc["password"];
    const char* url = doc["url"];
    const char* expectedHash = doc["hash"];

    if (!ssid || !url || !expectedHash) {
        Serial.println("Missing required JSON fields (wifi, url, hash)");
        currentScene = 0; 
        startupMessage = "Screen OTA Failed! (Bad JSON)";
        sendLog("Screen OTA Failed! (Bad JSON)", true);
        return;
    }

    Serial.printf("Connecting to WiFi: %s\n", ssid);
    WiFi.begin(ssid, password);
    
    int timeout = 20; 
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        Serial.print(".");
        timeout--;
    }
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection failed!");
        currentScene = 0; 
        startupMessage = "Screen OTA Failed! (No WiFi)";
        sendLog("Screen OTA Failed! (No WiFi)", true);
        return;
    }
    Serial.println("WiFi connected.");

    WiFiClientSecure client;
    client.setInsecure(); 
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
    
    http.begin(client, url);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET failed, code: %d\n", httpCode);
        http.end();
        currentScene = 0; 
        startupMessage = "Screen OTA Failed! (Bad HTTP)";
        sendLog("Screen OTA Failed! (Bad HTTP)", true);
        return;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("Invalid content length from server");
        http.end();
        currentScene = 0; 
        startupMessage = "Screen OTA Failed! (Bad File Size)";
        sendLog("Screen OTA Failed! (Bad File Size)", true);
        return;
    }

    if (!Update.begin(contentLength)) {
        Serial.println("Not enough space to begin OTA");
        Update.printError(Serial);
        http.end();
        currentScene = 0; 
        startupMessage = "Screen OTA Failed! (File too Large)";
        sendLog("Screen OTA Failed! (File too large)", true);
        return;
    }

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buff[1024];
    int remaining = contentLength;
    
    Serial.println("Starting download and flash...");
    
    while (http.connected() && remaining > 0) {
        size_t available = stream->available();
        if (available > 0) {
            int bytesToRead = (available > sizeof(buff)) ? sizeof(buff) : available;
            int bytesRead = stream->readBytes(buff, bytesToRead);
            
            if (Update.write(buff, bytesRead) != bytesRead) {
                Serial.println("Error writing to OTA partition");
                Update.printError(Serial);
                Update.abort(); 
                http.end();
                mbedtls_md_free(&ctx);
                currentScene = 0; 
                startupMessage = "Screen OTA Failed! (Write Error)";
                sendLog("Screen OTA Failed! (Write Error)", true);
                return;
            }
            
            mbedtls_md_update(&ctx, buff, bytesRead);
            remaining -= bytesRead;

            int downloaded = contentLength - remaining;
            otaProgressPercent = (downloaded * 100) / contentLength;
        }
        delay(1); 
    }

    uint8_t hashResult[32];
    mbedtls_md_finish(&ctx, hashResult);
    mbedtls_md_free(&ctx);

    char calcHashHex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(&calcHashHex[i * 2], "%02x", hashResult[i]);
    }

    Serial.printf("Calculated SHA-256: %s\n", calcHashHex);
    Serial.printf("Expected SHA-256:   %s\n", expectedHash);

    if (strcasecmp(calcHashHex, expectedHash) != 0) {
        Serial.println("SHA-256 mismatch! Aborting update to protect device.");
        Update.abort();
        http.end();
        currentScene = 0; 
        startupMessage = "Screen OTA Failed! (SHA Mismatch)";
        sendLog("Screen OTA Failed! (SHA Mismatch!)", true);
        return;
    }

    if (Update.end()) {
        otaProgressPercent = 100; 
        Serial.println("OTA Update Successful! Rebooting...");
        delay(1000); 
        ESP.restart(); 
    } else {
        Serial.println("Update.end() failed!");
        Update.printError(Serial);
        currentScene = 0; 
        startupMessage = "Screen OTA Failed! (End Failed)";
        sendLog("Screen OTA Failed! (End Failed)", true);
    }
    
    http.end();
}

void sendLog(String message, bool toHistory){
  JsonDocument messagePayload;
  messagePayload["message"] = message;
  messagePayload["auditLog"] = toHistory;
  String messageToSend;
  serializeJson(messagePayload, messageToSend);
  Serial0.println(messageToSend); 
}