void MachineState(void *pvParameters){
  while(1){
    delay(50);
    //Step 1: Check machine state variables

    bool IntAsserted;

    if(!digitalRead(DB9INT)){
      //Are you sure?
      delay(50);
      IntAsserted = digitalRead(DB9INT);
    } else{
      IntAsserted = 1;
    }

    //Step 1.1: Check for any reason we should be in a fault state
    if(OverTemp || SealBroken || NFCBroken || !IntAsserted){
      if(State != "FAULT"){
        //This is our first time going to the fault state
        if(!IntAsserted){
          StateChangeReason = "FAULT";
          Message = "Component Asserted Interrupt!";
          MessageToSend = 1;
        }
        if(OverTemp){
          State = "FAULT";
          StateChangeReason = "OVER_TEMP";
          Message = "Overtemperature!";
          MessageToSend = 1;
        }
        if(SealBroken && BUS_CHECK){
          //We only do this if the bus is broken.
          State = "FAULT";
          StateChangeReason = "INTEGRITY_FAIL";
          Message = "Bus Integrity Broken!";
          MessageToSend = 1;
        }
      }
    }

    //Step 1.2: Check for a change to the card. New one? Removed? 
    //Was a new card inserted?
    if(!CardPresent && Switch1 && Switch2){
      CardPresent = 1;
      if(!NoNetwork){
        //Let's read the card;
        byte NFCRetryCount = 0;
        bool success;                                //Determines if an NFC read was successful
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
        uint8_t uidLength = 0;                           // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
        while(NFCRetryCount <= 5){
          //Power cycle the NFC reader
          digitalWrite(NFCPWR, HIGH);
          vTaskDelay(10 / portTICK_PERIOD_MS);
          digitalWrite(NFCRST, HIGH);
          vTaskDelay(10 / portTICK_PERIOD_MS);
          nfc.wakeup();
          nfc.setPassiveActivationRetries(0xFF);
          delay(10); //Let the bus stabilize
          //Attempt to read the card via NFC:
          success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength, 500);
          if(success){
            break;
          } else{
            NFCRetryCount++;
          }
        }
        if(NFCRetryCount > 5){
          //We failed to read the NFC card all 5 times.
          //Is there actually an NFC card here?
          uint32_t versiondata = nfc.getFirmwareVersion();
          if(!versiondata){
            //This means the NFC reader is broken
            NFCBroken = true;
            Serial.println(F("NFC Reader Broken?"));
          } else{
            //Reader is working, must not be an NFC card?
            AccessDenied = true;
            Serial.println(F("Not an NFC card?"));
          }
        }
        digitalWrite(NFCRST, LOW);
        digitalWrite(NFCPWR, LOW);
        UID = "";
        if(uidLength > 0){
          Serial.print(F("Card: "));
          for (uint8_t i = 0; i < uidLength; i++) {
            Serial.print(uid[i], HEX);
            Serial.print(" ");
            char buf[3] = {0};
            snprintf(buf, sizeof(buf), "%02x", uid[i]);
            UID += String(buf);
          }
          Serial.println(" Found.");
        }
        delay(10);
        if(State == "IDLE" && !NoNetwork){
          //Let's check with the server
          PendingApproval = true;
          SendAuth = true;
        } else if(State == "UNLOCKED" || State == "ALWAYS_ON"){
          //Nothing changes, but let's give a beep to the user.
          SingleBeep = true;
        } else{
          //Auto-deny the user
          AccessDenied = true;
          Serial.println(F("Auto-denied due to bad state."));
        }
      } else{
        //We can't auth without network!
        AccessDenied = true;
        Serial.println(F("Access denied due to no network!"));
      }

    }
    //Was a card that was present removed?
    if(CardPresent && (!Switch1 || !Switch2)){
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

    //See if we have a regular status update to send
    if(NextStatusTime <= millis64()){
      //Time to send a status message.
      SendStatus = 1;
      NextStatusTime = millis64() + STATUS_INTERVAL;
    }

    //Step 1.3: Check if the state has changed since last time we went through the loop.
    if(State != LastState){
      Serial.print(F("State changed from "));
      Serial.print(LastState);
      Serial.print(F(" to "));
      Serial.println(State);
      PreservedLastState = LastState;
      LastState = State;
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
    if(RestartWhenUnused && State != "UNLOCKED" && State != "ALWAYS_ON"){
      //We are in a non-used state, so restart
      RequestReset = 1;
      while(1){
        delay(100);
      }
    }
    if(ScheduledRestart){
      //It is time for a scheduled restart
      if(ScheduledRestartTime <= millis64() && ScheduledRestartTime != 0){
        //Time to force a restart, the user has had 60 seconds to stop.
        State = "UNKNOWN";
        Serial.println(F("User has had 60 seconds to end session, forcing scheduled restart."));
      }
      if(State == "ALWAYS_ON" || State == "UNLOCKED"){
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
    if(NextPingTime <= millis64()){
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