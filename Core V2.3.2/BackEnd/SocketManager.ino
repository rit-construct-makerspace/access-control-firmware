/*

Socket Manager

This task is responsible for sending and receiving all network traffic via websockets.


*/

void SocketManager(void *pvParameters) {    
  JsonDocument wsresp;
  String WSServer = "wss://" + Server + "/api/ws";
  //TODO get from the actual WSServer
  socket.begin("calcarea.student.rit.edu", 3000, "/api/ws");
  //socket.beginSslWithBundle("10.42.0.1", 3000, "/", NULL, 0, "");
  socket.onEvent(webSocketEvent);
  socket.setReconnectInterval(1000); //Attempt to reconnect every second if we lose connection
  //Wait to connect before continuing
  while(!socket.isConnected()){
    delay(1);
  }
  Serial.println("Websocket Initial Connection.");
  while (1) {
    delay(50);
    //First, check if there is a new message:
    if(NewFromWS){
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
        if(kv.key() == "State"){
          //Immediately set the state of the machine to this
          String WSState = wsin["State"].as<String>();
          if(WSState == State){
            if(DebugMode){
              Serial.println(F("State change to same state. Ignoring."));
            }
          } else{
              if(WSState == "Restart"){
                //The system has ordered an immediate restart of the device.
                if(DebugMode){
                  Serial.println(F("Restart Ordered."));
                }
                xSemaphoreTake(StateMutex, portMAX_DELAY);
                settings.putString("LastState", State);
                delay(10);
                State = "Restart";
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
                if(WSState == "Fault"){
                  //Ordered to an irreversible fault state
                  Fault = 1;
                  State = "Fault";
                  Serial.println(F("FAULTING"));
                }
                //Some special fixings:
                //If the state is "Unlocked" the machine gets confused when we change state with a card still present.
                //So, act like the card was removed
                if(State == "Unlocked"){
                  CardVerified = 0;
                }
                State = WSState;
                if(DebugMode){
                  Serial.print(F("State remotely set to: "));
                  Serial.println(State);
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
        if(kv.key() == "Request"){
          //The server has requested some values from us.
          //The following parameters can be requested:
            //UID: The current user's UID (0 if no user)
            //Zone: The zone we are located in
            //NeedsWelcome: 1 if the machine requires sign-in before use
            //MachineName: The unique name of this machine
            //MachineType: The numerical identifer of the machine's type
            //Message: The last message we sent to the server (idk why you'd want this)
            //Key: The API key the Shlug is using currently
            //FWVersion: The firmware version of the device
            //HWVersion: The hardware revision of the device
            //HWType: The role the hardware is serving (Core in our case)
            //State: The current state of the system
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
            if(wsin["Request"][i] == "Zone"){
              wsresp["Zone"] = Zone;
            }
            if(wsin["Request"][i] == "NeedsWelcome"){
              wsresp["NeedsWelcome"] = NeedsWelcome;
            }
            if(wsin["Request"][i] == "MachineName"){
              wsresp["MachineName"] = MachineName;
            }
            if(wsin["Request"][i] == "MachineType"){
              wsresp["MachineType"] = MachineType;
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
      wsresp["Zone"] = Zone;
      wsresp["NeedsWelcome"] = NeedsWelcome;
      wsresp["MachineType"] = MachineType;
      wsresp["MachineName"] = MachineName;
      wsresp["Key"] = Key;
      wsresp["State"] = State;
      wsresp["FWVersion"] = Version;
      wsresp["HWVersion"] = Hardware;
      wsresp["HWType"] = "ACS Core";
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
      Serial.print(F("Acces Denied. Reason: "));
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
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      NoNetwork = 1;
      WSSend = 0; //Clear out anything that needs to be sent
      break;
    case WStype_CONNECTED:
      Serial.println(F("Connected."));
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      SendWSReconnect = 1;
      NoNetwork = 0;
      break;
    case WStype_TEXT:
      WSIncoming = String((char *)payload, length);
      if(DebugMode){
        Serial.print(F("Got Websocket Payload: "));
        Serial.println(WSIncoming);
      }
      NewFromWS = 1;
      break;
    case WStype_ERROR:
      Serial.println(F("Got a websocket error!"));
      break;
  }
}
