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
    //Was a new card inserted?
    if(!CardPresent && Switch1 && Switch2){
      CardPresent = 1;
      if(!NoNetwork){
        //Let's read the card;
        byte NFCRetryCount = 0;
        bool success;                                //Determines if an NFC read was successful
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
        uint8_t uidLength;                           // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
        while(NFCRetryCount <= 5){
          //Power cycle the NFC reader
          digitalWrite(NFCPWR, HIGH);
          vTaskDelay(10 / portTICK_PERIOD_MS);
          digitalWrite(NFCRST, HIGH);
          vTaskDelay(10 / portTICK_PERIOD_MS);
          nfc.wakeup();
          nfc.setPassiveActivationRetries(0xFF);
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
        Serial.print(F("Card: "));
        for (uint8_t i = 0; i < uidLength; i++) {
          Serial.print(uid[i], HEX);
          Serial.print(" ");
          char buf[3] = {0};
          snprintf(buf, sizeof(buf), "%02x", uid[i]);
          UID += String(buf);
        }
        Serial.println(" Found.");
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
        //Serial.println(F("Ping response OK."));
      } else{
        Serial.println(F("Didn't get a ping response?"));
        NoNetwork = true;
      }
      SendPing = 1;
    }
  }
}