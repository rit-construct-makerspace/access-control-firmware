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
      IdleCount = 0;
      uid[10] = {0};
      uid_len = mfrc630_iso14443a_select(uid, &sak);
      //Check the UID Length:
      USBSerial.print(F("UID Length: ")); USBSerial.println(uid_len);
      if(uid_len != ValidLength && ValidLength != 0){
        USBSerial.print(F("Invalid UID Length. Expected: ")); USBSerial.println(ValidLength);
        InvalidCard = 1;
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
      IdleCount++;
      if(!Ready){
        USBSerial.println(F("Ready to read card."));
      }
      Ready = 1;
      NotInSystem = 0;
      InSystem = 0;
      InvalidCard = 0;
      BuzzerStart = 1;
      Fault = 0;
    } 
    else if(Ready && !InvalidCard){
      //If there is a card in range and we are ready to receive it;
      Ready = 0;
      USBSerial.print(F("REQA Response: ")); USBSerial.println(atqa);
      if(ValidREQA != atqa && ValidREQA != 0){
        //More than 1 card found, error out
        InvalidCard = 1;
        USBSerial.print(F("Invalid REQA. Expected: ")); USBSerial.println(ValidREQA);
        continue;
      }
      //Check the SAK
      USBSerial.print(F("SAK Response: ")); USBSerial.println(sak);
      if(sak != ValidSAK && ValidSAK != 0){
        USBSerial.print(F("Invalid SAK Response. Expected: ")); USBSerial.println(ValidSAK);
        InvalidCard = 1;
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
      doc["Zone"] = Zone;
      doc["ID"] = UID;
      doc["Key"] = Key;
      String Serialized;
      serializeJson(doc, Serialized);
      String ServerPath = Server + "/api/welcome";
      NetworkTime = millis64();
      http.begin(client, ServerPath);
      http.addHeader("Content-Type","application/json");
      int httpCode = http.PUT(Serialized);
      USBSerial.print(F("HTTP Response Code: ")); USBSerial.println(httpCode);
      if(httpCode == 202){
        USBSerial.println(F("User found in system."));
        NetworkError = 0;
        InSystem = 1;
      }
      else if(httpCode == 406){
        USBSerial.println(F("User not in system"));
        NetworkError = 0;
        NotInSystem = 1;
      }
      else{
        USBSerial.println(F("Unexpected HTTP Response."));
        Fault = 1;
        if(httpCode < 0){
          USBSerial.println(F("Indication of Network Failure?"));
          NetworkError++;
        }
      }
    }

    //If we've been idle too long, ping the server to keep connection alive
    if(IdleCount >= IDLE_THRESHOLD){
      http.end();
      NetworkCheck = 1;
      LEDColor(255, 255, 255);
      IdleCount = 0;
      String ServerPath = Server + "/api/state/" + STATE_TARGET;
      http.begin(client, ServerPath);
      int resp = http.GET();
      USBSerial.println(F("Keeping server connection alive."));
      USBSerial.print(F("Response to state check: ")); USBSerial.println(resp);
      NetworkCheck = 0;
      LEDColor(0,0,0);
      http.end();
    }
  }
}
