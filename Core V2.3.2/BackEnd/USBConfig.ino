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

//REWRITING to support websockets

void USBConfig(void *pvParameters){
  //This is the new USBConfig for websocket MVP
  while(1){
    delay(50);
    String Input = "";
    
    #ifdef WebsocketUART
      //We shouldn't be in this task if we are using UART for websocket testing
      continue;
    #endif

    if(Serial.available()){
      //There's something in the serial terminal.
      Serial.setTimeout(5);
      Input = Serial.readString();
      Input.trim();
    }
    if(Input == ""){
      //Empty string, return
      continue;
    }
    if(DebugMode){
      Serial.println(F("Processing USB Input:"));
      Serial.println(Input);
    }
    if(Input.charAt(0) == '{'){
      if(DebugMode){
        Serial.println(F("This appears to be a configuration JSON."));
      }
      //This is the start of a JSON with settings
      deserializeJson(usbjson, Input);
      //Apply all of the settings
      //We always overwrite the server and key on any change
      Key = usbjson["Key"].as<String>();
      settings.putString("Key",Key);
      Server = usbjson["Server"].as<String>();
      settings.putString("Server",Server);
      //Update the rest if found
      if(usbjson["SSID"]){
        SSID = usbjson["SSID"].as<String>();
        settings.putString("SSID",SSID);
      }
      if(usbjson["Password"]){
        Password = usbjson["Password"].as<String>();
        settings.putString("Password",Password);
      }
    }
    if(Input.equalsIgnoreCase("Restart")){
      //Restart command
      Serial.println(F("Restarting."));
      settings.putString("ResetReason","USB-Commanded");
      delay(1000);
      ESP.restart();
    }
    if(Input.equalsIgnoreCase("DumpMAC")){
      //Print the MAC address
      Serial.println(F("WiFi Mac:"));
      Serial.println(ESP.getEfuseMac(), HEX);
      Serial.println(F("Formatted:"));
      Serial.println(WiFi.macAddress());
    }
    if(Input.equalsIgnoreCase("DumpSerial")){
      //Print the Serial Number
      Serial.println(F("Serial Number:"));
      Serial.println(SerialNumber);
    }
  }
}