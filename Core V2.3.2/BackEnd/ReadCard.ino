/* 

Read Card

This task handles reading cards with the NFC reader, reporting the UID.

Global Variables Used:
DebugMode: Sets verbose output
CardPresent: bool representing if a card has been detected and read
UID: The UID of the detected card.
ReadFailed: set to 1 if a card was not read properly.
SendMessage: set to 1 to indicate a message needs to be sent to the server
Message: String containing error message to report via API
ReadyToSend: set to 1 to indicate "Message" is ready to send.
LogoffSent: Flag to indicate that the status report "Session End" has been sent, so data can be cleared and reset.
NFCFault: Flag indicates that the NFC reader is malfunctioning
CardUnread: Flag that indicates a card is present but not yet read for animation sake.

*/

void ReadCard(void *pvParameters) {
  bool NewCard = 1;  //Used to track if this is the first time the card has been inserted.
  bool StuckSwitch = 0;
  while(1){
    //Check for a card every 50mS
    vTaskDelay(50 / portTICK_PERIOD_MS);
    if(Switch1 && Switch2 && NewCard){
      //A new card has been inserted
      NewCard = 0;
      CardUnread = 1;
      //Loop to retry NFC a few times;
      byte NFCRetryCount = 0;
      bool success;                                //Determines if an NFC read was successful
      uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
      uint8_t uidLength;                           // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
      while(NFCRetryCount <= 5){
        digitalWrite(NFCPWR, HIGH);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        digitalWrite(NFCRST, HIGH);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        nfc.wakeup();
        nfc.setPassiveActivationRetries(0xFF);
        //Attempt to read the card via NFC:
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength, 500);
        if(success){
          Fault = 0;
          break;
        } else{
          if(DebugMode){
            if(xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
              Serial.println(F("Failed to read an NFC card."));
              xSemaphoreGive(DebugMutex);
            }
          }
          digitalWrite(NFCRST, LOW);
          vTaskDelay(50 / portTICK_PERIOD_MS);
          digitalWrite(NFCRST, HIGH);
          vTaskDelay(10 / portTICK_PERIOD_MS);
          NFCRetryCount++;
        }
      }
      if(NFCRetryCount == 6){
        //Card read failed too many times. Must not be an NFC card?
        ReadFailed = 1;
        if(DebugMode){
          if(xSemaphoreTake(DebugMutex,(50/portTICK_PERIOD_MS)) == pdTRUE){
            Serial.println(F("Failed to read card too many times. Maybe not an NFC card?"));
            xSemaphoreGive(DebugMutex);
          }
        }
        //Restart the NFC reader and see if we can detect it.
        digitalWrite(NFCRST, LOW);
        digitalWrite(NFCPWR, LOW);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        digitalWrite(NFCPWR, HIGH);
        digitalWrite(NFCRST, HIGH);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        nfc.wakeup();
        uint32_t versiondata = nfc.getFirmwareVersion();
        if(!versiondata){
          //Wait until we can take the debug port no matter how long it takes...
          xSemaphoreTake(StateMutex, portMAX_DELAY); 
          Serial.println(F("CRITICAL ERROR: NFC READER MALFUNCTION."));
          xSemaphoreGive(DebugMutex);
          Fault = 1;
          ReadyMessage(F("NFC_READER_ERROR"));
          Fault = 1;
          ReadyToSend = 1;
          vTaskSuspend(NULL);
        } else{
          //NFC reader is fine but we couldn't read the card. Definitely not an NFC card.
          ReadError = 1;
        }
      }
      if(success){
        //The NFC reader was successful.
        ReadError = 0;
        UID = "";
        bool readreserved = 0;
        if(DebugMode){
          if(xSemaphoreTake(DebugMutex,(50/portTICK_PERIOD_MS)) == pdTRUE){
            readreserved = 1;
            Serial.print(F("Detected Card: "));
          }
        }
        for (uint8_t i = 0; i < uidLength; i++) {
          if(readreserved){
            Serial.print(uid[i], HEX);
            Serial.print(" ");
          }
          UID += String(uid[i], HEX);
        }
        if(readreserved){
          xSemaphoreGive(DebugMutex);
        }
        CardPresent = 1;
        CardVerified = 0; //Card can't be verified yet
        CardUnread = 0;

        //NEW V1.3.1: Moved internal verification here 
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
        //Next, check against the server if 1) we are in Idle state, or 2) the user could not be found on the internal list.
        if((State == "Idle") || !InternalVerified){
          if(DebugMode){
            Serial.println(F("Verifying UID against server."));
          }
          VerifyID = 1;
        }
      }
    }
    if(Switch1 && Switch2 && !NewCard && !CardUnread && !CardPresent){
      //This is likely an indication of an internal switch issue.
      //We've noticed on many devices they get stuck down over time.
      //Set a flag and see if the state persists next time around;
      if(!StuckSwitch){
        StuckSwitch = 1;
      } else{
        //Still stuck after another round, may be an issue.
        //Send a message to the server.
        ReadyMessage(F("POSSIBLE_STUCK_CARD_SWITCH"));
        Fault = 1;
        vTaskSuspend(NULL);
      }
    }
    if(!Switch1 || !Switch2){
      //Card not present. Reset card-related parameters.
      ReadFailed = 0;
      ReadError = 0;
      CardPresent = 0;
      CardUnread = 0;
      CardVerified = 0;
      InternalVerified = 0;
      InternalStatus = 0;
      UID = "";
    }
    if(!Switch1 && !Switch2){
      //While only one switch needs to be released for us to assume there's no card, both need to be released before we look for a new card.
      //This does mean that a fail-closed switch state will mean you cannot ever activate the machine.
      StuckSwitch = 0;
      NewCard = 1;
    }
  }
}