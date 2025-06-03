/*

--- Network Manager ---

This code is responsible for checking we have a network connection, and reconnecting on drop.

In the future, it will handle the ethernet as well.

*/

void NetworkManager(void *pvParameters){
  //First, determine what connections we should be using;
  if((NetworkMode == 0) || (NetworkMode == 2)){
    UseWiFi = 1;
  }
  if((NetworkMode == 1) || (NetworkMode == 2)){
    UseEthernet = 1;
  }
  //Next, connect to the network
  if(!NetworkConnect()){
    if(DebugMode){
      Serial.println(F("Failed first connection to network!"));
    }
    //This is eventually where code goes that has to do with first network connection failure.
  } 
  else{
    //Temporarily use NoNetwork to indicate a connection to the network for setup to continue
    //This variable normally means a connection to the socket
    NoNetwork = 0;
  }
  while(1){
    delay(5000);
    //Constantly run through the following steps;
    //First, check if we are connected:
    if(!InterfaceUsed){
      //Supposed to be on WiFi
      if(WiFi.status() != WL_CONNECTED){
        //We are not connected to the network, attempt to reconnect
        NetworkConnect();
      }
    }
    else{
      //Check ethernet
      //Not implemented
      delay(1);
    }
    //Next, check if we should swap interfaces:
    if(UseEthernet && UseWiFi && !InterfaceUsed){
      //We are allowed to use both interfaces, but using WiFi now.
      //Check if we can swap to ethernet (preferred if available)
      //TODO implement with ethernet
      delay(1);
    }
    //Then, check if we still have a network issue
    if(NoNetwork){
      //If our network is actually connected, this may be a sign of a server-specific outage 
      if(DebugMode){
        Serial.println(F("Socket Manager reports no network connection."));
      }
      if(TestNetwork()){
        if(DebugMode){
          Serial.println(F("Internet connection appears OK."));
          Serial.println(F("Will try restarting websocket."));
        }
        socket.disconnect();
        StartWebsocket();
      } 
      else{
        if(DebugMode){
          Serial.println(F("Was not able to reach the internet."));
        }
      }
    }
  }
}

bool NetworkConnect(){
  //Connects to the network
  //Return 0 if connection failed
  if(DebugMode){
    Serial.println(F("Connecting to network..."));
  }
  if(UseEthernet){
    if(DebugMode){
      Serial.println(F("Starting ethernet connection..."));
    }
    Serial.println(F("Ethernet not implemented. Falling back to WiFi."));
    //When implemented, on success it'd return so we skip the wifi code, or on fail fall through to WiFi.
    //if(TestNetwork()){
      //InterfaceUsed = 1;
      //return 1;
    //}
  }
  if(UseWiFi){
    if(DebugMode){
      Serial.println(F("Starting WiFi connection..."));
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, Password);
    byte WiFiCount = 0;
    while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      WiFiCount++;
      if(WiFiCount >= 10){
        //If we haven't connected in 10 tries, never will
        if(DebugMode){
          Serial.println(F("Failed to connect to network - bad SSID / password?"));
        }
        break;
      }
      delay(1000);
    }
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    if(DebugMode){
      Serial.println(F("WiFi reporting connected, testing if we can reach the internet."));
    }
    if(TestNetwork()){
      InterfaceUsed = 0;
      return 1;
    }
  }
  //If we made it here, we failed to connect to the network
  if(DebugMode){
    Serial.println(F("Failed to connect to network."));
  }
  return 0;
}

bool TestNetwork(){
  //Tests the network connection is working by pinging rit.edu
  if(Ping.ping("www.rit.edu")){
    //Ping Success
    if(DebugMode){
      Serial.println(F("Pinged www.rit.edu successfully. Network connection to internet OK."));
    }
    return 1;
  } else{
    if(DebugMode){
      Serial.println(F("Failed to ping www.rit.edu. If we have a network connection, probably means we can't reach the internet."));
    }
    return 0;
  }
}