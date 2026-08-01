/* 


Screen Controller
This task is responsible for talking with any connected screen.


*/

void sendCurrent(bool sendOneTime = false){
  //Sends the common regular information the screen needs
  JsonDocument CurrentStates;
  if(sendOneTime){
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
  }
  if(WelcomeMode){
    CurrentStates["welcoming"] = true;
  } else{
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
  for(int i = 0; i < ChannelCount; i++){
    stateArray.add(State[i]);
    deniedReasonArray.add(AuthReason[i]);
    unsigned long TapExpirationLeft = CurrentTapExpires[i] - millis64();
    if(TapExpirationLeft > 0){
      expirationArray.add(TapExpirationLeft);
    } else{
      expirationArray.add(0);
    }
    machineArray.add(HMIMachineName[i]);
    durationArray.add(TapDuration[i]*1000);
    hobbsArray.add(HobbsSeconds[i]);
  }
  CurrentStates["makerspace"] = HMIMakerspace;
  CurrentStates["deviceName"] = HMIDeviceName;
  CurrentStates["ACSRole"] = HMIRole;
  CurrentStates["mode"] = InputMode;
  CurrentStates["denied"] = AccessDenied;
  CurrentStates["faultMessage"] = FaultReason;
  CurrentStates["button"] = ResetLED; //ResetLED is a bool normally used for lighting animations, but it tracks with the button. 
  CurrentStates["startupMessage"] = ""; //Should be no startup message by the time we make it here.
  CurrentStates["identify"] = Identify;
  CurrentStates["url"] = Server;
  CurrentStates["setRotation"] = getTargetRotation();
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
  sendCurrent(true);
  delay(100);

  //Before we start regular operation, exit out of the startup screen;
  sendStartup("");

  while(1){

    delay(20);

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