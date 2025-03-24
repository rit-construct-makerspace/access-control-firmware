/*

USB Config

Eventually, this will have code for connection to a program on the computer to handle setup and configuration of the system.
For now, it handles a basic JSON-based update system.

If you paste a JSON document into the USBSerial monitor, containing the parameter "OldPassword" that matches the internal stored password, the settings you upload will be applied

The following parameters are accepted:
* OldPassword---Must match the internal SecurityCode for the JSON to be accepted.
* SSID----------WiFi SSID to attempt to connect to.
* Password------Wifi password to attempt to connect with.
* Server--------URL to send API calls to.
* Key-----------API key.
* Zone----------Identifier code for the room the machine is located in, for sign-in reasons.


Global Variables Used:
DebugPrinting used to determine if UART output is free and to use and reserves it
*/

void USBConfig(void *pvParameters){
  if(SecurityCode == NULL){
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
      
      String OldPassword = usbjson["OldPassword"];
      SecurityCode = settings.getString("SecurityCode");  // Make sure we have the most up-to-date code to be safe

      if(OldPassword.equals(SecurityCode) || (SecurityCode == NULL)) {
        // Passwords match or there is no password. Load the JSON info
        if (usbjson["NewPassword"]) {
            const char* Temp = usbjson["NewPassword"];
            settings.putString("SecurityCode", Temp);
            USBSerial.print(F("Updated Password to:"));
            USBSerial.println(Temp);
        }
        
        UpdateSetting("SSID");
        UpdateSetting("Password");
        UpdateSetting("Server");
        UpdateSetting("Key");
        UpdateSetting("Zone");
        UpdateSetting("NoBuzzer");
        UpdateSetting("Brightness");
        UpdateSetting("ValidLength");
        UpdateSetting("ValidSAK");
        UpdateSetting("ValidREQA");

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

void UpdateSetting(String Key) { 
  //Updates a value in the preferences with a value from a JSON document.
  const char* Temp = usbjson[Key];
  const char* KeyArray = Key.c_str();
  if (!Temp) {
    return;
  }
  USBSerial.print(F("Updating key "));
  USBSerial.print(KeyArray);
  USBSerial.print(F(" with value "));
  USBSerial.println(Temp);
  //Key is present
  settings.putString(Key.c_str(), Temp);
}