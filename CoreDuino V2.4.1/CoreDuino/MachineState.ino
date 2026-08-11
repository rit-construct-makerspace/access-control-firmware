void MachineState(void *pvParameters){
  //Serial.println(F("MachineState Started."));
  while(1){
    delay(50);

    //This code is only meant to be used in welcome mode. 
    //If we are not in welcomemode, just handle other housekeeping stuff.
    if(WelcomeMode){
      //Input mode is always TEMP_PRESENT in welcome mode;
      InputMode = "TEMP_PRESENT";

      //Some random cleanup, none of these should be set if there isn't a card present
      if (!CardPresent) {
        WelcomingPending = false;
      }

      //See if we have a regular status update to send
      if (NextStatusTime <= millis64()) {
        //Time to send a status message.
        SendStatus = 1;
        NextStatusTime = millis64() + STATUS_INTERVAL;
      }

      //Step 1.2: Check for a change to the card. New one? Removed?
      FoundUID = NFCCardFound();
      if (FoundUID == "") {
        //Double check there really isn't a card present
        FoundUID = NFCCardFound();
      }

      if (!CardPresent && FoundUID.length() > 2) {
        //We found a card!
        CardPresent = true;
        UID = FoundUID;
        if (!NoNetwork) {
          //Welcome them
          SendWelcome = 1;
          WelcomingPending = 1;
        } else {
          //No network, can't auth. Let's just beep.
          FaultBeep = 1;
        }
      }

      //Was a card that was present removed?

      //In TEMP_PRESENT mode, we do this based on the card no longer being detected by UID.
      if (CardPresent && !FoundUID.equalsIgnoreCase(UID)) {
        //Either found no UID or UID we found is different
        CardPresent = false;
        UID = "";
        SendWelcome = 0;
        WelcomingPending = 0;
        UserWelcomed = 0;
      }
    }

    if(RestartWhenUnused || ScheduledRestart){
      //A welcome reader is always "Unused", so just restart.
      delay(5);
      RequestReset = 1;
      while(1){
        delay(100);
      }
    }

    //Step 1.5: Send Regular ping
    if(NextPingTime <= millis64() && !NoNetwork){
      //It is time to send a new ping
      NextPingTime = millis64() + 1000;
      if(NewPing){
        //We got a ping response as expected.
        NewPing = 0;
        ////Serial.println(F("Ping response OK."));
      } else{
        //Serial.println(F("Didn't get a ping response?"));
        NoNetwork = true;
      }
      SendPing = 1;
    }
  }
}

String NFCCardFound(){
  //Let's first ask the NFC reader for the card (if one is there)
  
  String ReturnedID = "";
  
  uint16_t atqa = mfrc630_iso14443a_REQA();

  if (atqa != 0) {  // Are there any cards that answered?
    uint8_t sak;
    uint8_t uid[10] = {0};  // uids are maximum of 10 bytes long.

    // Select the card and discover its uid.
    uint8_t uid_len = mfrc630_iso14443a_select(uid, &sak);
    if (uid_len != 0) {  // did we get a UID?
      for (uint8_t i=0; i<uid_len; i++){
      if (uid[i] < 16){
          ReturnedID += "0"; 
          ReturnedID += String(uid[i], HEX);
        } else {
          ReturnedID += String(uid[i], HEX);;
        }
      }
      ReturnedID.toLowerCase();
      ////Serial.print(F("Found UID :"));
      ////Serial.println(ReturnedID);
    } else {
      //Serial.print("Could not determine UID, perhaps some cards don't play");
      //Serial.print(" well with the other cards? Or too many collisions?\n");
      ReturnedID = "";
    }
  } else{
    //Did not find a UID
    ////Serial.println(F("Didn't find a card."));
    ReturnedID = "";
  }
  return ReturnedID;
}