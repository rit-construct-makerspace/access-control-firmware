/*
 
Temperature Monitoring

This process measures all temperature sensors in an assembly.

Global Variables Used:
TemperatureUpdate: 1 when writing new information, to indicate other systems shouldn't read temperature-related info
SerialNumbers: an array of uint64_t serial numbers of the devices. Size of the array is based on MAX_DEVICES 
SysMaxTemp: a float of the maximum temperature of the system
DebugPrinting: 1 when the debug print output is being used
DebugMode: 1 when the system should be outputting debug data.
TemperatureTime: how long, in milliseconds, should be between system-wide temperature measurements

*/

void Temperature(void *pvParameters){
	OneWire32 ds(TEMP); //gpio pin
	
	//First time running, find the addresses
	uint8_t devices = ds.search(SerialNumbers, MAX_DEVICES);
	for (uint8_t i = 0; i < devices; i += 1) {
    if(DebugMode && !DebugPrinting){
      DebugPrinting = 1;
      Debug.printf("%d: 0x%llx,\n", i, SerialNumbers[i]);
      DebugPrinting = 0;
    }
	}

  //Infinite loop, measure all temperatures
	while(1){
    TemperatureUpdate = 1;
    float currTemp[MAX_DEVICES]; //Array to store all temperatures
    SysMaxTemp = 0;
		ds.request();
		vTaskDelay(750 / portTICK_PERIOD_MS); //Give some time for measurement and conversion
		for(byte i = 0; i < MAX_DEVICES; i++){ 
			uint8_t err = ds.getTemp(SerialNumbers[i], currTemp[i]);
			if(err){
				const char *errt[] = {"", "CRC", "BAD","DC","DRV"};
        if(DebugMode && !DebugPrinting){
          DebugPrinting = 1;
          Debug.print(i); Debug.print(": "); Debug.println(errt[err]);
          DebugPrinting = 0;
        }
			}else{
        if(currTemp[i] > SysMaxTemp){
          SysMaxTemp = currTemp[i];
        }
        if(DebugMode && !DebugPrinting){
          DebugPrinting = 1;
          Debug.print("Temperature Sensor "); Debug.print(i); Debug.print(": "); Debug.println(currTemp[i]);
          DebugPrinting = 0;
        }
			}
		}
    TemperatureUpdate = 0;
    if(DebugMode && !DebugPrinting){
      DebugPrinting = 1;
      Debug.print(F("Maximum Temp: ")); Debug.println(SysMaxTemp);
      DebugPrinting = 0;
    }
    if(SysMaxTemp > TempLimit){
      if(DebugMode && !DebugPrinting){
        DebugPrinting = 1;
        Debug.println(F("Overtemperature Error!"));
        DebugPrinting = 0;
      }
      TemperatureFault = 1;
      TemperatureStatus = 1;
    }
    if(((SysMaxTemp + 5.0) <= TempLimit) && TemperatureFault == 1){
      TemperatureFault = 0;
      if(DebugMode && !DebugPrinting){
        DebugPrinting = 1;
        Debug.println(F("Overtemperature Error Cleared"));
        DebugPrinting = 0;
      }
    }
		vTaskDelay(TemperatureTime / portTICK_PERIOD_MS);
	}
}