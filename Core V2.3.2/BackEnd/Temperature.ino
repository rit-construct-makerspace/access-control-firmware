/*

Temperature Monitoring via OneWire

This process is responsible for 2 main things;
1. Monitoring the temperature of all devices on the bus, to check for overtemperature issues that require a fault to stop equipment use
2. MOnitoring the composition and integrity of the bus, based on the unique IDs found in each OneWire device.

*/

const char* INV_FILE = "/inventory.dat"; //Where we store onewire devices for bus integrity monitoring



OneWire ds(TEMP); 

void Temperature(void *pvParameters){

  bool TemperatureMessage;
  //First time running, find the addresses
  if(DebugMode){
    xSemaphoreTake(DebugMutex, portMAX_DELAY);
    Serial.println(F("First time running OneWire..."));
    xSemaphoreGive(DebugMutex);
  }
  xSemaphoreTake(OneWireMutex, portMAX_DELAY); 
  //First, recover the old configuration from memory
  loadInventoryFromFile();
  //If the old configuration has no devices in it, we should discover as we are.
  if(deviceCount == 0){
     Serial.println(F("Inventory empty! Scanning to initialize..."));
     discoverDevices(); // This sets the initial baseline
     saveInventoryToFile(); // Save it so it's not empty next time
  }
  //We should make sure the internal OneWire device is configured properly;
  configurePriorityDevice(SerialNumber);
  if(deviceCount == 0){
    //Something has gone wrong, there should be at least 1 temperature sensor. Restart?
    Serial.println(F("ERROR: Found no OneWire devices?"));
    ESP.restart();
  }
  xSemaphoreGive(OneWireMutex);

  //Infinite loop, measure all temperatures
  bool TemperatureAlarm = 0;
  while(1){
    //Reserve the OneWire bus
    xSemaphoreTake(OneWireMutex, portMAX_DELAY); 
    //First, check if we should commit the current bus to memory;
    if(ReSealBus){
      commitCurrentBusToInventory();
      ReSealBus = false;
    }
    //Next, check the bus health against the saved list...
    checkBusHealth();
    //Then, get the temperature of each device expected.
    updateBusTemperatures();
    //Lastly, update this info so we can send it to the server
    if(UpdateAddressBuffer){
      refreshLiveAddressBuffer();
      AddressBufferValid = 1;
      UpdateAddressBuffer = 0;
    }
    if(DebugMode){
      Serial.println(F("\n--- Sensor Report ---"));
      for (int i = 0; i < deviceCount; i++) {
        // Print ID
        Serial.print(F("ID: ")); 
        printAddress(sensorList[i].address);
        
        // Print current stats
        Serial.print(F(" | Temp: ")); Serial.print(sensorList[i].currentTemp);
        Serial.print(F("C / Limit: ")); Serial.print(sensorList[i].highTempLimit);
        
        // --- ADDED METADATA HERE ---
        Serial.print(F(" | Type: "));  Serial.print(sensorList[i].deviceType);
        Serial.print(F(" | Class: ")); Serial.print(sensorList[i].classification);
        // ---------------------------

        if (!sensorList[i].isOnline) {
          Serial.println(F(" [OFFLINE]"));
        } else if (sensorList[i].isAlarming) {
          TemperatureAlarm = 1;
          Serial.println(F(" [!!! OVERHEAT !!!]"));
        } else {
          Serial.println(F(" [OK]"));
        }
      }
      Serial.println(F("---------------------"));
    }
    TemperatureUpdate = 0;
    if(TemperatureAlarm){
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
    //Release the OneWire bus;
    xSemaphoreGive(OneWireMutex);
    vTaskDelay(TemperatureTime / portTICK_PERIOD_MS);
  }
}

//OneWire Helper Functions;

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

void rawWrite(byte* addr, byte th, byte tl, byte cfg, byte u3, byte u4) {
  ds.reset();
  ds.select(addr);
  ds.write(0x4E);      
  ds.write(th); ds.write(tl); ds.write(cfg);
  ds.write(u3); ds.write(u4); 
  ds.reset();
  ds.select(addr);
  ds.write(0x48); 
  delay(100); 
}

// --- Logic & Monitoring ---

void updateBusTemperatures() {
  ds.reset();
  ds.skip();
  ds.write(0x44); 
  delay(750); 

  for (int i = 0; i < deviceCount; i++) {
    byte data[9];
    if (readScratchpad(sensorList[i].address, data)) {
      // --- ADD THIS LINE ---
      sensorList[i].isOnline = true; 
      // ---------------------

      int16_t raw = (data[1] << 8) | data[0];
      sensorList[i].currentTemp = (float)raw / 16.0;
      
      if (sensorList[i].currentTemp >= (float)sensorList[i].highTempLimit) {
        sensorList[i].isAlarming = true;
      } else {
        sensorList[i].isAlarming = false;
      }
    } else {
      // --- ADD THIS LINE TOO ---
      sensorList[i].isOnline = false; 
      // -------------------------
    }
  }
}

void discoverDevices() {
  byte addr[8];
  deviceCount = 0;
  ds.reset_search();
  Serial.println("\n--- Scanning Bus ---");
  while (ds.search(addr) && deviceCount < MAX_DEVICES) {
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

void writeAndVerify(byte* addr, byte newType, uint32_t newClass, byte newHighTemp) {
  byte current[9];
  if (!readScratchpad(addr, current)) return;

  byte b3_tl = 0x80 | ((newType & 0x07) << 3) | ((newClass & 0x01) << 2) | 0x03;
  byte b6 = (newClass >> 1) & 0xFF;
  byte b7 = (newClass >> 9) & 0xFF;

  rawWrite(addr, newHighTemp, b3_tl, current[4], b6, b7);
  Serial.println("EEPROM Updated.");
  
  // REMOVE THIS: discoverDevices(); 
  
  // ADD THIS: Update the specific sensor in the list manually
  for (int i = 0; i < deviceCount; i++) {
    if (memcmp(addr, sensorList[i].address, 8) == 0) {
      sensorList[i].highTempLimit = newHighTemp;
      sensorList[i].classification = newClass;
      sensorList[i].deviceType = newType;
      break;
    }
  }
}

void configurePriorityDevice(String reversedHex) {
  byte targetAddr[8];
  for (int i = 0; i < 8; i++) {
    int strPos = (7 - i) * 2; 
    String part = reversedHex.substring(strPos, strPos + 2);
    targetAddr[i] = (byte) strtoul(part.c_str(), NULL, 16);
  }
  writeAndVerify(targetAddr, 6, 20, 60);
}


void checkBusHealth() {
  byte addr[8];
  int liveCount = 0;
  bool currentMismatch = false;

  // 1. Reset responded status for all known sensors
  bool responded[MAX_DEVICES] = {false};

  ds.reset_search();
  while (ds.search(addr)) {
    if (OneWire::crc8(addr, 7) != addr[7]) continue;
    liveCount++;

    bool foundInInventory = false;
    for (int i = 0; i < deviceCount; i++) {
      if (memcmp(addr, sensorList[i].address, 8) == 0) {
        foundInInventory = true;
        responded[i] = true;
        sensorList[i].isOnline = true; // Mark it online for temperature updates
        break;
      }
    }
    
    if (!foundInInventory) {
      Serial.print("SECURITY ALERT: Unknown device detected: ");
      printAddress(addr);
      Serial.println();
      currentMismatch = true; 
    }
  }

  // 2. Check for missing devices
  for (int i = 0; i < deviceCount; i++) {
    if (!responded[i]) {
      Serial.print("SECURITY ALERT: Missing device: ");
      printAddress(sensorList[i].address);
      Serial.println();
      currentMismatch = true;
      sensorList[i].isOnline = false;
    }
  }

  // 3. Final count check
  if (liveCount != deviceCount) {
    currentMismatch = true;
  }

  // 4. State Management
  if (currentMismatch) {
    if (!SealBroken) {
      Serial.println("BUS SEAL RUPTURED!");
      SealBroken = true;
      SealFault = true;
      SendWSReconnect = true;
    }
  } else {
    // Optional: Auto-clear the fault if the bus returns to the saved state?
    // If you want manual re-seal only, leave SealBroken = true until ReSealBus is toggled.
  }
}

void saveInventoryToFile() {
  File file = SPIFFS.open(INV_FILE, FILE_WRITE);
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
  SealFault = false;
}

void loadInventoryFromFile() {
  if (!SPIFFS.exists(INV_FILE)) {
    Serial.println("No inventory file found. System is uninitialized.");
    deviceCount = 0;
    return;
  }

  File file = SPIFFS.open(INV_FILE, FILE_READ);
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

void commitCurrentBusToInventory() {
  Serial.println("Refreshing hardware inventory...");
  
  // 1. Run a fresh scan to see what's physically there
  deviceCount = 0;
  ds.reset_search();
  byte addr[8];
  while (ds.search(addr) && deviceCount < MAX_DEVICES) {
    if (OneWire::crc8(addr, 7) != addr[7]) continue;
    memcpy(sensorList[deviceCount].address, addr, 8);
    deviceCount++;
  }

  // 2. Save this exact list to Flash
  saveInventoryToFile();
}

void refreshLiveAddressBuffer() {
  liveAddressCount = 0; // Reset the count
  for (int i = 0; i < deviceCount; i++) {
    // Only include if it's in our SPIFFS inventory AND currently responding
    if (sensorList[i].isOnline) {
      memcpy(liveAddresses[liveAddressCount], sensorList[i].address, 8);
      liveAddressCount++;
    }
  }
}