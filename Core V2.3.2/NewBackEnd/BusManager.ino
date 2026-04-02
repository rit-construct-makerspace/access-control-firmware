/*
The Bus Manager is responsible for everything OneWire related
  * Getting bus temperatures
  * Checking for bus integrity
*/

void BusManager(void *pvParameters){
  unsigned long long OneWireTime = 0;      //Next time we should check the bus.
  struct Device {
    byte address[8];
    byte deviceType;         
    byte highTempLimit;      
    uint32_t classification; 
    float currentTemp;       
    bool isAlarming;         
    bool isOnline;           
  };
  Device sensorList[5];
  while(1){
    //Step 1: Check if we are commanded to seal the bus;
    if(ReSealBus){
      ReSealBus = false;
      discoverDevices();
      saveInventoryToFile();
      SealBroken = false;
      //Set state lockout;
      State = "LOCKED_OUT";
      StateChangeReason = "LOCAL";
      //Immediately re-run the bus scan;
      OneWireTime = 0;
    }
    //Step 2: Check if it is time to check the bus
    if(OneWireTime <= millis64()){
      OneWireTime = millis64() + 10000; //Check again in 10 seconds
      //Step 2: Scan the bus for devices, make sure they are all there
      checkBusHealth();
      if(SealBroken){
        refreshLiveAddressBuffer();
      }
      //Step 3: Check for any overtemp devices
      updateBusTemperatures();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void loadInventoryFromFile() {
  //Gets the expected list of OneWire devices from memory.
  if (!SPIFFS.exists("/inventory.dat")) {
    Serial.println("No inventory file found. System is uninitialized.");
    deviceCount = 0;
    return;
  }

  File file = SPIFFS.open("/inventory.dat", FILE_READ);
  deviceCount = file.read(); // First byte is the count
  
  for (int i = 0; i < deviceCount; i++) {
    file.read(sensorList[i].address, 8);
    Serial.print("Loaded Address: ");
    printAddress(sensorList[i].address);
    Serial.println();
    // Fetch stored HighTemp from the physical chip now that we have the address
    byte data[9];
    if (readScratchpad(sensorList[i].address, data)) {
      sensorList[i].highTempLimit = data[2];
    }
  }

  file.close();
  Serial.print("Loaded "); Serial.print((int)deviceCount); Serial.println(" devices from SPIFFS.");
}

void discoverDevices() {
  //Discovers and categorizes all devices in a deployment.
  byte addr[8];
  deviceCount = 0;
  ds.reset_search();
  Serial.println("\n--- Scanning Bus ---");
  while (ds.search(addr) && deviceCount < 10) {
    if (OneWire::crc8(addr, 7) != addr[7]) continue;
    for (int i = 0; i < 8; i++) sensorList[deviceCount].address[i] = addr[i];
    
    sensorList[deviceCount].classification = fetchMetadata(addr, sensorList[deviceCount].deviceType, sensorList[deviceCount].highTempLimit);
    
    Serial.print("["); Serial.print(deviceCount); Serial.print("] ID: ");
    printAddress(sensorList[deviceCount].address);
    Serial.print(" | HighLimit: "); Serial.print(sensorList[deviceCount].highTempLimit);
    Serial.print(" | Class: "); Serial.println(sensorList[deviceCount].classification);
    deviceCount++;
  }
}

void saveInventoryToFile() {
  //Saves the current list of debices to SPIFFs, treating it as a valid deployment. 
  File file = SPIFFS.open("/inventory.dat", FILE_WRITE);
  if (!file) {
    Serial.println("Error: Could not create inventory file!");
    return;
  }

  // Record how many devices we are saving
  file.write((byte)deviceCount);
  
  // Save each 8-byte address
  for (int i = 0; i < deviceCount; i++) {
    file.write(sensorList[i].address, 8);
  }
  
  file.close();
  Serial.println("Inventory locked to SPIFFS.");
  SealBroken = false; //Reset the seal
}

void checkBusHealth() {
  byte addr[8];
  int liveCount = 0;
  bool currentMismatch = false;
  bool responded[10] = {false};

  // --- 1. Initial Full Bus Scan ---
  ds.reset_search();
  while (ds.search(addr)) {
    if (OneWire::crc8(addr, 7) != addr[7]) continue;

    bool foundInInventory = false;
    for (int i = 0; i < deviceCount; i++) {
      if (memcmp(addr, sensorList[i].address, 8) == 0) {
        foundInInventory = true;
        responded[i] = true;
        sensorList[i].isOnline = true;
        liveCount++;
        break;
      }
    }
    
    // Check for "New/Unexpected" devices (Intruders)
    if (!foundInInventory) {
      Serial.print("SECURITY ALERT: Unknown device detected: ");
      printAddress(addr);
      Serial.println();
      currentMismatch = true; 
    }
  }

  // --- 2. Verified Retry for Missing Devices ---
  for (int i = 0; i < deviceCount; i++) {
    if (!responded[i]) {
      // Use a targeted scratchpad read instead of a general bus reset.
      // If the device is missing, the bus floats high, CRC fails, and this returns false.
      byte dummyData[9];
      if (readScratchpad(sensorList[i].address, dummyData)) { 
        responded[i] = true;
        sensorList[i].isOnline = true;
        liveCount++;
      } else {
        // Still missing after targeted communication attempt
        Serial.print("SECURITY ALERT: Missing device: ");
        printAddress(sensorList[i].address);
        Serial.println();
        currentMismatch = true;
        sensorList[i].isOnline = false;
      }
    }
  }

  // --- 3. Final Count Comparison ---
  if (liveCount != deviceCount) {
    currentMismatch = true;
  }

  // --- 4. State Management & Terminal Fault Logic ---
  if (currentMismatch) {
    if (!SealBroken) {
      Serial.println("BUS SEAL RUPTURED!");
      SealBroken = true;
    }
  }
}

void updateBusTemperatures() {
  //Scans through the bus, gets the temperature of every device.
  ds.reset();
  ds.skip();
  ds.write(0x44); 
  delay(750); 

  //Start by assuming everything is fine. 
  OverTemp = false; 

  for (int i = 0; i < deviceCount; i++) {
    byte data[9];
    if (readScratchpad(sensorList[i].address, data)) {
      sensorList[i].isOnline = true; 

      int16_t raw = (data[1] << 8) | data[0];
      sensorList[i].currentTemp = (float)raw / 16.0;
      
      if (sensorList[i].currentTemp >= (float)sensorList[i].highTempLimit) {
        sensorList[i].isAlarming = true;
        //If this specific sensor is alarming, the whole system is Overtemp
        OverTemp = true; 
        Serial.println(F("Overtemperature detected!"));
        Serial.print("Device ");
        for(int j = 0; j < 8; j++){
          Serial.print(sensorList[i].address[j]);
          Serial.print(":");
        }
        Serial.print(F(" is at "));
        Serial.print(sensorList[i].currentTemp);
        Serial.println("!");
      } else {
        sensorList[i].isAlarming = false;
      }
    } else {
      sensorList[i].isOnline = false; 
    }
  }
}

void refreshLiveAddressBuffer() {
  //Makes a list of all live device addresses for reporting.
  liveAddressCount = 0; // Reset the count
  for (int i = 0; i < deviceCount; i++) {
    // Only include if it's in our SPIFFS inventory AND currently responding
    if (sensorList[i].isOnline) {
      memcpy(liveAddresses[liveAddressCount], sensorList[i].address, 8);
      liveAddressCount++;
    }
  }
}

void printAddress(byte addr[8]) {
  for (int i = 0; i < 8; i++) {
    if (addr[i] < 16) Serial.print("0");
    Serial.print(addr[i], HEX);
    if (i < 7) Serial.print(":");
  }
}

bool readScratchpad(byte addr[8], byte* buffer) {
  ds.reset();
  ds.select(addr);
  ds.write(0xBE); 
  for (int i = 0; i < 9; i++) buffer[i] = ds.read();
  return (OneWire::crc8(buffer, 8) == buffer[8]);
}

uint32_t fetchMetadata(byte addr[8], byte &outType, byte &outHigh) {
  byte data[9];
  if (readScratchpad(addr, data)) {
    outHigh = data[2]; // TH is our High Temp Limit
    outType = (data[3] >> 3) & 0x07;
    
    uint32_t combined = 0;
    combined |= (uint32_t)data[7] << 9; 
    combined |= (uint32_t)data[6] << 1;
    combined |= (uint32_t)(data[3] & 0x04) >> 2;
    return combined & 0x1FFFF;
  }
  return 0xFFFFFFFF;
}