/*

Network Check

This task checks the health of the network connection periodically. 

Global Variables Used:

*/

void NetworkCheck(void *pvParameters) {
  while (1) {
    //Periodically check
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    if(CheckNetwork){
      //Network issue reported
      Serial.println(F("Checking network."));
      xSemaphoreTake(NetworkMutex, portMAX_DELAY);
      //Step 1: Are we connected to WiFi? 
      if(WiFi.status() != WL_CONNECTED){
        NoNetwork = 1;
        if(DebugMode){
          Serial.println(F("No WiFi connection!"));
        }
        //Check if the network is available even;
        int count = WiFi.scanNetworks();
        if(count == 0){
          if(DebugMode){
            Serial.println(F("No WiFi networks available!"));
          }
          NoNetwork = 1;
        } 
        else{
          for (int i = 0; i < count; i++){
            if(WiFi.SSID(i) == SSID){
              //The network is here, but we are not connected.
              if(DebugMode){
                Serial.println(F("Attempting to reconnect to WiFi"));
              }
              //Wireless Initialization:
              if (Password != "null") {
                WiFi.mode(WIFI_STA);
                WiFi.begin(SSID, Password);
              } else {
                if (DebugMode) {
                  Serial.println(F("Using no password."));
                }
                WiFi.begin(SSID);
              }
              WiFi.setSleep(false);
              WiFi.setAutoReconnect(true);
            }
          }
        }
        if(WiFi.status() != WL_CONNECTED){
          //If we make it here, we didn't find or reconnect to the network.
          if(DebugMode){
            Serial.println(F("Couldn't find network."));
            NoNetwork = 1;
            xSemaphoreGive(NetworkMutex);
            continue;
          }
        }
      }
      //Step 2: Are we connected to the web?
      String ServerPath = "https://example.com";
      http.begin(client, ServerPath);
      int httpResponseCode = http.GET();
      http.end();
      xSemaphoreGive(NetworkMutex);
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        CheckNetwork = 0;
        //So the network works.
        //Step 3: Is it our specific server maybe? 
        ServerPath = Server + "/api/check/" + MachineID;
        xSemaphoreTake(NetworkMutex, portMAX_DELAY);
        http.begin(client, ServerPath);
        httpResponseCode = http.GET();
        http.end();
        xSemaphoreGive(NetworkMutex);
        if(httpResponseCode != 200){
          //Unable to reach server. Not a network or hardware issue though.
          //So we still act like there's no network, but don't bother trying to reconnect or restart or anything.
          NoNetwork = 1;
        } else{
          //We were able to connect fine 
          NoNetwork = 0;
        }
      }
      else {
        Serial.print("Error code: ");
        Serial.println(http.errorToString(httpResponseCode));
        //We keep getting a -1 refuse to connect here. What's the deal?
        Serial.println(F("Failed repeatedly to connect to network with an invalid response"));
        NoNetwork = 1;
        if(State != "Unlocked" && State != "AlwaysOn"){
          //We are not in a running state
          Serial.println(F("Since we are not doing anything right now, going to restart the machine."));
          settings.putString("LastState", State);
          xSemaphoreTake(NetworkMutex, portMAX_DELAY);
          State = "Lockout";
          delay(5000);
          ESP.restart();
        } else{
          Serial.println(F("Deferring restart since the machine is in use."));
          delay(20000);
        }
      }
    }
  }
}