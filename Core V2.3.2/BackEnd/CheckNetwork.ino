/*

Network Check

This task checks the health of the network connection periodically. 

Global Variables Used:

*/

void NetworkCheck(void *pvParameters) {
  while (1) {
    //Periodically check
    vTaskDelay(200 / portTICK_PERIOD_MS);
    if(CheckNetwork){
      //Network issue reported
      Serial.println(F("Checking network."));
      xSemaphoreTake(NetworkMutex, portMAX_DELAY);
      String ServerPath = "https://example.com";
      http.begin(client, ServerPath);
      int httpResponseCode = http.GET();
      http.end();
      xSemaphoreGive(NetworkMutex);
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
        CheckNetwork = 0;
        //So the network works, but not the website maybe? 
        ServerPath = Server + "/api/check/" + MachineID;
        xSemaphoreTake(NetworkMutex, portMAX_DELAY);
        http.begin(client, ServerPath);
        httpResponseCode = http.GET();
        http.end();
        xSemaphoreGive(NetworkMutex);
        if(httpResponseCode != 200){
          //Unable to reach server. Not a network or hardware issue though.
          NoNetwork = 1;
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
          State = "Lockout";
          delay(5000);
          ESP.restart();
        } else{
          Serial.println(F("Deferring restart since the machine is in use."));
          delay(20000);
        }
      }
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
  }
}