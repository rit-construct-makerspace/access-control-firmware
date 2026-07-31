void MachineState(void *pvParameters){
  Serial.println(F("MachineState Started."));
  while(1){
    delay(50);

    //Step 1.1: Check for any reason we should be in a fault state
    //Temporarily disabled due to too many false positives
    /*
    if(OverTemp || SealBroken){
      if(State != "FAULT"){
        //This is our first time going to the fault state
        State = "FAULT";
        StateChangeReason = "FAULT";
        Message = "ACS Fault!";
        FaultReason = "ACS Fault!";
        if(OverTemp){
          StateChangeReason = "OVER_TEMP";
          Message = "Overtemperature!";
          FaultReason = "Overtemperature!";
        }
        if(SealBroken){
          StateChangeReason = "INTEGRITY_FAIL";
          Message = "Bus Integrity Broken!";
          FaultReason = "Bus Integrity!";
        }
        MessageToSend = 1;
        UpdateScreen = true;
      }
    }
    */

    //Interrupt Manager:
    if(!IsInterrupted){
      //Check if the interrupt is low;
      if(!digitalRead(INTERRUPT)){
        InterruptCount++;
      } else{
        //Not interrupted and not reading interrupt, set the counter back to 0.
        InterruptCount = 0;
      }
      if(InterruptCount >= 5){
        //We had multiple interrupts in a row, so we must be in an interrupt state!
        Serial.println(F("Interrupt Triggered!"));
        IsInterrupted = true;
        //Actually execute on the interrupt state;
        if(InterruptResponse == "MESSAGE"){
          //Send a message
          Message = "Interrupt Triggered!";
          MessageToSend = true;
        }
        if(InterruptResponse == "LOCK_TEMP"){
          //Temporarily lock the channels;
          for(int i = 0; i < ChannelCount; i++){
            if(State[i] == "UNLOCKED" || State[i] == "ALWAYS_ON" || State[i] == "IDLE"){
              State[i] = "LOCKED_OUT";
              StateChangeReason[i] = "LOCK_TEMP";
              SingleBeep = true;
            }
          }
        }
        if(InterruptResponse == "IDLE"){
          //Idle any unlocked or Always-On channel:
          for(int i = 0; i < ChannelCount; i++){
            if(State[i] == "UNLOCKED" || State[i] == "ALWAYS_ON"){
              State[i] = "IDLE";
              StateChangeReason[i] = "LOCAL";
              SingleBeep = true;
            }
          }
        }
        if(InterruptResponse == "FAULT"){
          //Fault all channels.
          for(int i = 0; i < ChannelCount; i++){
            State[i] = "FAULT";
            StateChangeReason[i] = "FAULT";
          }
          FaultReason = "Interrupt Asserted!";
        }
        UpdateScreen = true; //tell the frontend ASAP
      }
    } else{
      //Check if we are out of the interrupt state;
      if(digitalRead(INTERRUPT)){
        InterruptCount--;
        if(InterruptCount <= 2){
          //We had multiple interrupts missed in a row, so we must no longer be in an interrupt state.
          IsInterrupted = false;
          Serial.println(F("De-asserted Interrupt."));
          InterruptCount = 0;
          if(InterruptResponse == "LOCK_TEMP"){
            //Now that the interrupt is clear, release the locks on channels
            for(int i = 0; i < ChannelCount; i++){
              if(State[i] == "LOCKED_OUT"){
                State[i] = "IDLE";
                StateChangeReason[i] = "LOCAL";
                SingleBeep = true;
              }
            }
          }
        }
      }
    }

    //IF we are in welcoming mode, the input mode is always TEMP_PRESENT and there are no access channels.
    if(WelcomeMode){
      InputMode = "TEMP_PRESENT";
      ChannelCount = 0;
    } else{
      InputMode = DefaultInputMode;
      //Re-load the channel count we have stored in memory;
      ChannelCount = settings.getString("ChannelCount").toInt();
    }

    //Some random cleanup, none of these should be set if there isn't a card present
    if(!CardPresent){
      WelcomingPending = false;
      AccessDenied = false;
      PendingApproval = false;
      PendingApproval = false;
    }

    //See if we have a regular status update to send
    if(NextStatusTime <= millis64()){
      //Time to send a status message.
      SendStatus = 1;
      NextStatusTime = millis64() + STATUS_INTERVAL;
    }

    //Step 1.2: Check for a change to the card. New one? Removed? 
    FoundUID = NFCCardFound();
    if(FoundUID == ""){
      //Double check there really isn't a card present
      FoundUID = NFCCardFound();
    }
    
    //Then, what state are we in? Tap? Insert? 
      //If we are in INSERT mode, we look at the switches before determining if we are looking for a card.
      //If we are in TEMP_PRESENT mode, we look for a card no matter what.

    if(InputMode == "TEMP_PRESENT"){
      if(!CardPresent && FoundUID.length() > 2){
        //Accept the current card as the actual card.
        CardPresent = true;
        UID = FoundUID;
        if(WelcomeMode && !NoNetwork){
          //Let's welcome the user to the makerspace
          SendWelcome = 1;
          WelcomingPending = 1;
        }
        if(anyChannelIs("IDLE") && !NoNetwork){
          //Let's check for auth with the server
          PendingApproval = true;
          SendAuth = true;
        } else if(!anyChannelIs("IDLE") && (anyChannelIs("UNLOCKED") || anyChannelIs("ALWAYS_ON"))){
          //Logic: If there are no channels in IDLE that the user could unlock, but something is already unlocked or always on, beep to confirm.
          SingleBeep = true;
        } else if(anyChannelIs("IDLE") && NoNetwork){
          //Fault beep and deny the user due to no network
          FaultBeep = true;
          AccessDenied = true;
          for(int i = 0; i < ChannelCount; i++){
            AuthReason[i] = "No network, try again soon or talk to staff.";
          }
          Serial.println(F("Access denied due to no network!"));
        } else{
          //Auto-deny the user, likely all locked or in a fault state?
          AccessDenied = true;
          for(int i = 0; i < ChannelCount; i++){
            AuthReason[i] = "Incorrect state, machine must be in \"IDLE\" mode to activate.";
          }
          Serial.println(F("Auto-denied due to bad state."));
        }
        if(NoNetwork){
          //Give a fault beep, reject them immediately
          FaultBeep = 1;
        }
      }
    } else{ //INSERT
      if(!CardPresent && !digitalRead(DET1) && !digitalRead(DET2)){
        //New card inserted!
        CardPresent = true;
        UID = FoundUID;
        if(anyChannelIs("IDLE") && !NoNetwork){
          //Let's check for auth with the server
          PendingApproval = true;
          SendAuth = true;
        } else if(!anyChannelIs("IDLE") && (anyChannelIs("UNLOCKED") || anyChannelIs("ALWAYS_ON"))){
          //Logic: If there are no channels in IDLE that the user could unlock, but something is already unlocked or always on, beep to confirm.
          SingleBeep = true;
        } else if(anyChannelIs("IDLE") && NoNetwork){
          //Fault beep and deny the user due to no network
          FaultBeep = true;
          AccessDenied = true;
          for(int i = 0; i < ChannelCount; i++){
            AuthReason[i] = "No network, try again soon or talk to staff.";
          }
          Serial.println(F("Access denied due to no network!"));
        } else{
          //Auto-deny the user, likely all locked or in a fault state?
          AccessDenied = true;
          for(int i = 0; i < ChannelCount; i++){
            AuthReason[i] = "Incorrect state, machine must be in \"IDLE\" mode to activate.";
          }
          Serial.println(F("Auto-denied due to bad state."));
        }
      }
    }
  
    //Was a card that was present removed?

    //In TEMP_PRESENT mode, we do this based on the card no longer being detected by UID.
    if(InputMode == "TEMP_PRESENT"){
      if(CardPresent && !FoundUID.equalsIgnoreCase(UID)){
        //Either found no UID or UID we found is different
        Serial.print(F("Card "));
        Serial.print(UID);
        Serial.print(F(" replaced with "));
        Serial.println(FoundUID);
        CardPresent = false;
        UID = "";
        SendWelcome = 0;
        WelcomingPending = 0;
        UserWelcomed = 0;
        AccessDenied = 0;
      }
    } else{ //INSERT
      //In INSERT mode, we detect this based on switches
      if(CardPresent && (digitalRead(DET1) || digitalRead(DET2))){
        //Reset everything to normal.
        CardPresent = false;
        UID = "";
        PendingApproval = false;
        AccessDenied = false;
        for(int i = 0; i < ChannelCount; i++){
          if(State[i] == "UNLOCKED"){
            State[i] = "IDLE";
            StateChangeReason[i] = "CARD_REMOVED";
          }
        }
      }
    }

    //Handle access expiration for channels if in TEMP_PRESENT mode:
    if(InputMode == "TEMP_PRESENT"){
      for(int i = 0; i < ChannelCount; i++){
        if(CurrentTapExpires[i] <= millis64()){
          if(State[i] == "UNLOCKED"){
            State[i] = "IDLE";
            StateChangeReason[i] = "CARD_REMOVED";
            SingleBeep = true;
          }
          CurrentTapExpires[i] = 0; //Cleanup
        }
      }
    } else{
      //If we are not in TEMP_PRESENT, then all CurrentTapExpires should be 0.
      for(int i = 0; i < ChannelCount; i++){
        CurrentTapExpires[i] = 0;
      }
    }

    //Step 1.3: Check if the states have changed since last time we went through the loop.
    //Check that ChannelAccess values are right, and set them properly.
    bool AccessOn = false;
    bool TellUpdateScreen = false;
    for(int i = 0; i < ChannelCount; i++){
      if(State[i] == "UNLOCKED" || State[i] == "ALWAYS_ON"){
        if(ChannelAccess[i] != 1){
          TellUpdateScreen = true;
        }
        ChannelAccess[i] = 1;
        digitalWrite(ACCESS, HIGH);
        AccessOn = true;
      } else{
        if(ChannelAccess[i] != 0){
          TellUpdateScreen = true;
        }
        ChannelAccess[i] = 0;
      }
      //While we are here, check that there is a valid state change reason for everyone. 
      if(StateChangeReason[i] == ""){
        StateChangeReason[i] = "UNKNOWN";
      }
    }
    if(AccessOn == false){
      //No channels are on, disable Access
      digitalWrite(ACCESS, LOW);
    }
    if(TellUpdateScreen){
      //A ChannelAccess state changed, update the screen
      UpdateScreen = true;
    }
    //Set the GPIO of the bus based on ChannelAccess
    digitalWrite(GPIO1, ChannelAccess[0]);
    digitalWrite(GPIO2, ChannelAccess[1]);
    digitalWrite(GPIO3, ChannelAccess[2]);
    digitalWrite(GPIO4, ChannelAccess[3]);

    //See if any of the channels changed state to report to the server;
    bool SendStateChange = false;
    for(int i = 0; i < ChannelCount; i++){
      if(State[i] != LastState[i]){
        if(LastState[i] != "UNKNOWN"){
          //The state changed for something other than setting back from unkown, we should send it.
          SendStateChange = true;
        }
        LastState[i] = State[i]; //Override LastState with State
      }
    }
    if(SendStateChange){
      StateChange = true;
    }

    //Step 1.4: Check for and execute any flags;
    if(LockWhenIdle && !anyChannelIs("UNLOCKED") && !anyChannelIs("ALWAYS_ON")){
      //If no channel is unlocked or always on, lock any idle channels.
      for(int i = 0; i < ChannelCount; i++){
        if(State[i] == "IDLE"){
          State[i] = "LOCKED_OUT";
          StateChangeReason[i] = "COMMANDED";
        }
      }
      LockWhenIdle = 0;
    }
    if(RestartWhenUnused && !anyChannelIs("UNLOCKED") && !anyChannelIs("ALWAYS_ON")){
      Serial.println(F("Executing restart-when-unused flag."));
      Serial.flush();
      delay(5);
      RequestReset = 1;
      while(1){
        delay(100);
      }
    }
    if(ScheduledRestart){
      //It is time for a scheduled restart
      if(ScheduledRestartTime <= millis64() && ScheduledRestartTime != 0){
        //Time to force a restart, the user has had 60 seconds to stop.
        for(int i = 0; i < ChannelCount; i++){
          State[i] = "UNKNOWN";
        }
        Serial.println(F("User has had 60 seconds to end session, forcing scheduled restart."));
      }
      if(anyChannelIs("ALWAYS_ON") || anyChannelIs("UNLOCKED")){
        //We should not restart now, someone is using the machine? 
        //Let the user know we are restarting soon.
        ImminentShutdown = true;
        if(ScheduledRestartTime == 0){
          Serial.println(F("Server commanded scheduled shutdown, but user present? Giving them 60 seconds."));
          ScheduledRestartTime = millis64() + 60000; //Give them 60 seconds
        }
      } else{
        //Time to execute a restart.
        Serial.println(F("Executing scheduled restart."));
        Serial.flush();
        RequestReset = 1;
        while(1){
          delay(100);
        }
      }
    }

    //Step 1.5: Send Regular ping
    if(NextPingTime <= millis64() && !NoNetwork){
      //It is time to send a new ping
      NextPingTime = millis64() + 1000;
      if(NewPing){
        //We got a ping response as expected.
        NewPing = 0;
        //Serial.println(F("Ping response OK."));
      } else{
        Serial.println(F("Didn't get a ping response?"));
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
      //Serial.print(F("Found UID :"));
      //Serial.println(ReturnedID);
    } else {
      Serial.print("Could not determine UID, perhaps some cards don't play");
      Serial.print(" well with the other cards? Or too many collisions?\n");
      ReturnedID = "";
    }
  } else{
    //Did not find a UID
    //Serial.println(F("Didn't find a card."));
    ReturnedID = "";
  }
  return ReturnedID;
}

bool anyChannelIs(String targetState) {
  //Checks if any channel is in this state
  for (int i = 0; i < ChannelCount; i++) {
    if (State[i] == targetState) {
      return true; // Found a match, exit early
    }
  }
  return false; // No matches found
}