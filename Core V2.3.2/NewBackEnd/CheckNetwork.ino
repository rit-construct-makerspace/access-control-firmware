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
      Debug.println(F("Checking network."));
      xSemaphoreTake(NetworkMutex, portMAX_DELAY);
      String ServerPath = "https://example.com";
      http.begin(client, ServerPath);
      int httpResponseCode = http.GET();
      http.end();
      Debug.println(httpResponseCode);
      if (httpResponseCode>0) {
        Debug.print("HTTP Response code: ");
        Debug.println(httpResponseCode);
        String payload = http.getString();
        Debug.println(payload);
        CheckNetwork = 0;
      }
      else {
        Debug.print("Error code: ");
        Debug.println(http.errorToString(httpResponseCode));
        //We keep getting a -1 refuse to connect here. What's the deal?
        Debug.println(F("Failed repeatedly. Restarting device."));
        ESP.restart();
      }
    xSemaphoreGive(NetworkMutex);
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
  }
}