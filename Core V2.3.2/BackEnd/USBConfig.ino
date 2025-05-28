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
      //This is the start of a JSON with settings
      Input.trim();
      deserializeJson(usbjson, Input);
      //Apply all of the settings
      //We always overwrite the server and key on any change
      Key = usbjson["Key"].as<String>();
      Server = usbjson["Server"].as<String>();
      //Update the rest if found
      if(usbjson["SSID"]){
        SSID = usbjson["SSID"].as<String>();
      }
      if(usbjson["Password"]){
        Password = usbjson["Password"].as<String>();
      }
    }
    if(Input.equalsIgnoreCase("Restart")){
      //Restart command
      Serial.println(F("Restarting."));
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

/*
void USBConfig(void *pvParameters){
  byte BadInputCount = 0; //Tracks how many times an incorrect password/JSON was input.

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
      delay(50);
      if(TXINTERRUPT){
        //TX is being rerouted to the interrupt pin, need to route back to USB.
        Serial.end();
        Serial.begin(115200);
      }
      USBInput.trim();
      deserializeJson(usbjson, USBInput);
      //Check if the passwords match
      String OldPassword = usbjson["OldPassword"];
      SecurityCode = settings.getString("SecurityCode"); //Make sure we have the most up-to-date code to be safe
      if(OldPassword.equals(SecurityCode) || (SecurityCode == NULL)){
        //Passwords match or there is no password. Load the JSON info;
        BadInputCount = 0;
        if(usbjson["NewPassword"]){
          //New password present, update that...
          const char* Temp = usbjson["NewPassword"];
          settings.putString("SecurityCode", Temp);
          xSemaphoreTake(DebugMutex, portMAX_DELAY);
          Serial.print(F("Updated Password to:"));
          Serial.println(Temp);
          xSemaphoreGive(DebugMutex);
        }
        if(usbjson["DumpAll"]){
          //Dump all settings
          usbjson.clear(); //Wipe whatever's in the json
          usbjson["WiFi MAC"] = ESP.getEfuseMac();
          WriteSetting("SSID");
          WriteSetting("Password");
          WriteSetting("Server");
          if(DumpKey){
            WriteSetting("Key");
          } else{
            usbjson["Key"] = "super-secret";
          }
          WriteSetting("MachineName");
          WriteSetting("MachineType");
          WriteSetting("SwitchType");
          WriteSetting("Zone");
          WriteSetting("NeedsWelcome");
          WriteSetting("TempLimit");
          WriteSetting("NetworkMode");
          WriteSetting("Frequency");
          WriteSetting("DebugMode");
          WriteSetting("NoBuzzer");
          String ToSend;
          serializeJson(usbjson, ToSend);
          xSemaphoreTake(DebugMutex, portMAX_DELAY);
          Serial.println(ToSend);
          Serial.flush();
          xSemaphoreGive(DebugMutex);
          continue;
        }
        UpdateSetting("SSID");
        UpdateSetting("Password");
        UpdateSetting("Server");
        UpdateSetting("Key");
        UpdateSetting("MachineType");
        UpdateSetting("MachineName");
        UpdateSetting("SwitchType");
        UpdateSetting("Zone");
        UpdateSetting("NeedsWelcome");
        UpdateSetting("TempLimit");
        UpdateSetting("NetworkMode");
        UpdateSetting("Frequency");
        UpdateSetting("DebugMode");
        UpdateSetting("NoBuzzer");

        //Restart to apply settings.
        if(usbjson["DumpAll"]){
          //Since we just dumped info and didn't update, can skip restart
          continue;
        }
        xSemaphoreTake(DebugMutex, portMAX_DELAY);
        Serial.println(F("Settings Applied."));   
        Serial.println(F("Rebooting in 5 seconds..."));
        Serial.flush();
        xSemaphoreGive(DebugMutex);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        ESP.restart();
      } else{
        //Bad input, disregard.
        if(usbjson["Wipe"]){
          //Ordered to wipe data
          Serial.println(F("Forcing wipe of memory with bad inputs."));
          BadInputCount = BAD_INPUT_THRESHOLD;
        }
        BadInputCount++;
        xSemaphoreTake(DebugMutex, portMAX_DELAY);
        Serial.println(F("Bad Input"));
        Serial.print(BadInputCount);
        Serial.print(" / ");
        Serial.println(BAD_INPUT_THRESHOLD);
        Serial.flush();
        if(BadInputCount >= BAD_INPUT_THRESHOLD){
          Serial.println(F("TOO MANY BAD INPUTS. WIPING DATA."));
          nvs_flash_deinit();     // disable NVS flash.
          nvs_flash_erase();      // erase the NVS partition and...
          nvs_flash_init();       // initialize the NVS partition.
          Serial.println(F("Wipe Complete."));
          Serial.println(F("Restarting..."));
          Serial.flush();
          delay(100);
          ESP.restart();
        }
        xSemaphoreGive(DebugMutex);
      }
      if(TXINTERRUPT){
        //Need to set the TX back to the debug output.
        Serial.println(F("Rerouting UART to Interrupt."));
        Serial.flush();
        Serial.end();
        Serial.begin(115200, SERIAL_8N1, 44 , DB9INT);
        Serial.println(F("Serial rerouted back to Interrupt pin for logging (was rerouted for USBN Config)."));
        Serial.flush();
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

void WriteSetting(String Parameter){
  //Writes the specified parameter to the JSON "usbjson", for eventual printing.
  usbjson[Parameter] = settings.getString(Parameter.c_str());
}

*/