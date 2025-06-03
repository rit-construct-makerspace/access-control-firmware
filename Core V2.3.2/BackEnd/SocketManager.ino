/*

Socket Manager

This task is responsible for sending and receiving all network traffic via websockets.


*/

void SocketManager(void *pvParameters) {    
  JsonDocument wsresp;
  #ifndef WebsocketUART
    StartWebsocket();
    //Wait to connect before continuing
    while(!socket.isConnected()){
      delay(10);
    }
  #endif

  Serial.println("Websocket Initial Connection.");
  
  while (1) {
    delay(50);

    #ifdef WebsocketUART
      NoNetwork = 0;
      if(Serial.available() > 0){
        WSIncoming = Serial.readString();
        WSIncoming.trim();
        NewFromWS = 1;
      }
    #endif

    //First, check if there is a new message:
    if(NewFromWS){
      //If we managed to get a message input, that means the firmware is good enough to mark as valid.
      const esp_partition_t *running_partition = esp_ota_get_running_partition();
      esp_ota_img_states_t ota_state;
      esp_ota_get_state_partition(running_partition, &ota_state);
      if(ota_state == ESP_OTA_IMG_PENDING_VERIFY){
        esp_ota_mark_app_valid_cancel_rollback();
        Serial.println(F("OTA update marked valid."));
        Message = "OTA update marked valid.";
        ReadyToSend = 1;
      }
      NewFromWS = 0;
      bool alreadyAuth = 0;
      if(DebugMode){
        Serial.println(F("We got a websocket message:"));
        Serial.println(WSIncoming);
      }
      JsonDocument wsin;
      //Get all the info from the incoming message
      deserializeJson(wsin, WSIncoming);
      //Iterate through all keys to update accordingly
      for (JsonPair kv : wsin.as<JsonObject>()) {
        if((kv.key() == "Verified") || (kv.key() == "Auth") || (kv.key() == "Role") || kv.key() == "Reason"){
          //These are all the keys we expect when we are dealing with an authentication response
          if(!alreadyAuth){
            //We haven't processed this yet
            if(DebugMode){
              Serial.println(F("Appears to be an authentication message."));
            }
            alreadyAuth = 1;
            authUser(wsin);
          }
        }
        if(kv.key() == "Key"){
          //There's a new key to set
          settings.putString("Key", wsin["Key"].as<String>());
          Key = settings.getString("Key");
          if(DebugMode){
            Serial.println(F("New key saved and updated."));
          }
        }
        if(kv.key() == "Identify"){
          //Set the Identify mode, to easily find a shlug
          Identify = wsin["Identify"].as<bool>();
        }
        if(kv.key() == "State"){
          //Immediately set the state of the machine to this
          bool ServerStateSet = 0;
          String WSState = wsin["State"].as<String>();
          if(State.equalsIgnoreCase(WSState)){
            if(DebugMode){
              Serial.println(F("State change to same state. Ignoring."));
            }
          } 
          else{
            if(WSState.equalsIgnoreCase("Restart")){
              //The system has ordered an immediate restart of the device.
              if(DebugMode){
                Serial.println(F("Restart Ordered."));
              }
              xSemaphoreTake(StateMutex, portMAX_DELAY);
              settings.putString("LastState", State);
              delay(10);
              State = "Restart";
              ServerStateSet = 1;
              //Turn off the internal write task so that it doesn't overwrite the restart led color.
              vTaskSuspend(xHandle);
              delay(100);
              settings.putString("ResetReason","Server-Ordered");
              Internal.println("L 255,0,0");
              Internal.println("S 0");
              Internal.flush();
              ESP.restart();
            }
            //If the state is fault, we do not accept anything but restart.
            if(State == "Fault"){
              if(DebugMode){
                Serial.println(F("Unable to change state from Fault. Must restart."));
              }
            } else{
              if(WSState.equalsIgnoreCase("Fault")){
                //Ordered to an irreversible fault state
                Fault = 1;
                State = "Fault";
                ServerStateSet = 1;
                Serial.println(F("FAULTING"));
              }
              //Some special fixings:
              //If the state is "Unlocked" the machine gets confused when we change state with a card still present.
              //So, act like the card was removed
              if(State == "Unlocked"){
                CardPresent = 0;
                CardVerified = 0;
              }
              if(WSState.equalsIgnoreCase("Unlocked")){
                if(CardPresent){
                  PreUnlockState = State;
                  State = "Unlocked";
                  ServerStateSet = 1;
                } else{
                  if(DebugMode){
                    Serial.println(F("Cannot set Unlocked, no card present!"));
                  }
                }
              }
              if(WSState.equalsIgnoreCase("Idle")){
                State = "Idle";
                ServerStateSet = 1;
              }
              if(WSState.equalsIgnoreCase("Lockout")){
                State = "Lockout";
                ServerStateSet = 1;
              }
              if(WSState.equalsIgnoreCase("AlwaysOn")){
                State = "AlwaysOn";
                ServerStateSet = 1;
              }
              if(WSState.equalsIgnoreCase("Startup")){
                State = "Startup";
                ServerStateSet = 1;
              }
              if(ServerStateSet){
                if(DebugMode){
                  Serial.print(F("State remotely set to: "));
                  Serial.println(State);
                }
                ChangeBeep = 1;
                StateSource = "Server";
              } else{
                if(DebugMode){
                  Serial.print(F("Got an unexpected state set: "));
                  Serial.println(WSState);
                }
                Message = "Got unexpected state set: " + WSState;
                ReadyToSend = 1;
              }
            }
          }
        }
        if(kv.key() == "Time"){
          //Set the time to this (unix seconds)
          rtc.setTime(wsin["Time"].as<unsigned long long>());
          if(DebugMode){
            Serial.print(F("RTC time set to: "));
            Serial.println(rtc.getDateTime(true));
          }
        }
        if(kv.key() == "TempLimit"){
          //Set the temperature limit
          TempLimit = wsin["TempLimit"].as<unsigned int>();
          settings.putString("TempLimit",String(TempLimit));
          if(DebugMode){
            Serial.print(F("Set Temperature Limit to: "));
            Serial.print(TempLimit);
            Serial.println(F(" degrees C."));
          }
        }
        if(kv.key() == "Brightness"){
          //Set the overall brightness
          Brightness = wsin["Brightness"].as<byte>();
          settings.putString("Brightness",String(Brightness));
          if(DebugMode){
            Serial.print(F("Brightness set to:"));
            Serial.print(Brightness);
            Serial.println(F(" / 255."));
          }
        }
        //TODO Eventually check for UseEthernet and UseWiFi here. or NetworkMode.
        if(kv.key() == "Frequency"){
          Frequency = wsin["Frequency"].as<unsigned int>();
          settings.putString("Frequency", String(Frequency));
          if(DebugMode){
            Serial.print(F("Regular message frequency set to once every: "));
            Serial.print(Frequency);
            Serial.println(F(" seconds."));
          }
        }
        if(kv.key() == "NoBuzzer"){
          NoBuzzer = wsin["NoBuzzer"].as<bool>();
          settings.putString("NoBuzzer",String(NoBuzzer));
          if(DebugMode){
            if(NoBuzzer){
              Serial.println(F("Buzzer Disabled"));
            } else{
              Serial.println(F("Buzzer Enabled"));
            }
          }
        }
        if(kv.key() == "Request"){
          //The server has requested some values from us.
          //We send back the sequence number in the response, so the server knows what the response is in regards to
          wsresp["Seq"] = wsin["Seq"];
          //Extract each element of the array;
          size_t N = wsin["Request"].size();
          for (size_t i=0; i < N; i++){
            if(DebugMode){
              Serial.print(F("Server Requested: "));
              Serial.println(wsin["Request"][i].as<String>());
            }
            //Go through all possible requests and fulfill them:
            if(wsin["Request"][i] == "UID"){
              wsresp["UID"] = UID;
            }
            if(wsin["Request"][i] == "Message"){
              wsresp["Message"] = Message;
            }
            if(wsin["Request"][i] == "Key"){
              wsresp["Key"] = Key;
            }
            if(wsin["Request"][i] == "FWVersion"){
              wsresp["FWVersion"] == Version;
            }
            if(wsin["Request"][i] == "HWVersion"){
              wsresp["HWversion"] = Hardware;
            }
            if(wsin["Request"][i] == "HWType"){
              wsresp["HWType"] = "ACS Core";
            }
            if(wsin["Request"][i] == "State"){
              wsresp["State"] = State;
            }
            if(wsin["Request"][i] == "TempLimit"){
              wsresp["TempLimit"] = TempLimit;
            }
            if(wsin["Request"][i] == "Brightness"){
              wsresp["Brightness"] = Brightness;
            }
            if(wsin["Request"][i] == "NetworkMode"){
              //Report what network mode we are in - sanitize it to how the server expects
              wsresp["WifiAllowed"] = UseWiFi;
              wsresp["EthernetAllowed"] = UseEthernet;
              wsresp["EthernetPresent"] = EthernetPresent;
            }
            if(wsin["Request"][i] == "NoBuzzer"){
              wsresp["NoBuzzer"] = NoBuzzer;
            }      
            if(wsin["Request"][i] == "Frequency"){
              wsresp["Frequency"] = Frequency;
            }                                    
          }
          //Prime us to send the response:
          WSSend = 1;
        }
      }
    }
    //There are very few reasons we need to send a message. 
    //Check if each is the case:
    //0: We just (re)connected
    if(SendWSReconnect && !WSSend){
      wsresp["Key"] = Key;
      wsresp["State"] = State;
      wsresp["FWVersion"] = Version;
      wsresp["HWVersion"] = Hardware;
      wsresp["HWType"] = "ACS Core";
      if(devices == 0){
        //The Onewire manager hasn't finished finding IDs yet
        delay(50);
      }
      //Now that we have found the onewire devices, we should know our serial number;
      wsresp["SerialNumber"] = SerialNumber;
      //And we also know the hardware IDs of the entire system;
      for (uint8_t i = 0; i < devices; i += 1) {
        String TempID = String(SerialNumbers[i],HEX);
        TempID.toUpperCase();
        wsresp["HardwareIDs"][i] = TempID;
      }
      //Determine if this is the first time we are sending this since startup. 
      if(MessageNumber > 0){
        //We've sent messages in the past, reset the message count but no need to ask for anything special
        MessageNumber = 0;
      } else{
        //This is the first message, so ask what state we should be in and what time it is
        wsresp["Request"][0] = "State";
        wsresp["Request"][1] = "Time";
      }
      WSSend = 1;
      SendWSReconnect = 0;
    }
    //1: Authenticating an ID
    if(VerifyID && !WSSend){
      wsresp["Auth"] = UID;
      WSSend = 1;
      VerifyID = 0;
    }
    //2: There's an error message to send
    if(ReadyToSend && !WSSend){
      wsresp["Message"] = Message;
      WSSend = 1;
      ReadyToSend = 0;
      WritingMessage = 0;
    }
    //3: The state changed
    if(ChangeStatus && !WSSend){
      wsresp["State"] = State;
      wsresp["UID"] = UID;
      wsresp["StateSource"] = StateSource;
      WSSend = 1;
      ChangeStatus = 0;
    }
    //4: It is time for a regular report
    if(RegularStatus && !WSSend){
      wsresp["State"] = State;
      wsresp["UID"] = UID;
      WSSend = 1;
      RegularStatus = 0;
    }
    delay(1);
    if(WSSend){
      //There is something to send
      WSSend = 0;
      wsresp["Seq"] = MessageNumber;
      MessageNumber++;
      String WSToSend;
      serializeJson(wsresp, WSToSend);
      wsresp.clear(); //Wipe the JSON for the next write session
      if(DebugMode){
        Serial.print(F("Sending Websocket Message:" ));
        Serial.println(WSToSend);
      }
      //This seems to be causing the crash?
      //Is it because we are sending before connected?
      if(socket.isConnected()){
        //If we don't have a connection, we don't want to send
        socket.sendTXT(WSToSend);
        if(DebugMode){
          Serial.println(F("Sent."));
        }
      } else{
        if(DebugMode){
          Serial.println(F("Didn't send because of lost connection. Dropping message."));
        }
      }
    }
    //Check if it has been long enough to send a ping to the server.
    if((millis64() >= NextPing) && socket.isConnected() && !PingPending){
      PingTimeout = millis64() + 1000;
      PingPending = 1;
      socket.sendPing();
      if(DebugMode){
        Serial.println(F("Ping..."));
      }
    }
    //Check if we have waited to long to get a ping response.
    if(PingPending && millis64() >= PingTimeout){
      if(DebugMode){
        Serial.println(F("Did not get a ping in time! Websocket issue?"));
      }
      PingPending = 0;
      NoNetwork = 1;
      socket.disconnect();
    }
  }
}

void authUser(JsonDocument input){
  //Verifies the user based on the contents of the JSON payload.
  if(DebugMode){
    Serial.println(F("Parsing Auithentication Data..."));
  }
  //Step 1: Check the UID matches what's currently in the machine
  if(input["Auth"].as<String>() != UID){
    if(DebugMode){
      Serial.println(F("UIDs Do not match!"));
    }
    return;
  }
  //Step 2: Check if the user was approved or denied
  if(input["Verified"].as<bool>() == 0){
    //Access Denied
    CardStatus = 0;
    CardVerified = 1;
    InternalStatus = 0;
    InternalVerified = 1;
    if(DebugMode){
      Serial.print(F("Access Denied. Reason: "));
      Serial.println(input["Reason"].as<String>());
    }
    return;
  }
  //If we are here, that means the user was approved:
  CardStatus = 1;
  CardVerified = 1;
  if(DebugMode){
    Serial.println(F("Access Granted."));
  }
  if(!InternalStatus){
  //Card was not authorized based on the internal list. Must not be on the list. Adding.
  File file = SPIFFS.open("/validids.txt",FILE_APPEND);
  file.println(UID);
  file.close();
  InternalVerified = 1;
  VerifiedBeep = 1;
  InternalStatus = 1;
  }
}


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  //We check for OTA validity on the receipt of a text message. Need this info to do that;
  switch(type) {
    case WStype_DISCONNECTED:
      if(JustDisconnected){
        //Stops spam if we have no connection
        Serial.printf("[WSc] Disconnected!\n");
        JustDisconnected = 0;
      }

      NoNetwork = 1;
      WSSend = 0; //Clear out anything that needs to be sent
      break;
    case WStype_CONNECTED:
      JustDisconnected = 1;
      Serial.println(F("Connected."));
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      SendWSReconnect = 1;
      NoNetwork = 0;
      NextPing = millis64() + 2000;
      break;
    case WStype_TEXT:
      WSIncoming = String((char *)payload, length);
      NewFromWS = 1;
      NextPing = millis64() + 2000;
      break;
    case WStype_PONG:
      PingPending = 0;
      if(DebugMode){
        Serial.println(F("Pong!"));
      }
      NextPing = millis64() + 2000;
      break;
    case WStype_ERROR:
      Serial.println(F("Got a websocket error!"));
      break;
  }
}


void StartWebsocket(){
  //This function starts the websocket
  //socket.beginSslWithBundle(Server.c_str(), 443, "/api/ws", NULL, 0, "");
  socket.begin(Server.c_str(), 80, "/api/ws");
  socket.onEvent(webSocketEvent);
  socket.setReconnectInterval(1000); //Attempt to reconnect every second if we lose connection
}