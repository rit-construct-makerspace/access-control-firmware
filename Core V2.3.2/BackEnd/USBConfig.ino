/*

USB Config

Eventually, this will have code for connection to a program on the computer to handle setup and configuration of the system.
For now, it handles a basic JSON-based update system.

If you paste a JSON document into the serial monitor, containing the parameter "OldPassword" that matches the internal stored password, the settings you upload will be applied

The following parameters are accepted:
* OldPassword---Must match the internal SecurityCode for the JSON to be accepted.
* SSID----------WiFi SSID to attempt to connect to.
* Password------Wifi password to attempt to connect with.
* Server--------URL to send API calls to.
* Key-----------API key.
* MachineID-----Unique machine name/ID.
* MachineType---Type of machine.
* SwitchType----Expected switch type used for the analog switch type detection.
* Interface-----No longer used.
* Zone----------Identifier code for the room the machine is located in, for sign-in reasons.
* NeedsWelcome--1 if the machine requires a sign-in to the room it is in to be registered with the server.
* TempLimit-----Maximum temperature before the system shuts down and throws a temperature warning.
* NewPassword---Can be used to change the internal SecurityCode.
* NetworkMode---Sets if the system uses WiFi, Ethernet, or both. Not implemented yet.
* Frequency-----How often to send an update to the server, in seconds.
* DebugMode-----1 if verbose serial output should be enabled. WARNING: Will plain-text print sensitive information!

Global Variables Used:
DebugPrinting used to determine if UART output is free and to use and reserves it
DebugMode used to turn on verbose outputs
*/

void USBConfig(void *pvParameters){
    if(SecurityCode == NULL){
      Serial.println(F("ERROR: NO CONFIG?"));
    }
  while(1){
    //Check once a second;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    if(Serial.available() > 10){
      //There is a message of substance in the serial buffer
      Serial.setTimeout(3);
      String USBInput = Serial.readString();
      USBInput.trim();
      deserializeJson(usbjson, USBInput);
      //Check if the passwords match
      String OldPassword = usbjson["OldPassword"];
      SecurityCode = settings.getString("SecurityCode"); //Make sure we have the most up-to-date code to be safe
      if(OldPassword.equals(SecurityCode) || (SecurityCode == NULL)){
        //Passwords match or there is no password. Load the JSON info;
        if(usbjson["NewPassword"]){
          //New password present, update that...
          const char* Temp = usbjson["NewPassword"];
          settings.putString("SecurityCode", Temp);
          xSemaphoreTake(DebugMutex, portMAX_DELAY);
          Serial.print(F("Updated Password to:"));
          Serial.println(Temp);
          xSemaphoreGive(DebugMutex);
        }
        UpdateSetting("SSID");
        UpdateSetting("Password");
        UpdateSetting("Server");
        UpdateSetting("Key");
        UpdateSetting("MachineID");
        UpdateSetting("MachineType");
        UpdateSetting("SwitchType");
        UpdateSetting("Zone");
        UpdateSetting("NeedsWelcome");
        UpdateSetting("TempLimit");
        UpdateSetting("NetworkMode");
        UpdateSetting("Frequency");
        UpdateSetting("DebugMode");
        UpdateSetting("NoBuzzer");

        //Restart to apply settings.
        xSemaphoreTake(DebugMutex, portMAX_DELAY);
        Serial.println(F("Settings Applied."));   
        Serial.println(F("Rebooting in 5 seconds..."));
        Serial.flush();
        xSemaphoreGive(DebugMutex);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        ESP.restart();
      } else{
        //Bad input, disregard.
        xSemaphoreTake(DebugMutex, portMAX_DELAY);
        Serial.println(F("Bad Input"));
        Serial.flush();
        xSemaphoreGive(DebugMutex);
      }
    }
    //Wait another 10 seconds before checking again.
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

void UpdateSetting(String Key) { 
  //Updates a value in the preferences with a value from a JSON document.
  const char* Temp = usbjson[Key];
  const char* KeyArray = Key.c_str();
  if (!Temp) {
    return;
  }
  if(DebugMode){
    if(xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
      Serial.print(F("Updating key "));
      Serial.print(KeyArray);
      Serial.print(F(" with value "));
      Serial.println(Temp);
      xSemaphoreGive(DebugMutex);
    }
  }
  //Key is present
  settings.putString(Key.c_str(), Temp);
}