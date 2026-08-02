/* 


Screen Controller
This task is responsible for talking with any connected screen.


*/

//Variables - Announcements
unsigned long long CheckAnnouncements = 600000;  //How often to check for announcements
JsonDocument AnnouncementsDoc;  // Global document to hold the latest announcements

void sendCurrent(bool sendRarely = false, bool sendFrequently = true){
  //Sends the common regular information the screen needs
  JsonDocument CurrentStates;
  if(sendRarely){
    //These are the things we don't need to send often, since they don't change much.
    //Send the current time
    CurrentStates["time"] = rtc.getEpoch();
    //Send WiFi credentials
    if(SSID != ""){
      //We have valid WiFi credentials to send;
      CurrentStates["WiFi-Available"] = true;
      CurrentStates["WiFi-SSID"] = SSID;
      CurrentStates["WiFi-Password"] = Password;
      CurrentStates["TLS-Cert"] = RootCert;
    } else{
      CurrentStates["WiFi-Available"] = false;
    }
    //Send the announcements:
    if (!AnnouncementsDoc["list"].isNull()) {
      CurrentStates["announcements"] = AnnouncementsDoc["list"];
    } else {
      // Optional: send an empty array if there are no announcements yet
      CurrentStates["announcements"].to<JsonArray>();
    }
    CurrentStates["makerspace"] = HMIMakerspace;
    CurrentStates["deviceName"] = HMIDeviceName;
    CurrentStates["ACSRole"] = HMIRole;
    CurrentStates["mode"] = InputMode;
    CurrentStates["url"] = Server;
    //Send a "station name" to refer to a group of equipment by on multi-channel systems;
    if(ChannelCount == 1){
      CurrentStates["stationName"] = HMIMachineName[1];
    } else if (ChannelCount == 0){
      CurrentStates["stationName"] = "";
    } else{
      CurrentStates["stationName"] = StationName;
    }
    //Send the makerspace's hours;
    if (!HoursDoc["list"].isNull()) {
      CurrentStates["hours"] = HoursDoc["list"];
    } else {
      // Send an empty array if there are no hours yet
      CurrentStates["hours"].to<JsonArray>();
    }
  }
  if(sendFrequently){
    if (WelcomeMode) {
      CurrentStates["welcoming"] = true;
    } else {
      CurrentStates["welcoming"] = false;
    }
    CurrentStates["noNetwork"] = NoNetwork;
    //Channel-related things;
    CurrentStates["channels"] = ChannelCount;
    JsonArray stateArray = CurrentStates["state"].to<JsonArray>();
    JsonArray deniedReasonArray = CurrentStates["deniedReason"].to<JsonArray>();
    JsonArray expirationArray = CurrentStates["currentAuthExpires"].to<JsonArray>();
    JsonArray machineArray = CurrentStates["deviceNames"].to<JsonArray>();
    JsonArray durationArray = CurrentStates["durations"].to<JsonArray>();
    JsonArray hobbsArray = CurrentStates["hobbsSeconds"].to<JsonArray>();
    for (int i = 0; i < ChannelCount; i++) {
      stateArray.add(State[i]);
      deniedReasonArray.add(AuthReason[i]);
      unsigned long TapExpirationLeft = CurrentTapExpires[i] - millis64();
      if (TapExpirationLeft > 0) {
        expirationArray.add(TapExpirationLeft);
      } else {
        expirationArray.add(0);
      }
      machineArray.add(HMIMachineName[i]);
      durationArray.add(TapDuration[i] * 1000);
      hobbsArray.add(HobbsSeconds[i]);
    }
    CurrentStates["denied"] = AccessDenied;
    CurrentStates["faultMessage"] = FaultReason;
    CurrentStates["button"] = ResetLED;    //ResetLED is a bool normally used for lighting animations, but it tracks with the button.
    CurrentStates["startupMessage"] = "";  //Should be no startup message by the time we make it here.
    CurrentStates["identify"] = Identify;
    CurrentStates["setRotation"] = getTargetRotation();
  }

  CurrentStates["crc"] = 0; //Temporary until we calculate CRC.
  String CurrentToSend;
  serializeJson(CurrentStates, CurrentToSend);
  //Calculate the CRC with the "crc" field set to 0;
  uint32_t checksum = esp_crc32_le(0, (const uint8_t*)CurrentToSend.c_str(), CurrentToSend.length());
  CurrentStates["crc"] = checksum;
  serializeJson(CurrentStates, CurrentToSend);
  Serial0.println(CurrentToSend);
  Serial0.flush();
}

void SceenController(void *pvParameters){
  
  unsigned long long NextScreenUpdate;


  //One time only; need to ask for all the info about the screen
  //TODO

  //Send our current information
  fetchAnnouncements();
  fetchHours();
  sendCurrent(true);
  delay(100);

  //Before we start regular operation, exit out of the startup screen;
  sendStartup("");

  while(1){

    delay(20);

    //Every 10 minutes, fetch the latest announcements and hours;
    if (CheckAnnouncements <= millis64()) {
      if(fetchAnnouncements() && fetchHours()){
        sendCurrent(true);
        NextScreenUpdate = millis64() + 1000;
        CheckAnnouncements = millis64() + 600000;
      }
    }

    //Check if it is time for a regular update of the screen
    if(NextScreenUpdate <= millis64()){
      UpdateScreen = true;
    }

    if(UpdateScreen){
      NextScreenUpdate = millis64() + 1000;
      sendCurrent();
      UpdateScreen = false;
    }
  }
}

int getTargetRotation() {
  // Read raw values from the LIS2DH12
  int16_t y = accel.getRawY();
  int16_t z = accel.getRawZ();

  // Settings for 12-bit mode (+/- 2g)
  const int minThreshold = 2500; 
  const int hysteresis = 1000; 

  // 'static' stays in memory between function calls
  static int lastValidOrientation = 5; 
  int currentCalculation = 5; 

  // 1. Determine the PHYSICAL state based on current sensor data
  if (abs(z) > (abs(y) + hysteresis) && abs(z) > minThreshold) {
    if (z < -minThreshold) {
      currentCalculation = 3; 
    } else if (z > minThreshold) {
      currentCalculation = 1;
    }
  } 
  else if (abs(y) > (abs(z) + hysteresis) && abs(y) > minThreshold) {
    if (y > minThreshold) {
      currentCalculation = 0; 
    } else if (y < -minThreshold) {
      currentCalculation = 2; 
    }
  }

  // 2. Decide what to return
  // If we are in a "messy" middle state (currentCalculation is 5), 
  // or if the state is exactly the same as before, return 5.
  if (currentCalculation == 5 || currentCalculation == lastValidOrientation) {
    return 5; 
  }

  // 3. If we reached here, it means we have a brand new valid orientation
  lastValidOrientation = currentCalculation;
  
  // Print only when the change actually happens
  Serial.print(F("Screen rotation changed to: "));
  Serial.println(lastValidOrientation);
  
  return lastValidOrientation;
}

bool fetchAnnouncements() {
  bool AnnouncementsUpdated = false;
  if (!NoNetwork) {
    networkclient.setCACert(RootCert.c_str());  
    Serial.println("Connecting to server...");

    if (networkclient.connect(Server.c_str(), 443)) {  
      Serial.println("Connected!");

      String payload = R"({"operationName": "GetAnnouncements", "variables": {}, "query": "query GetAnnouncements { getAllAnnouncements { title description linkText linkUrl } }"})";  

      // --- 1. SEND THE HTTP POST REQUEST MANUALLY ---
      networkclient.println("POST /graphql HTTP/1.1");          
      networkclient.print("Host: ");                            
      networkclient.println(Server);                            
      networkclient.println("Content-Type: application/json");  
      networkclient.print("Content-Length: ");                  
      networkclient.println(payload.length());                  
      networkclient.println("Connection: close");               
      networkclient.println();                                  
      networkclient.print(payload);                             

      // --- 2. READ THE HTTP RESPONSE ---
      while (networkclient.connected() && !networkclient.available()) {  
        delay(10);                                                       
      }

      while (networkclient.connected()) {                   
        String line = networkclient.readStringUntil('\n');  
        if (line == "\r") {                                 
          break;                                            
        }
      }

      // --- 3. PARSE THE JSON BODY ---
      String responseBody = networkclient.readString();  

      // Create a temporary document for parsing the raw response
      JsonDocument tempDoc;
      DeserializationError error = deserializeJson(tempDoc, responseBody);

      if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        AnnouncementsUpdated = false;
      } else {
        // Clear the global doc before adding new data
        AnnouncementsDoc.clear();

        // Copy the announcements array from the temporary doc to the global doc
        JsonArray announcements = tempDoc["data"]["getAllAnnouncements"];

        // Add it to our global document. We wrap it in an object key for clarity,
        // but you could also just make AnnouncementsDoc directly an array.
        AnnouncementsDoc["list"] = announcements;

        Serial.println("Announcements updated successfully.");
        AnnouncementsUpdated = true;
      }

      networkclient.stop();  

    } else {
      Serial.println("Connection to server failed.");
      AnnouncementsUpdated = false;
    }
  }
  return AnnouncementsUpdated;
}

bool fetchHours() {
  bool HoursUpdated = false;
  if (!NoNetwork) {
    networkclient.setCACert(RootCert.c_str());
    Serial.println("Connecting to server for hours...");

    if (networkclient.connect(Server.c_str(), 443)) {
      Serial.println("Connected!");

      // Construct the URL path using your variable
      String path = "/api/hours/" + String(MakerspaceNumber);

      // --- 1. SEND THE HTTP GET REQUEST MANUALLY ---
      networkclient.print("GET ");
      networkclient.print(path);
      networkclient.println(" HTTP/1.1");

      networkclient.print("Host: ");
      networkclient.println(Server);

      // Tell the server to close the connection after responding
      networkclient.println("Connection: close");

      // Send a blank line (\r\n) to indicate the end of the HTTP headers
      networkclient.println();

      // --- 2. READ THE HTTP RESPONSE ---
      // Wait for the server to reply
      while (networkclient.connected() && !networkclient.available()) {
        delay(10);
      }

      // Read headers line by line until we find the empty line
      while (networkclient.connected()) {
        String line = networkclient.readStringUntil('\n');
        if (line == "\r") {
          break;  // Empty line found, headers are done
        }
      }

      // --- 3. PARSE THE JSON BODY ---
      String responseBody = networkclient.readString();

      // Create a temporary document for parsing the raw response
      JsonDocument tempDoc;
      DeserializationError error = deserializeJson(tempDoc, responseBody);

      if (error) {
        Serial.print("Hours JSON parsing failed: ");
        Serial.println(error.c_str());
        HoursUpdated = false;
      } else {
        // Clear the global doc before adding new data
        HoursDoc.clear();

        // Copy the 'obj' array from the response to our global document
        JsonArray hoursData = tempDoc["obj"];
        HoursDoc["list"] = hoursData;

        Serial.println("Hours updated successfully.");
        HoursUpdated = true;
      }

      // Clean up the connection
      networkclient.stop();

    } else {
      Serial.println("Connection to server failed for hours.");
      HoursUpdated = false;
    }
    return HoursUpdated;
  }
}