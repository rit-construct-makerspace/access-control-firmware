/*

Network Check

This task checks the health of the network connection periodically. 

Global Variables Used:

*/

void NetworkCheck(void *pvParameters) {
  while (1) {
    //Periodically check
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    if(CheckNetwork || NoNetwork){
      CheckNetwork = 0;
      //Network issue reported
      Serial.println(F("Checking local network connection."));
      xSemaphoreTake(NetworkMutex, portMAX_DELAY);
      if(WiFi.status() != WL_CONNECTED){
        Serial.println(F("Already see no connection, I'll try to reconnect."));
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        WiFiConnect();
        delay(2000);
      }
      xSemaphoreGive(NetworkMutex);
      //Step 1: Are we connected to WiFi? 
      if(WiFi.status() != WL_CONNECTED){
        Serial.println(F("No WiFi connection."));
        NoNetwork = 1;
        //We know the issue, no need to continue check.
        continue;
      } else{
        Serial.println(F("Found WiFi network."));
      }
      //Step 2: Are we connected to the web?
      Serial.println(F("Checking internet connection."));
      String ServerPath = "https://www.rit.edu";
      http.begin(client, ServerPath);
      int httpResponseCode = http.GET();
      http.end();
      xSemaphoreGive(NetworkMutex);
      if(httpResponseCode == 200){
        Serial.println(F("Got expected 200 response from rit.edu."));
        Serial.println(F("No issue with internet connection."));
      }
      else if (httpResponseCode>0){
      Serial.print("Got unexpected HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println(F("Probably an internet connectivity issue?"));
      continue;
      } 
      else{        
        Serial.print(F("Got concerning HTTP response code: "));
        Serial.println(httpResponseCode);
        Serial.print(F("Which means: "));
        Serial.println(http.errorToString(httpResponseCode));
        NetworkReboot();
      }
      //Step 3: Is it our specific server maybe? 
      Serial.println(F("Checking our server."));
      ServerPath = Server + "/api/check/" + MachineID;
      xSemaphoreTake(NetworkMutex, portMAX_DELAY);
      http.begin(client, ServerPath);
      httpResponseCode = http.GET();
      http.end();
      xSemaphoreGive(NetworkMutex);
      if(httpResponseCode != 200){
        //Unable to reach server. Not a network or hardware issue though.
        //So we still act like there's no network, but don't bother trying to reconnect or restart or anything.
        Serial.println(F("Did not get expected response code."));
        Serial.println(F("Probably an issue with our server."));
        NoNetwork = 1;
      } else{
        //We were able to connect fine 
        NoNetwork = 0;
        Serial.println(F("Seemed to connect fine. No network problems found."));
      }
    }
  }
}

void NetworkReboot(){
  //Reboots the device due to a network issue.
  Serial.println(F("Found a network issue that likely means there's a hardware fault."));
  if(State != "Unlocked" && State != "AlwaysOn"){
  //We are not in a running state
  Serial.println(F("Since we are not doing anything right now, going to restart the machine."));
  settings.putString("LastState", State);
  State = "Lockout";
  delay(5000);
  ESP.restart();
  } else{
    Serial.println(F("Deferring restart since the machine is in use."));
    Serial.println(F("Will check everything again in 10 seconds."));
    delay(10000);
  }
}