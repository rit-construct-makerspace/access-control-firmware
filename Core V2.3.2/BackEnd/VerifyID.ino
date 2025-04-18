/* 

Verify ID

This task will check IDs against the server and internal list for if they are approved or not. This information is then used by MachineState to set the system.

Global Variables Used:
DebugMode: Sets verbose output
CardPresent: bool representing if a card has been detected and read
UID: The UID of the detected card.
CardVerified: Flag set to 1 once the results of the ID check are complete.
CardStatus: Set to 1 if a card is authorized to use the machine, 0 if not,
InternalVerified: Set to 1 when the card has been verified against the internal list.
InternalStatus: Set to 1 if the card is authorized to use the machine based on the internal list.
CheckNetwork: Set to 1 if we have repeated failed network attempts.
LastServer: Tracks the last time we had a good connection to the server.
*/

void VerifyID(void *pvParameters){
  while(1){
    //Check every 50mS
    vTaskDelay(50 / portTICK_PERIOD_MS);
    if(!InternalVerified && CardPresent){
      //New card that needs verification
      //First, try to verify the card against the internal list;
      File file = SPIFFS.open("/validids.txt", "r");
      while(file.available()){
        String temp = file.readStringUntil('\r');
        temp.trim();
        if(UID.equalsIgnoreCase(temp)){
          //ID was found in list.
          if(DebugMode){
            if(xSemaphoreTake(DebugMutex,(portMAX_DELAY)) == pdTRUE){
              Serial.println(F("ID Found on internal list."));
              xSemaphoreGive(DebugMutex);
            }
          }
          InternalStatus = 1;
          InternalVerified = 1;
          VerifiedBeep = 1;
          break;
        }
      }
      file.close();
      //We only need to verify against the server if;
      //1. We are in idle state OR
      //2. The ID could not be verified on the internal list.
      if(InternalVerified && State != "Idle"){
        //No need to verify, exit out.
        if(DebugMode){
          if(xSemaphoreTake(DebugMutex,(portMAX_DELAY)) == pdTRUE){
            Serial.println(F("No need to check against server."));
            xSemaphoreGive(DebugMutex);
          }
        }
        continue;
      }
      //Now verify against the server;
      bool AuthRetry = 1;
      bool AuthFailed = 0;
      if(NoNetwork){
        //Since we can't reach the network, we are just going to go off of what the internal list says.
        if(DebugMode){
          xSemaphoreTake(DebugMutex, portMAX_DELAY);
          Serial.println(F("No network, so verifying card against internal list."));
          xSemaphoreGive(DebugMutex);
        }
        CardStatus = InternalStatus;
        CardVerified = 1;
        continue;
      }
      while(AuthRetry){
        if(AuthFailed){
          //This is the second attempt
          AuthRetry = 0;
        }
        //We only need to check for needs welcome if the machine is in idle mode. Should be able to unlock without signing into the shop.
        String NeedsWelcomeTemp;
        if(State == "Idle"){
          NeedsWelcomeTemp = NeedsWelcome;
        } else{
          NeedsWelcomeTemp = "0";
        }
        //Reserve the network;
        xSemaphoreTake(NetworkMutex, portMAX_DELAY); 
        String ServerPath = Server + "/api/auth?type=" + MachineType + "&machine=" + MachineID + "&zone=" + Zone + "&needswelcome=" + NeedsWelcomeTemp + "&id=" + UID;
        if(DebugMode && xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
          Serial.print(F("Sending Auth Request: ")); Serial.println(ServerPath);
          xSemaphoreGive(DebugMutex);
        } 
        http.begin(client, ServerPath);
        int httpCode = http.GET();
        if(DebugMode){
          if(xSemaphoreTake(DebugMutex,(portMAX_DELAY)) == pdTRUE){
            Serial.print(F("Got HTTP: ")); Serial.println(httpCode);
            xSemaphoreGive(DebugMutex);
          }
        }
        //HTTP Codes:
        //202: ID Approved
        //406: ID Denied
        //401: ID Denied (Not Documented?)
        if(httpCode != 202 && httpCode != 406 && httpCode != 401){
          //Invalid HTTP code
          if(DebugMode){
            if(xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
              Serial.print(F("Got invalid HTTP code ")); Serial.println(httpCode);
              xSemaphoreGive(DebugMutex);
            }
          }
          if(AuthFailed == 1){
            //Failed for a second time.
            CheckNetwork = 1;
          }
          AuthFailed = 1;
          http.end();
          //Release the mutex
          xSemaphoreGive(NetworkMutex);
          continue;
        } else{
          //Correct HTTP code
          AuthRetry = 0;
          LastServer = millis64();
          String payload = http.getString();
          if(DebugMode){
            if(xSemaphoreTake(DebugMutex,(portMAX_DELAY)) == pdTRUE){
              Serial.print(F("Got response ")); Serial.println(payload);
              xSemaphoreGive(DebugMutex);
            }
          }
          http.end();
          //Release the mutex
          xSemaphoreGive(NetworkMutex);
          if(!CardPresent){
            //The card was removed while we were waiting for the network :(
            if(DebugMode){
              if(xSemaphoreTake(DebugMutex,(portMAX_DELAY)) == pdTRUE){
                Serial.println(F("Card removed during auth request. Aborting."));
                xSemaphoreGive(DebugMutex);
              }
            }
            continue;
          }
          JsonDocument auth;
          deserializeJson(auth, payload);
          if((auth["UID"] == UID) && (auth["Allowed"] == 1)){
            //Authorization granted!
            if(DebugMode){
              if(xSemaphoreTake(DebugMutex,(portMAX_DELAY)) == pdTRUE){
                Serial.println(F("Authorization Granted."));
                xSemaphoreGive(DebugMutex);
              }
            }
            CardStatus = 1;
            CardVerified = 1;
            if(!InternalStatus){
              //Card was not authorized based on the internal list. Must not be on the list. Adding.
              File file = SPIFFS.open("/validids.txt",FILE_APPEND);
              file.println(UID);
              file.close();
              InternalVerified = 1;
              VerifiedBeep = 1;
              InternalStatus = 1;
            }
            continue;
          } else{
            //Authorization Denied!
            if(DebugMode){
              if(xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
                Serial.println(F("Authorization Denied!"));
                xSemaphoreGive(DebugMutex);
              }
            }
            //Set permission level to 0 on both internal and card. 
            CardStatus = 0;
            CardVerified = 1;
            InternalStatus = 0;
            InternalVerified = 1;
          }
        }
      }
    }
  }
}