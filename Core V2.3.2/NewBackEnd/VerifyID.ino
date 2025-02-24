/* 

Verify ID

This task will check IDs against the server for if they are approved or not. This information is then used by MachineState to set the system.

Global Variables Used:
DebugMode: Sets verbose output
DebugPrinting: Used to reserve the debug uart output
CardPresent: bool representing if a card has been detected and read
UID: The UID of the detected card.
CardVerified: Flag set to 1 once the results of the ID check are complete.
CardStatus: Set to 1 if a card is authorized to use the machine, 0 if not,
InternalVerified: Set to 1 when the card has been verified against the internal list.
InternalStatus: Set to 1 if the card is authorized to use the machine based on the internal list.
CheckNetwork: Set to 1 if we have repeated failed network attempts.
LastServer: Tracks the last time we had a good connection to the server.
*/

void VerifyID(void *pvParameters) {
  while(1){
    if(!CardVerified && CardPresent){
      //New card that needs verification
      //First, try to verify the card against the internal list;
      File file = SPIFFS.open("/validids.txt", "r");
      while(file.available()){
        String temp = file.readStringUntil('\r');
        temp.trim();
        if(UID.equalsIgnoreCase(temp)){
          //ID was found in list.
          if(DebugMode && !DebugPrinting){
            DebugPrinting = 1;
            Debug.println(F("ID Found on internal list."));
            DebugPrinting = 0;
          }
          InternalStatus = 1;
          InternalVerified = 1;
          VerifiedBeep = 1;
          break;
        }
      }
      file.close();
      //Now verify against the server;
      bool AuthRetry = 1;
      bool AuthFailed = 0;
      while(AuthRetry){
        if(AuthFailed){
          //This is the second attempt
          AuthRetry = 0;
        }
        //Wait for the network to become available;
        while(UsingNetwork){
          vTaskDelay(1 / portTICK_PERIOD_MS);
        }
        UsingNetwork = 1;
        HTTPClient http;
        String ServerPath = Server + "api/auth?type=" + MachineType + "&machine=" + MachineID + "&zone=" + Zone + "&needswelcome=" + NeedsWelcome + "&id=" + UID;
        if(DebugMode && !DebugPrinting){
          DebugPrinting = 1;
          Debug.print(F("Sending Auth Request: ")); Debug.println(ServerPath);
          DebugPrinting = 0;
        } 
        http.begin(client, ServerPath);
        int httpCode = http.GET();
        if(httpCode < 200 || httpCode > 299){
          //Invalid HTTP code
          if(DebugMode && !DebugPrinting){
            DebugPrinting = 1;
            Debug.print(F("Got invalid HTTP code ")); Debug.println(httpCode);
            DebugPrinting = 0;
          }
          if(AuthFailed == 1){
            //Failed for a second time.
            CheckNetwork = 1;
          }
          AuthFailed = 1;
        } else{
          //Correct HTTP code
          LastServer = millis();
          String payload = http.getString();
          if(DebugMode && !DebugPrinting){
            DebugPrinting = 1;
            Debug.print(F("Got response ")); Debug.println(payload);
            DebugPrinting = 0;
          }
          http.end();
          UsingNetwork = 0;
          JsonDocument auth;
          deserializeJson(auth, payload);
          if((auth["UID"] == UID) && (auth["Allowed"] == 1)){
            //Authorization granted!
            if(DebugMode && !DebugPrinting){
              DebugPrinting = 1;
              Debug.println(F("Authorization Granted."));
              DebugPrinting = 0;
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
          } else{
            //Authorization Denied!
            if(DebugMode && !DebugPrinting){
              DebugPrinting = 1;
              Debug.println(F("Authorization Denied!"));
              DebugPrinting = 0;
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