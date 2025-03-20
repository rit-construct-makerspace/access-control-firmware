/*

  NetworkCheck handles network-related failures

*/

void NetworkCheck(void *pvParameters){
  while(1){
    delay(1000);
    NoNetwork = 0;
    if(NetworkError >= 2 || (NetworkTime + 30000) <= millis64()){
      NoNetwork = 1;
      //We aren't doing anything and haven't sent a message to the server in over 30 seconds, let's make sure we're still connected.
      String ServerPath = Server + "/api/check";
      http.begin(client, ServerPath);
      int httpCode = http.GET();
      USBSerial.println(F("Checking server..."));
      USBSerial.print(F("Got response: ")); USBSerial.println(httpCode);
      if(httpCode == 404){
        //Good response
        USBSerial.println(F("Still connected to network."));
        NetworkError = 0;
      } else{
        USBSerial.println(F("Server check failed?"));
        NetworkError++;
        if(NetworkError > 3){
          USBSerial.println(F("We've had too many server errors. Restarting."));
          vTaskSuspendAll();
          NoNetwork = 1;
          delay(5000);
          ESP.restart();
        }
      }
    }
  }
}