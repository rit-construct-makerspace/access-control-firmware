/*

Internal Read

This task reads for updates from the frontend MCU and sets flags properly. 

The frontend MCU sends the following messages;
* B (0/1) - Sends 1 when the button is pressed, 0 when it is released.
* S1 (0/1) - Sends 1 when the first card detect is pressed, 0 when it is released.
* S2 (0/1) - Sends 1 when the second card detect is pressed, 0 when it is released.
* K1 (0/1) - Sends the state of the first key switch pin when it changes.
* K2 (0/1) - Sends the state of the second key swtich pin when it changes.
* V (Version) - Sends the version of the frontend firmware

Global Variables Used:
* Button - State of the frontend button.
* Switch1 - State of card detect switch 1
* Switch2 - state of card detect switch 2
* Key1 - state of keyswitch input 1
* Key2 - state of keyswitch input 2
* DebugMode - Determines if there should be a verbose output.

InternalReadDebug is a define that turns on or off debug messages in this task, since it does fill the terminal with a lot of crap.

*/

void InternalRead(void *pvParameters){
  while(1){
    //Check every 50mS for a new message
    vTaskDelay(50 / portTICK_PERIOD_MS);
    if(Internal.available()){
      //There is a new message incoming.
      String incoming = Internal.readStringUntil('\n');
      incoming.trim();
      if(incoming.charAt(0) == 'B'){
        //Button state
        if(incoming.charAt(2) == '1'){
          //inverted logic, button press to ground.
          Button = 0;
        } else{
          Button = 1;
        }
        if(DebugMode && InternalReadDebug){
          if(xSemaphoreTake(DebugMutex,portMAX_DELAY) == pdTRUE){
            Serial.print(F("Button state changed to "));
            Serial.println(Button);
            xSemaphoreGive(DebugMutex);
          }
        }
        continue;
      }
      if(incoming.charAt(0) == 'V'){
        //Version info
        FEVer = incoming.substring(2);
        if(DebugMode && InternalReadDebug){
          if(xSemaphoreTake(DebugMutex,portMAX_DELAY) == pdTRUE){
            Serial.print(F("Front Version Reported: ")); Serial.println(FEVer);
            xSemaphoreGive(DebugMutex);
          }
        }
        continue;
      }
      bool state;
      if(incoming.charAt(3) == '1'){
        //Inverted logic, button press is 0
        state = 0;
      } else{
        state = 1;
      }
      bool internalreserved = 0;
      if(DebugMode && InternalReadDebug){
        if(xSemaphoreTake(DebugMutex,portMAX_DELAY) == pdTRUE){
          internalreserved = 1;
          Serial.print("Setting state ");
          Serial.print(state);
          Serial.print(" on ");
        }
      }
      if(incoming.substring(0, 2).equals("S1")){
        Switch1 = state;
        if (internalreserved) {
          Serial.println(F("Switch1"));
          xSemaphoreGive(DebugMutex);
        }
      }
      if (incoming.substring(0, 2).equals("S2")) {
        Switch2 = state;
        if (internalreserved) {
          Serial.println(F("Switch2"));
          xSemaphoreGive(DebugMutex);
        }
      }
      if (incoming.substring(0, 2).equals("K1")) {
        Key1 = state;
        if (internalreserved) {
          Serial.println(F("Key1"));
          xSemaphoreGive(DebugMutex);
        }
      }
      if (incoming.substring(0, 2).equals("K2")) {
        Key2 = state;
        if (internalreserved) {
          Serial.println(F("Key2"));
          xSemaphoreGive(DebugMutex);
        }
      }
    }
  }
}