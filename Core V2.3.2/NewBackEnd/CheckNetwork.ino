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
      HTTPClient http;
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
        //Try restarting WiFi?
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        WiFi.mode(WIFI_STA);
        //Wireless Initialization:
        if (Password != "null") {
          WiFi.begin(SSID, Password);
        } else {
          if (DebugMode) {
            Debug.println(F("Using no password."));
          }
          WiFi.begin(SSID);
        }
      }
    xSemaphoreGive(NetworkMutex);
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
  }
}