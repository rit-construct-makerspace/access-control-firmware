/*

Socket Manager

This task is responsible for sending and receiving all network traffic via websockets.


*/

bool SendInfoMessage = 1; //this starts as a 1 when code starts, set to 0 once the message goes through once
bool ServerStateSet = 0; //Used to track if we changed states when the server commanded it.

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
  
  unsigned long long LastFromWS = 0;

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
      LastFromWS = millis64();
      if(DebugMode){
        Serial.println(F("We got a websocket message:"));
        Serial.println(WSIncoming);
      }
      JsonDocument wsin;
      //Get all the info from the incoming message
      deserializeJson(wsin, WSIncoming);
      //Determine how to act based on the key at the top-level;
      if(wsin.containsKey("authTo")){
        //The JSON payload contains information about an attempted auth request from a card recently inserted.

        //Needs a rewrite...
        //bool IsAuthed true false
        //String AuthID is the card tag we should have
        bool IsAuthed = wsin["authTo"]["channels"][0]["approved"].as<bool>();
        String AuthID = wsin["authTo"]["cardTagID"].as<String>();

        if(IsAuthed){
          //Access request approved!
          //Before we accept it, let's make sure the AuthID matches the current ID;
          if(AuthID == UID){
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
          } else{
            if(DebugMode){
              Serial.println(F("Access was granted, but card IDs mismatch!"));
            }
          }
        } 
        else{
          //Access request denied!
          CardStatus = 0;
          CardVerified = 1;
          InternalStatus = 0;
          InternalVerified = 1;
          if(DebugMode){
            Serial.print(F("Access Denied."));
          }
        }
      }
      if(wsin.containsKey("info")){
        //The JSON payload contains responses to our information request we send on startup.
        JsonObject infoObj = wsin["info"];
        if(infoObj.containsKey("time")){
          //The message has the unix time for us.
          unsigned long long millisecondTime = infoObj["time"].as<unsigned long long>();
          rtc.setTime(millisecondTime/1000);
          if(DebugMode){
            Serial.print(F("Time set to: "));
            Serial.println(rtc.getDateTime(true));
          }
        }
        if(infoObj.containsKey("state")){
          //The message has the state we should be in, we should change to it if we are in a startup state only.
          if(State == "Startup"){
            //take the incoming API state and change it to our internal state
            String IncomingState = APIStateToInternal(infoObj["state"][0]["state"]);
            if(IncomingState != "Fault"){
              State = IncomingState;
              ChangeBeep = 1;
              StateSource = "COMMANDED";
              if(DebugMode){
                Serial.print(F("Set state to "));
                Serial.print(State);
                Serial.println(F("from 'info' message on startup"));
              }
            } 
            else{
              State = "Lockout";
              ChangeBeep = 1;
              StateSource = "COMMANDED";
              if(DebugMode){
                Serial.println(F("Ignoring command to fault state on startup."));
              } 
            }
          } else{
            if(DebugMode){
              Serial.println(F("Ignoring state from info payload, not in startup state to accept it."));
            }
          }
        }
      }
      if(wsin.containsKey("command")){
        //The JSON payload contains information on a command, such as setting flags or states.
        JsonObject commandObj = wsin["command"];
        //Need to check for each of the possible command keys;
        if(commandObj.containsKey("toState")){
          //Command to set a state
          String WSState = commandObj["toState"][0]["state"];
          //First, convert the WSState to the internal state syntax.
          WSState = APIStateToInternal(WSState);
          //Compare the commanded state to the current state.
          if(State == WSState){
            if(DebugMode){
              Serial.println(F("Ignoring state change command, same as current."));
            }
          } else{
            //This is a new state command.
            if(State == "Fault"){
              //We do not permit commanding out of a fault state.
              Serial.println(F("Commanded state ignored, in fault mode!"));
            } else{
              //We should swap to this state.
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
                ServerStateSet = 1;
                if(CardPresent){
                  //There is a card present
                  PreUnlockState = State;
                  State = "Unlocked";
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
            }
          }
          if(ServerStateSet){
            if(DebugMode){
              Serial.print(F("State remotely set to: "));
              Serial.println(State);
            }
            ChangeBeep = 1;
            StateSource = "COMMANDED";
          }
        }
        if(commandObj.containsKey("action")){
          //Command to execute an action
          //There are 3 possible actions supported;
          if(commandObj["action"] == "RESTART"){
            //Restart the device immediately
            if(DebugMode){
              Serial.println(F("Restart Ordered."));
            }
            xSemaphoreTake(StateMutex, portMAX_DELAY);
            settings.putString("LastState", State);
            xSemaphoreGive(StateMutex);
            delay(10);
            State = "Restart";
            ServerStateSet = 1;
            ChangeBeep = 1;
            while(ChangeBeep){
              //Wait for the change beep to happen
              delay(10);
            }
            delay(100);
            //Turn off the internal write task so that it doesn't overwrite the restart led color.
            vTaskSuspend(xHandle);
            delay(100);
            settings.putString("ResetReason","Server-Ordered");
            Internal.println("L 255,0,0");
            delay(10);
            Internal.println("S 0");
            delay(10);
            Internal.flush();
            delay(10);
            ESP.restart();
          }
          if(commandObj["action"] == "SEAL"){
            //Lock the current configuration as a valid setup
            if(!SealBroken){
              if(DebugMode){
                Serial.println(F("Seal commanded, but seal also intact."));
              }
            } else{
              //Re-seal the deployment as it currently stands.
              Serial.println(F("Commiting current bus to memory..."));
              ReSealBus = 1;
              while(ReSealBus){
                delay(1);
              }
              //If we make it here, the seal is all set to.
              //Time for a restart to commit this all.
              if(DebugMode){
                Serial.println(F("Restart Ordered."));
              }
              xSemaphoreTake(StateMutex, portMAX_DELAY);
              settings.putString("LastState", State);
              xSemaphoreGive(StateMutex);
              delay(10);
              State = "Restart";
              ServerStateSet = 1;
              ChangeBeep = 1;
              while(ChangeBeep){
                //Wait for the change beep to happen
                delay(10);
              }
              delay(100);
              //Turn off the internal write task so that it doesn't overwrite the restart led color.
              vTaskSuspend(xHandle);
              delay(100);
              settings.putString("ResetReason","Server-Ordered");
              Internal.println("L 255,0,0");
              delay(10);
              Internal.println("S 0");
              delay(10);
              Internal.flush();
              delay(10);
              ESP.restart();
            }
          }
          if(commandObj["action"] == "IDENTIFY"){
            //Identify the device
            if(DebugMode){
              Serial.println(F("Server commanded identify."));
            }
            Identify = 1;
          }
        }
        if(commandObj.containsKey("identifyChannel")){
          //This is technically meant to identify the unique channel of an access control system, we use it instead to identify the device since we are a one-channel-only device.
          Identify = 1;
        }
        if(commandObj.containsKey("flags")){
          //sets control flags
          JsonObject flagObj = commandObj["flags"].as<JsonObject>();
          if(flagObj.containsKey("lockWhenIdle")){
            LockWhenIdle = flagObj["lockWhenIdle"].as<bool>();
          }
          if(flagObj.containsKey("restartWhenIdle")){
            RestartWhenIdle = flagObj["restartWhenIdle"].as<bool>();
          }
          //Respond with a config message, so the server knows we set flags;
          SendWSReconnect = 1;
        }
      }
    }

    //Check how long it has been since we heard a non-ping message from the websocket. If it has been a while, prompt the server by sending the startup messages again.
    if(LastFromWS + 15000 <= millis64()){
      SendInfoMessage = 1;
      LastFromWS = LastFromWS + 5000; //Give a 1 second delay so we don't spam this.
      if(DebugMode){
        Serial.println(F("Haven't heard from the server in a while, sending Config Info message to prompt it to do something."));
      }
    }

    //There are very few reasons we need to send a message. 
    //Check if each is the case:

    //Need to convert our current state to the style the WSACS API expects;
    String APIState = InternalStateToAPI(State);

    //0: We just (re)connected
    if(SendWSReconnect && !WSSend){

      JsonObject status = wsresp["status"].to<JsonObject>();
      status["currentCardTag"] = UID;
      JsonObject config = status["config"].to<JsonObject>();
      String VerToSend = "shlugduino V" + String(Version);
      config["firmware"] = VerToSend;
      JsonArray channels = config["channels"].to<JsonArray>();
      JsonObject channelsObject = channels.createNestedObject();
      channelsObject["id"] = 0;
      channelsObject["tempDuration"] = 0;
      config["inputMode"] = "INSERT";
      JsonObject deployment = config["deployment"].to<JsonObject>();
      deployment["SN"] = SerialNumber;
      JsonArray components = deployment["components"].to<JsonArray>();
      //Need to iterate through each connected device on the bus, and add at as a nested object within the components array
      //Each object should have the key "SN" for the OneWire address, "type" for the type. Type has to be a string of the type, we can set it to always be "SWITCH_1_CHANNEL" by sending 0b001
      //First, let's make sure that the buffer of addresses is up to date;
      AddressBufferValid = 0;
      UpdateAddressBuffer = 1;
      while(!AddressBufferValid){
        delay(1);
      }
      //This does suck since it means that it will wait a few seconds for the next temperature loop to iterate.
      //The address buffer has been updated if we make it here.
      for (int i = 0; i < liveAddressCount; i++) {
        JsonObject deviceObj = components.createNestedObject();
        // Convert the 8-byte address to a Hex String for JSON
        char addrStr[17]; 
        snprintf(addrStr, sizeof(addrStr), "%02X%02X%02X%02X%02X%02X%02X%02X",
          liveAddresses[i][0], liveAddresses[i][1], liveAddresses[i][2], liveAddresses[i][3],
          liveAddresses[i][4], liveAddresses[i][5], liveAddresses[i][6], liveAddresses[i][7]);
        deviceObj["SN"] = String(addrStr);
        for(int j = 0; j < deviceCount; j++) {
          if(memcmp(liveAddresses[i], sensorList[j].address, 8) == 0) {
            // Here we grab the deviceType and other data from the struct
            deviceObj["type"] = sensorList[j].deviceType; 
            deviceObj["identifier"] = sensorList[j].classification; //Server doesn't expect this yet, but we should send it
            break;
          }
        }
      }
      //Now we need to tell the server the state of all flags we support;
      //This is sent as part of the "config" object, 
      JsonObject flags = config["flags"].to<JsonObject>();
      flags["lockWhenIdle"] = LockWhenIdle;
      flags["restartWhenIdle"] = RestartWhenIdle;
      WSSend = 1;
      SendWSReconnect = 0;
    }

    //0.5: We just connected but the scrummers can't figure out how to process multiple requests, so we send a second message right away. They claim it was an "intentional decision".
    if(SendInfoMessage && !WSSend){
      JsonObject info = wsresp["info"].to<JsonObject>();
      JsonArray fields = info["fields"].to<JsonArray>();
      fields.add("TIME");
      fields.add("STATE");
      WSSend = 1;
      SendInfoMessage = 0;
    }

    //1: Authenticating an ID
    if(VerifyID && !WSSend){
      JsonObject authTo = wsresp["authTo"].to<JsonObject>();
      authTo["state"] = "UNLOCKED";
      authTo["cardTagID"] = UID;
      WSSend = 1;
      VerifyID = 0;
    }
    //2: There's an error message to send
    if(ReadyToSend && !WSSend){
      JsonObject message = wsresp["message"].to<JsonObject>();
      JsonObject content = message["content"].to<JsonObject>();
      content["message"] = Message;
      message["auditLog"] = true; 
      WSSend = 1;
      ReadyToSend = 0;
      WritingMessage = 0;
    } else if (LogReadyToSend && !WSSend){
      JsonObject message = wsresp["message"].to<JsonObject>();
      JsonObject content = message["content"].to<JsonObject>();
      content[LogKey] = LogValue;
      message["auditLog"] = false;
      WSSend = 1;
      LogReadyToSend = 0;
      WritingMessage = 0;
    }
    
    //3: The state changed
    if(ChangeStatus && !WSSend){
      JsonObject status = wsresp["status"].to<JsonObject>();
      JsonObject stateChange = status["stateChange"].to<JsonObject>();
      JsonArray channels = stateChange["channels"].to<JsonArray>();
      JsonObject authObj = channels.createNestedObject();
      authObj["fromState"] = APIOldState;
      authObj["toState"] = APIState;
      authObj["reason"] = StateSource;
      authObj["channelID"] = 0;
      WSSend = 1;
      ChangeStatus = 0;
    }
    //4: It is time for a regular report
    if(RegularStatus && !WSSend){
      if(State != "Startup"){
        //We skip this if we are in startup state still.
        JsonObject status = wsresp["status"].to<JsonObject>();
        JsonObject regular = status["regular"].to<JsonObject>();
        JsonArray currentStates = regular["currentStates"].to<JsonArray>();
        JsonObject statusObject = currentStates.createNestedObject();
        statusObject["state"] = APIState;
        statusObject["channelID"] = 0;
        status["currentCardTag"] = UID;
        WSSend = 1;
        RegularStatus = 0;
      } else{
        RegularStatus = 0;
        if(DebugMode){
          Serial.println(F("Skipped regular report becuase we are in startup mode."));
        }
      }
    }
    delay(1);
    if(WSSend){
      //There is something to send
      String WSToSend = "";
      serializeJson(wsresp, WSToSend);
      //TODO: We sometimes send a null payload. This should fix it but it doesn't. If we fix this it may make startup time faster.
      if(WSToSend.length() > 1){
        wsresp.clear(); //Wipe the JSON for the next write session
        if(DebugMode){
          Serial.print(F("Sending Websocket Message:"));
          Serial.println(WSToSend);
        }
        //This seems to be causing the crash?
        //Is it because we are sending before connected?
        if(socket.isConnected()){
          //If we don't have a connection, we don't want to send
          SocketText = WSToSend;
          if(DebugMode){
            Serial.println(F("Sent."));
          }
          WSSend = 0;
        } else{
          if(DebugMode){
            Serial.println(F("Skipped send because of no connection."));
          }
        }
      } else{
        if(DebugMode){
          Serial.println(F("Ignoring send of a null payload..."));
        }
      }
    }
    //Check if it has been long enough to send a ping to the server.
    if((millis64() >= NextPing) && socket.isConnected() && !PingPending){
      PingTimeout = millis64() + 2000;
      PingPending = 1;
      SendPing = 1;
      if(DebugMode){
        Serial.println(F("Ping..."));
      }
    }
    //Check if we have waited to long to get a ping response.
    if(PingPending && millis64() >= PingTimeout){
      if(DebugMode){
        Serial.println(F("Did not get a ping in time! Websocket issue?"));
      }
      if(!SecondPing){
        SecondPing = 1;
        PingPending = 0;
      } else{
        PingPending = 0;
        SecondPing = 0;
        //Message = "Missed 2 pings. Can you hear me?";
        //ReadyToSend = 1;
        NoNetwork = 1;
        if(DebugMode){
          Serial.println(F("Missed second websocket ping. Attempting to reconnect..."));
        }
      }
    }
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
      SendInfoMessage = 1;
      NoNetwork = 0;
      NextPing = millis64() + 2000;
      break;
    case WStype_TEXT:
      WSIncoming = String((char *)payload, length);
      NewFromWS = 1;
      NextPing = millis64() + 2000;
      NoNetwork = 0;
      break;
    case WStype_PONG:
      PingPending = 0;
      SecondPing = 0;
      NoNetwork = 0;
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
  if(ConnectRandomMax > 0){
    long WebsocketDelay = random(1, ConnectRandomMax);
    if(DebugMode){
      Serial.print(F("Delaying connect to server by "));
      Serial.print(WebsocketDelay);
      Serial.println(F(" milliseconds to prevent overburdening server."));
    }
    delay(WebsocketDelay);
  }

  //New API requires some info in the header of the Websocket starting:
  String ExtraHeader = "device-sn:" + SerialNumber + "\r\ndevice-key:" + Key;
  socket.setExtraHeaders(ExtraHeader.c_str());
  //socket.begin(Server.c_str(), 80, "/api/devices/cores/access/ws");
  socket.beginSslWithCA(Server.c_str(), 443, "/api/devices/cores/access/ws", root_ca);

  if(DebugMode){
    Serial.print(F("Connecting to websocket URL: "));
    Serial.println(Server);
  }
  socket.onEvent(webSocketEvent);
  socket.setReconnectInterval(2000); //Attempt to reconnect every 2 seconds if we lose connection
}

String InternalStateToAPI(String incoming){
  //This function converts our internal state to the API-compliant one.
    if(incoming == "Idle"){
      return "IDLE";
    }
    if(incoming == "Unlocked"){
      return "UNLOCKED";
    }
    if(incoming == "AlwaysOn"){
      return "ALWAYS_ON";
    }
    if(incoming == "Lockout"){
      return "LOCKED_OUT";
    }
    if(incoming == "Fault" || State == "TemperatureFault"){
      return "FAULT";
    }
    if(incoming == "Startup"){
      return "UNKNOWN";
    }
}

String APIStateToInternal(String incoming){
  //This function converts the API-compliant state to an internal one.
  if(incoming == "IDLE"){
    return "Idle";
  }
  if(incoming == "UNLOCKED"){
    return "Unlocked";
  }
  if(incoming == "ALWAYS_ON"){
    return "AlwaysOn";
  }
  if(incoming == "LOCKED_OUT"){
    return "Lockout";
  }
  if(incoming == "FAULT"){
    return "Fault";
  }
  if(incoming == "UNKNONW"){
    //If we don't know what we should be, let's just go to the current state
    return State;
  }
}