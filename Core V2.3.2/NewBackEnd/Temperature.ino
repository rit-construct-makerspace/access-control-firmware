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
	
	//First time running, find the addresses
  xSemaphoreTake(OneWireMutex, portMAX_DELAY); 
	uint8_t devices = ds.search(SerialNumbers, MAX_DEVICES);
	for (uint8_t i = 0; i < devices; i += 1) {
    if(DebugMode && xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
      Debug.printf("%d: 0x%llx,\n", i, SerialNumbers[i]);
      xSemaphoreGive(DebugMutex);
    }
	}

  //Infinite loop, measure all temperatures
	while(1){
    //Reserve the OneWire bus
    xSemaphoreTake(OneWireMutex, portMAX_DELAY); 
    float currTemp[MAX_DEVICES]; //Array to store all temperatures
    SysMaxTemp = 0;
		ds.request();
		vTaskDelay(750 / portTICK_PERIOD_MS); //Give some time for measurement and conversion
		for(byte i = 0; i < MAX_DEVICES; i++){ 
			uint8_t err = ds.getTemp(SerialNumbers[i], currTemp[i]);
			if(err){
				const char *errt[] = {"", "CRC", "BAD","DC","DRV"};
        if(DebugMode && xSemaphoreTake(DebugMutex,(50/portTICK_PERIOD_MS)) == pdTRUE){
          Debug.print(i); Debug.print(": "); Debug.println(errt[err]);
          xSemaphoreGive(DebugMutex);
        }
			}else{
        if(currTemp[i] > SysMaxTemp){
          SysMaxTemp = currTemp[i];
        }
        if(DebugMode && xSemaphoreTake(DebugMutex,(50/portTICK_PERIOD_MS)) == pdTRUE){
          Debug.print("Temperature Sensor "); Debug.print(i); Debug.print(": "); Debug.println(currTemp[i]);
          xSemaphoreGive(DebugMutex);
        }
			}
		}
    TemperatureUpdate = 0;
    if(DebugMode && xSemaphoreTake(DebugMutex,(50/portTICK_PERIOD_MS)) == pdTRUE){
      Debug.print(F("Maximum Temp: ")); Debug.println(SysMaxTemp);
      xSemaphoreGive(DebugMutex);
    }
    if(SysMaxTemp > TempLimit){
      TemperatureFault = 1;
      TemperatureStatus = 1;
      xSemaphoreTake(DebugMutex, portMAX_DELAY); 
      Debug.println(F("CRITICAL ERROR: OVERTEMPERATURE FAULT"));
      xSemaphoreGive(DebugMutex);
    }
    if(((SysMaxTemp + 5.0) <= TempLimit) && TemperatureFault == 1){
      TemperatureFault = 0;
      if(DebugMode && xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
        Debug.println(F("Overtemperature Error Cleared"));
        xSemaphoreGive(DebugMutex);
      }
    }
    //Release the OneWire bus;
    xSemaphoreGive(OneWireMutex);
		vTaskDelay(TemperatureTime / portTICK_PERIOD_MS);
	}
}