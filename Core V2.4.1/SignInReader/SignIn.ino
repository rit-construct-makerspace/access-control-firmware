/*

  Sign In is the main code that handles checking for IDs, sending them to the server, etc.

*/

void SignIn(void *pvParameters){
  uint8_t uid_len;
  uint8_t uid[10] = {0};  // uids are maximum of 10 bytes long.
  uint8_t sak;
  while(1){
    delay(50);
    //First, check if there is a new card;
    uint16_t atqa = mfrc630_iso14443a_REQA();

    //Get the UID:
    if(atqa != 0){
      idleCount = 0;
      memset(uid, 0, sizeof(uid));
      uid_len = mfrc630_iso14443a_select(uid, &sak);
      //Check the UID Length:
      USBSerial.print(F("UID Length: ")); USBSerial.println(uid_len);
      if(uid_len != nfcValidLength && nfcValidLength != 0){
        USBSerial.print(F("Invalid UID Length. Expected: ")); USBSerial.println(nfcValidLength);
        invalidCard = true;
      }
      //Once we have the UID, we attempt to access the card's encrypted contents.
      //This is not actually used by the code, but its inevitable failure causes the card to break the connection, and respond to subsequent REQAs
      //This essentially lets us detect the presence of a card.
      //Without this, the card wouldn't respond to the next REQA, since we are already communicating.
      // Use the manufacturer default key...
      uint8_t FFkey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      mfrc630_cmd_load_key(FFkey);  // load into the key buffer
      mfrc630_MF_auth(uid, MFRC630_MF_AUTH_KEY_A, 0);
    }

    if(atqa == 0){ 
      //No cards in range, reset states
      idleCount++;
      if(!ready){
        USBSerial.println(F("ready to read card."));
      }
      ready = true;
      notInSystem = false;
      inSystem = false;
      invalidCard = false;
      buzzerStart = true;
      fault = false;
    } 
    else if(ready && !invalidCard){
      //If there is a card in range and we are ready to receive it;
      ready = false;
      
      USBSerial.print(F("REQA Response: ")); USBSerial.println(atqa);
      if(nfcValidREQA != atqa && nfcValidREQA != 0){
        //More than 1 card found, error out
        invalidCard = true;
        USBSerial.print(F("Invalid REQA. Expected: ")); USBSerial.println(nfcValidREQA);
        continue;
      }
      //Check the SAK
      USBSerial.print(F("SAK Response: ")); USBSerial.println(sak);
      if(sak != nfcValidSAK && nfcValidSAK != 0){
        USBSerial.print(F("Invalid SAK Response. Expected: ")); USBSerial.println(nfcValidSAK);
        invalidCard = true;
        continue;
      }
      USBSerial.println(F("ATQA, SAK, and UID Length are valid."));
      USBSerial.print(F("UID:"));
      String UID;
      for (uint8_t i=0; i<uid_len; i++){
        USBSerial.print(" ");
        USBSerial.print(uid[i], HEX);
        UID.concat(String(uid[i], HEX));
      }
      USBSerial.println();

      //Now let's send the UID to the server;
      doc.clear();
      doc["Type"] = "Welcome";
      doc["zone"] = zone;
      doc["ID"] = UID;
      doc["key"] = key;
      String Serialized;
      serializeJson(doc, Serialized);
      String serverPath = server + "/api/welcome";
      networkTime = millis64();
      
      // Set timeout and connection parameters for improved reliability
      http.setTimeout(10000); // 10 second timeout
      http.begin(client, serverPath);
      http.addHeader("Content-Type","application/json");
      
      // Track time for performance monitoring
      uint64_t requestStartTime = millis64();
      int httpCode = http.PUT(Serialized);
      uint64_t requestDuration = millis64() - requestStartTime;
      
      // Log performance metrics
      USBSerial.print(F("Network request completed in: "));
      USBSerial.print(requestDuration);
      USBSerial.println(F(" ms"));
      USBSerial.print(F("HTTP Response Code: ")); USBSerial.println(httpCode);
      
      if(httpCode == 202){
        USBSerial.println(F("User found in system."));
        networkError = 0;
        inSystem = true;
      }
      else if(httpCode == 406){
        USBSerial.println(F("User not in system"));
        networkError = 0;
        notInSystem = true;
      }
      else{
        USBSerial.println(F("Unexpected HTTP Response."));
        fault = true;
        if(httpCode < 0){
          USBSerial.println(F("Indication of Network Failure?"));
          networkError++;
        }
      }
    }

    // If we've been idle too long, ping the server to keep connection alive
    if(idleCount >= IDLE_THRESHOLD){
      http.end();
      networkCheck = true;
      idleCount = 0;
      
      LEDColor(255, 255, 255);
      String serverPath = server + "/api/state/" + STATE_TARGET;
      
      // 10 second timeout
      http.setTimeout(10000);
      http.begin(client, serverPath);
      
      int resp = http.GET();
      USBSerial.println(F("Keeping server connection alive."));
      USBSerial.print(F("Response to state check: ")); USBSerial.println(resp);
      
      if(resp < 0){
        USBSerial.println(F("Network error detected."));
        
        networkError++;
        
        if(networkError > 3) {
          USBSerial.println(F("Multiple network failures. Restarting."));
          delay(500);
          ESP.restart();
        }
      } else {
        networkError = 0;
      }
      
      networkCheck = false;
      LEDColor(0,0,0);
      http.end();
    }
  }
}