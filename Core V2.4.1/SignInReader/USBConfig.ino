/*

USB Config

Eventually, this will have code for connection to a program on the computer to handle setup and configuration of the system.
For now, it handles a basic JSON-based update system.

If you paste a JSON document into the USBSerial monitor, containing the parameter "oldPassword" that matches the internal stored password, the settings you upload will be applied

The following parameters are accepted:
* oldPassword---Must match the internal securityCode for the JSON to be accepted.
* ssid----------WiFi ssid to attempt to connect to.
* password------Wifi password to attempt to connect with.
* server--------URL to send API calls to.
* key-----------API key.
* zone----------Identifier code for the room the machine is located in, for sign-in reasons.


Global Variables Used:
DebugPrinting used to determine if UART output is free and to use and reserves it
*/

void USBConfig(void *pvParameters){
  if(securityCode == NULL){
    USBSerial.println(F("ERROR: NO CONFIG?"));
  }

  while (1) {
    //Check once a second;
    if(USBSerial.available() > 10){
      //There is a message of substance in the USBSerial buffer
      USBSerial.setTimeout(100); // TODO: Check with Jim. This was originally 3 ms which might be too short for UART
      String USBInput = USBSerial.readString();
      USBInput.trim();
      
      DeserializationError error = deserializeJson(usbjson, USBInput);
      if (error) {
        USBSerial.print(F("Deserialization Error: "));
        USBSerial.println(error.c_str());
        USBSerial.flush();
        vTaskDelay(1000 / portTICK_PERIOD_Ms);
        continue;
      }
      
      String oldPassword = usbjson["OldPassword"];
      securityCode = settings.getString("securityCode");  // Make sure we have the most up-to-date code to be safe

      if(oldPassword.equals(securityCode) || (securityCode == NULL)) {
        // passwords match or there is no password. Load the JSON info
        if (usbjson["Newpassword"]) {
            const char* Temp = usbjson["Newpassword"];
            settings.putString("securityCode", Temp);
            USBSerial.print(F("Updated password to:"));
            USBSerial.println(Temp);
        }
        
        UpdateSetting("ssid");
        UpdateSetting("password");
        UpdateSetting("server");
        UpdateSetting("key");
        UpdateSetting("zone");
        UpdateSetting("noBuzzer");
        UpdateSetting("brightness");
        UpdateSetting("nfcValidLength");
        UpdateSetting("nfcValidSAK");
        UpdateSetting("nfcValidREQA");

        //Restart to apply settings.
        USBSerial.println(F("Settings Applied."));   
        USBSerial.println(F("Rebooting in 5 seconds..."));
        USBSerial.flush();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        ESP.restart();
      } else{
        //Bad input, disregard.
        USBSerial.println(F("Bad Input"));
        USBSerial.flush();
      }
    }
    //Wait another 10 seconds before checking again.
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

void UpdateSetting(String key) { 
  //Updates a value in the preferences with a value from a JSON document.
  const char* Temp = usbjson[key];
  const char* keyArray = key.c_str();
  if (!Temp) {
    return;
  }
  USBSerial.print(F("Updating key "));
  USBSerial.print(keyArray);
  USBSerial.print(F(" with value "));
  USBSerial.println(Temp);
  //key is present
  settings.putString(key.c_str(), Temp);
}