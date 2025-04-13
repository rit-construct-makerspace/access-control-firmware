/*
 
Temperature Monitoring

This process measures all temperature sensors in an assembly.

Global Variables Used:
TemperatureUpdate: 1 when writing new information, to indicate other systems shouldn't read temperature-related info
SerialNumbers: an array of uint64_t serial numbers of the devices. Size of the array is based on MAX_DEVICES 
SysMaxTemp: a float of the maximum temperature of the system
DebugMode: 1 when the system should be outputting debug data.
TemperatureTime: how long, in milliseconds, should be between system-wide temperature measurements

*/

void Temperature(void *pvParameters){
	OneWire32 ds(TEMP); //gpio pin
  bool TemperatureMessage;
	//First time running, find the addresses
  if(DebugMode){
    xSemaphoreTake(DebugMutex, portMAX_DELAY);
    Serial.println(F("First time running temperature..."));
    xSemaphoreGive(DebugMutex);
  }
  xSemaphoreTake(OneWireMutex, portMAX_DELAY); 
	uint8_t devices = ds.search(SerialNumbers, MAX_DEVICES);
	for (uint8_t i = 0; i < devices; i += 1) {
    if(DebugMode){
      if(xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
        Serial.printf("%d: 0x%llx,\n", i, SerialNumbers[i]);
        xSemaphoreGive(DebugMutex);
      }
    }
	}
  if(devices == 0){
    //Something has gone wrong, there should be at least 1 temperature sensor. Restart?
    Serial.println(F("ERROR: Found no OneWire devices?"));
    ESP.restart();
  }
  xSemaphoreGive(OneWireMutex);

  //Infinite loop, measure all temperatures
	while(1){
    //Reserve the OneWire bus
    xSemaphoreTake(OneWireMutex, portMAX_DELAY); 
    float currTemp[MAX_DEVICES]; //Array to store all temperatures
		ds.request();
		vTaskDelay(750 / portTICK_PERIOD_MS); //Give some time for measurement and conversion
		for(byte i = 0; i < MAX_DEVICES; i++){ 
			uint8_t err = ds.getTemp(SerialNumbers[i], currTemp[i]);
			if(err){
				const char *errt[] = {"", "CRC", "BAD","DC","DRV"};
        //We don't do anything with the errors but this is where we would if we cared.
			}else{
        if(currTemp[i] > SysMaxTemp){
          SysMaxTemp = currTemp[i];
        }
			}
		}
    TemperatureUpdate = 0;
    if(DebugMode){
      if(xSemaphoreTake(DebugMutex,(50/portTICK_PERIOD_MS)) == pdTRUE){
        Serial.print(F("Maximum Temp: ")); Serial.println(SysMaxTemp);
        xSemaphoreGive(DebugMutex);
      }
    }
    if(TempLimit == 0){
      continue;
    }
    if(SysMaxTemp > TempLimit){
      TemperatureFault = 1;
      TemperatureStatus = 1;
      xSemaphoreTake(DebugMutex, portMAX_DELAY); 
      Serial.println(F("CRITICAL ERROR: OVERTEMPERATURE FAULT"));
      xSemaphoreGive(DebugMutex);
      if(!TemperatureMessage){
        String TempSend = "INTERNAL_TEMPERATURE_OVERHEAT_AT_" + String(SysMaxTemp); + "C";
        ReadyMessage(TempSend);
        TemperatureMessage = 1;
      }
    }
    if(((SysMaxTemp + 5.0) <= TempLimit) && TemperatureFault == 1){
      TemperatureFault = 0;
      TemperatureMessage = 0;
      if(DebugMode){
        if(xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
          Serial.println(F("Overtemperature Error Cleared"));
          xSemaphoreGive(DebugMutex);
        }
      }
      ReadyMessage("INTERNAL_TEMPERATURE_CLEAR");
    }
    //Release the OneWire bus;
    xSemaphoreGive(OneWireMutex);
		vTaskDelay(TemperatureTime / portTICK_PERIOD_MS);
	}
}