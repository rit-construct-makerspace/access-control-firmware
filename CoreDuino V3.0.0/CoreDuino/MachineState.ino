void MachineState(void *pvParameters){
  while(1){
    delay(50);
    //Step 1: Check machine state variables

    //Step 1.1: Check for any reason we should be in a fault state
    if(OverTemp || SealBroken || NFCBroken){
      State = "FAULT";
      StateChangeReason = "FAULT";
      if(OverTemp){
        StateChangeReason = "OVER_TEMP";
      }
      if(SealBroken){
        StateChangeReason = "INTEGRITY_FAIL";
      }
    }

    //Step 1.2: Check for a change to the card. New one? Removed? 

    //Let's first ask the NFC reader for the card (if one is there)
    uint16_t atqa = mfrc630_iso14443a_REQA();

    if (atqa != 0) {  // Are there any cards that answered?
      uint8_t sak;
      uint8_t uid[10] = {0};  // uids are maximum of 10 bytes long.

      // Select the card and discover its uid.
      uint8_t uid_len = mfrc630_iso14443a_select(uid, &sak);

      if (uid_len != 0) {  // did we get a UID?
        for (uint8_t i=0; i<uid_len; i++){
        if (uid[i] < 16){
            FoundUID += "0"; 
            FoundUID += String(uid[i], HEX);
          } else {
            FoundUID += String(uid[i], HEX);;
          }
        }
        FoundUID.toUpperCase();
      } else {
        USBSerial.print("Could not determine UID, perhaps some cards don't play");
        USBSerial.print(" well with the other cards? Or too many collisions?\n");
      }
      //Print the UID if it is a new one
      if(!CardPresent){
        USBSerial.print(F("Found UID: "));
        USBSerial.println(FoundUID);
      }
    }
    
    //Then, what state are we in? Tap? Insert? 
      //If we are in INSERT mode, we look at the switches before determining if we are looking for a card.
      //If we are in TEMP_PRESENT mode, we look for a card no matter what.

    if(InputMode == "TEMP_PRESENT"){
      if(!CardPresent){
        //Accept the current card as the actual card.
        CardPresent = 1;
        UID = FoundUID;
        if(State == "WELCOMING" && !NoNetwork){
          //Let's welcome the user to the makerspace
          SendWelcome = 1;
        }
        if(NoNetwork){
          //Give a fault beep
          FaultBeep = 1;
        }
      }
    } else{ //INSERT
      if(!CardPresent && !digitalRead(DET1) && !digitalRead(DET2)){
        //New card inserted!
        CardPresent = 1;
        UID = FoundUID;
        if(State == "IDLE" && !NoNetwork){
          //Let's check for auth with the server
          PendingApproval = true;
          SendAuth = true;
        } else if(State == "UNLOCKED" || State == "ALWAYS_ON"){
          //Nothing changes, but let's give a beep to the user.
          SingleBeep = true;
        } else if(State == "IDLE" && NoNetwork){
          //Fault beep and deny the user due to no network
          FaultBeep = true;
          AccessDenied = true;
          USBSerial.println(F("Access denied due to no network!"));
        } else{
          //Auto-deny the user
          AccessDenied = true;
          Serial.println(F("Auto-denied due to bad state."));
        }
      }
    }
  
    //Was a card that was present removed?

    //In TEMP_PRESENT mode, we do this based on the card no longer being detected by UID.
    if(InputMode == "TEMP_PRESENT"){
      if(FoundUID != UID){
        //Either found no UID or UID we found is different
        CardPresent = 0;
        UID = "";
        SendWelcome = 0;
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
        if(State == "UNLOCKED"){
          State = "IDLE";
          StateChangeReason = "CARD_REMOVED";
        }
      }
    }

    //Step 1.3: Check if the state has changed since last time we went through the loop.
    if(State != LastState){
      USBSerial.print(F("State changed from "));
      USBSerial.print(LastState);
      USBSerial.print(F(" to "));
      USBSerial.println(State);
      PreservedLastState = LastState;
      LastState = State;
      if(State == "WELCOMING"){
        //In welcoming, we detect card by tap
        InputMode = "TEMP_PRESENT";
      } else{
        InputMode = "INSERT";
      }
      if(State == "UNLOCKED" || State == "ALWAYS_ON"){
        Access = 1;
      } else{
        Access = 0;
      }
      StateChange = 1;
      if(StateChangeReason == ""){
        StateChangeReason = "UNKNOWN";
      }
    }

    //Step 1.4: Check for and execute any flags;
    if(LockWhenIdle && State == "IDLE"){
      State = "LOCKED_OUT";
      StateChangeReason = "COMMANDED";
      LockWhenIdle = 0;
    }
    if(RestartWhenIdle && State == "IDLE"){
      RequestReset = 1;
      while(1);
    }

    //Step 1.5: Send Regular ping
    if(NextPingTime <= millis64()){
      //It is time to send a new ping
      NextPingTime = millis64() + 1000;
      if(NewPing){
        //We got a ping response as expected.
        NewPing = 0;
        USBSerial.println(F("Ping response OK."));
      } else{
        USBSerial.println(F("Didn't get a ping response?"));
        NoNetwork = true;
      }
      SendPing = 1;
    }
  }
}