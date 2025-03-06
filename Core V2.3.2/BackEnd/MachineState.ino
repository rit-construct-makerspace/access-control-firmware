/*

Machine State

This task is responsible for the core logic of the system, tieing everything else together.


Global Variables Used:
LogoffSent: Flag to indicate that the status report "Session End" has been sent, so data can be cleared and reset.
State: Plaintext indication of the state of the system for status reports.
ReadFailed: set to 1 if a card was not read properly repeatedly, indicating it is not an NFC card.

*/

void MachineState(void *pvParameters) {
  unsigned long LastKeyState = 0;
  bool OldKey1 = 0;
  bool OldKey2 = 0;
  String OldState; //Stores the last state of the system.
  while(1){
    //Only needs to run every 10 milliseconds
    vTaskDelay(10 / portTICK_PERIOD_MS);
    //Clear the data after a logoff message is sent, if it doesn't appear there is another card present.
    if(LogoffSent && !CardPresent && !CardUnread){
      UID = "";
      SessionTime = 0;
      LogoffSent = 0;
    }
    //The rest of this loop requires the State string, so let's reserve it;
    xSemaphoreTake(StateMutex, portMAX_DELAY);
    //Read the key switches and set the state, with a debounce time
    if(millis() >= LastKeyState){
      //it has been more than the debounce time
      LastKeyState = millis() + KEYSWITCH_DEBOUNCE;
      if((OldKey1 != Key1) || (OldKey2 != Key2)){
        //A key switch has changed
        OldKey1 = Key1;
        OldKey2 = Key2;
        if(Key1){
          //Locked on
          //Entering this mode requires a valid ID, check that now;
          byte delaycount = 0;
          while(!InternalVerified){
            //Delay until the card is verified or removed...
            if(!CardPresent && !CardUnread){
              //A card is not present, read or otherwise. Stop waiting.
              break;
            }
            delaycount++;
            if(delaycount >= 50){
              //Waiting too long. break out.
              break;
            }
            vTaskDelay(1 / portTICK_PERIOD_MS);
          }
          if(InternalStatus || InternalVerified){
            //Card is verified, so set the state;
            State = "AlwaysOn";
          } else{
            //Card was not verified, throw an error
            //TODO
          }
        } else if(!Key1 && !Key2){
          //Locked off
            //We only want to go into lockout if we've been in this key position for a bit
            //Because this is how the keys report when between positions as well
            byte KeyDebounce = 0;
            while(!Key1 && !Key2 && KeyDebounce <= 150){
              vTaskDelay(1 / portTICK_PERIOD_MS);
              KeyDebounce++;
            }
            if(KeyDebounce >= 150 && !Key1 && !Key2){
              //WE made it the full debounce time
              State = "Lockout";
            }
        } else if(Key2){
          //Normal mode
          //Entering this mode requires a valid ID, check that now;
          byte delaycount = 0;
          while(!InternalVerified){
            //Delay until the card is verified or removed...
            if(!CardPresent && !CardUnread){
              //A card is not present, read or otherwise. Stop waiting.
              break;
            }
            delaycount++;
            if(delaycount >= 50){
              //Waiting too long. break out.
              break;
            }
            vTaskDelay(1 / portTICK_PERIOD_MS);
          }
          if(InternalStatus || InternalVerified){
            //Card is verified, so set the state
            State = "Idle";
          } else{
            //Card was not verified, throw an error
            //TODO
          }
        }
      }
    }
    //If we are in a temperature fault, set lockout;
    if(TemperatureFault){
      State == "Lockout";
    }
    //Check for what state we should be in based on the card, and assert that if it isn't already;
    if(State == "Idle" && CardVerified && CardStatus){
      //The keycard is present and verified, but we are in the idle state.
      //We should be unlocked
      State = "Unlocked";
    }
    if(State == "Unlocked" && !CardPresent){
      //The machine is unlocked, but the keycard was removed.
      //We should be in idle mode
      State = "Idle";
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
    //Release the semaphore;
    xSemaphoreGive(StateMutex);
  }
}