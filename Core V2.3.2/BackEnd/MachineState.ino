/*

Machine State

This task is responsible for the core logic of the system, tieing everything else together.


Global Variables Used:
LogoffSent: Flag to indicate that the status report "Session End" has been sent, so data can be cleared and reset.
State: Plaintext indication of the state of the system for status reports.
ReadFailed: set to 1 if a card was not read properly repeatedly, indicating it is not an NFC card.

*/

void MachineState(void *pvParameters) {
  uint64_t LastKeyState = 0;
  while(Key1 && Key2){
    //Both keys set to 1 never can actually happen. This means we haven't read new states from the frontend yet.
    delay(10);
  }
  bool OldKey1 = Key1;
  bool OldKey2 = Key2;
  String OldState = State; //Stores the last state of the system.
  while(1){
    //Only needs to run every 2 milliseconds
    vTaskDelay(2 / portTICK_PERIOD_MS);
    //Clear the data after a logoff message is sent, if it doesn't appear there is another card present.
    if(LogoffSent && !CardPresent && !CardUnread){
      SessionTime = 0;
      LogoffSent = 0;
    }
    //The rest of this loop requires the State string, so let's reserve it;
    xSemaphoreTake(StateMutex, portMAX_DELAY);
    if(TemperatureFault && State != "Fault"){
      State = "Fault";
      StateSource = "Fault Temperature";
    }
    if(Fault && State != "Fault"){
      State = "Fault";
      StateSource = "Fault Other";
    }
    
    //Read the key switches and set the state, with a debounce time
    //We only have to do this if there is no network connection, and the state is not fault.
    if(NoNetwork && (State != "Fault")){
      if(millis64() >= LastKeyState){
        //it has been more than the debounce time
        LastKeyState = millis64() + KEYSWITCH_DEBOUNCE;
        if((OldKey1 != Key1) || (OldKey2 != Key2)){
          //A key switch has changed
          OldKey1 = Key1;
          OldKey2 = Key2;
          if(NoNetwork && (State != "Fault")){
            //We only acknowledge the key switch if there is no network connection and we are not in a fault state.
            if(Key1){
              //Locked On
              State = "AlwaysOn";
              StateSource = "Key Switch";
              SessionStart = millis64();
            } 
            else if(!Key1 && !Key2){
              //Locked Off
              //We only want to go into lockout if we've been in this key position for a bit
              //Because this is how the keys report when between positions as well
              byte KeyDebounce = 0;
              while(!Key1 && !Key2 && KeyDebounce <= 250){
                delay(1);
                KeyDebounce++;
              }
              if(KeyDebounce >= 150 && !Key1 && !Key2){
                //We made it the full debounce time
                State = "Lockout";
                StateSource = "Key Switch";
              }
            }
            else if(Key2){
              //Normal Mode
              State = "Idle";
              StateSource = "Key Switch";
            }
          }
        }
      }
    }

    //Check for what state we should be in based on the card, and assert that if it isn't already;
    if(State == "Idle" && CardVerified && CardStatus){
      //The keycard is present and verified, but we are in the idle state.
      //We should be unlocked
      PreUnlockState = State;
      State = "Unlocked";
      StateSource = "Access Granted";
      SessionStart = millis64();
    }
    if(State == "Unlocked" && !CardPresent){
      //The machine is unlocked, but the keycard was removed.
      //We should return to our previous state
      State = PreUnlockState;
      StateSource = "Card Removed";
      EndStatus = 1; //Send a message to the server that the session ended
    }
    //Set the ACS output based on state;
    if(State == "AlwaysOn" || State == "Unlocked"){
      Switch = 1;
    } else{
      //Lockout, Idle, or other unknown modes
      Switch = 0;
    }
    if(State != OldState){
      OldState = State;
      ChangeStatus = 1; //Send a message to the server that the state changed.
    }

    //Check what the saved LastState is. Sometimes this gets into a weird state.
    if((settings.getString("LastState") != "Idle") && (State == "Idle")){
      //Saved state is something weird, even though device is in normal state. Override it.
      settings.putString("LastState","Idle");
      if(DebugMode){
        Serial.println(F("Set LastState in settings to Idle, don't know why it wasn't."));
      }
    }

    //Check if we should lockout based on LockWhenIdle
    if(State == "Idle" && LockWhenIdle){
      State = "Lockout";
      StateSource = "LockWhenIdle, bypassed Idle state";
      if(DebugMode){
        Serial.println(F("Executed LockWhenIdle command."));
      }
      if(ReadyToSend){
        delay(1);
      }
      Message = "Idle state bypassed due to LockWhenIdle.";
      ReadyToSend = 1;
    }

    //Release the semaphore;
    xSemaphoreGive(StateMutex);

    //Check if it is 4am and we need to restart
    if((NightlyFlag == 0) && (rtc.getHour(true) == 4) && (millis64() > 3700000)){
      NightlyFlag = 1;
      if(DebugMode){
        Serial.println(F("Set nightly restart flag."));
      }
    }

    //Restart the machine if not in use and nightlyflag set.
    if((State != "Unlocked") && NightlyFlag){
      settings.putString("LastState", State);
      delay(10);
      //Set the front LED blue
      vTaskSuspend(xHandle);
      Internal.println("L 0,0,255");
      Internal.println("S 0");
      Internal.flush();
      //Tell the server we are shutting down, and close the socket.
      //1.4.1: Turned off nightly message.
      /*
      while(ReadyToSend){
        //Wait for any current message to go
        delay(10);
      }
      Message = "Nightly Restart. Goodnight!";
      ReadyToSend = 1;
      while(ReadyToSend){
        delay(10);
      }
      */
      DisconnectWebsocket = 1;
      while(DisconnectWebsocket){
        delay(10);
      }
      State = "Restart";
      delay(100);
      Serial.println(F("RESTARTING. Source: Timer."));
      settings.putString("ResetReason","Nightly-Restart");
      delay(100);
      ESP.restart();
    }

  }
}