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

*/

void InternalRead(void *pvParameters){
  while(1){
    if(Internal.available()){
      //There is a new message incoming.
      String incoming = Internal.readStringUntil('\n');
      incoming.trim();
      if(DebugMode && (xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE)){
        Debug.print("Received ");
        Debug.print(incoming);
        Debug.println(F(" from the frontend."));
        xSemaphoreGive(DebugMutex);
      }
      if(incoming.charAt(0) == 'B'){
        //Button state
        if(incoming.charAt(2) == '1'){
          Button = 1;
        } else{
          Button = 0;
        }
        if(DebugMode && xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
          Debug.print(F("Button state changed to "));
          Debug.println(Button);
          xSemaphoreGive(DebugMutex);
        }
        continue;
      }
      if(incoming.charAt(0) == 'V'){
        //Version info
        FEVer = incoming.substring(2);
        if(DebugMode && xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
          Debug.print(F("Front Version Reported: ")); Debug.println(FEVer);
          xSemaphoreGive(DebugMutex);
        }
        continue;
      }
      bool state;
      if(incoming.charAt(3) == '1'){
        state = 1;
      } else{
        state = 0;
      }
      bool internalreserved = 0;
      if(DebugMode && xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
        internalreserved = 1;
        Debug.print("Setting state ");
        Debug.print(state);
        Debug.print(" on ");
      }
      if(incoming.substring(0, 2).equals("S1")){
        Switch1 = state;
        if (internalreserved) {
          Debug.println(F("Switch1"));
          xSemaphoreGive(DebugMutex);
        }
      }
      if (incoming.substring(0, 2).equals("S2")) {
        Switch2 = state;
        if (internalreserved) {
          Debug.println(F("Switch2"));
          xSemaphoreGive(DebugMutex);
        }
      }
      if (incoming.substring(0, 2).equals("K1")) {
        Key1 = state;
        if (internalreserved) {
          Debug.println(F("Key1"));
          xSemaphoreGive(DebugMutex);
        }
      }
      if (incoming.substring(0, 2).equals("K2")) {
        Key2 = state;
        if (internalreserved) {
          Debug.println(F("Key2"));
          xSemaphoreGive(DebugMutex);
        }
      }
    }
  }
}