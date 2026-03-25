/*
The Bus Manager is responsible for everything OneWire related
  * Getting bus temperatures
  * Checking for bus integrity
*/

//Variables - Inter-Task Communication
bool SealBroken = 0;  //Set to 1 if there is an incorrect OneWire device on the bus. 
bool Overtemp = 0;    //Set to 1 if there is a device overtemperature on the bus, so we can fault. 
byte liveAddresses[5][8];      //OneWire addresses of what is currently connected, for server reporting.
int liveAddressCount = 0;                //Number of currently connected devices on the bus, for server reporting.

void BusManager(void *pvParameters){
  byte deviceCount = 0; //Tracks the number of OneWire devices found on the bus.
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
    //Step 1: Check if it is time to check the bus
    if(OneWireTime <= millis64()){
      OneWireTime = millis64() + 10000; //Check again in 10 seconds
      //Step 2: Scan the bus for devices, make sure they are all there
      checkBusHealth();
      if(SealBroken){
        refreshLiveAddresSBuffer();
      }
      //Step 3: Check for any overtemp devices
      updateBusTemperatures();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void loadInventoryFromFile() {
  //Gets the expected list of OneWire devices from memory.
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

void discoverDevices() {
  //Discovers and categorizes all devices in a deployment.
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

void saveInventoryToFile() {
  //Saves the current list of debices to SPIFFs, treating it as a valid deployment. 
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
}

void checkBusHealth() {
  byte addr[8];
  int liveCount = 0;
  bool currentMismatch = false;
  bool responded[MAX_DEVICES] = {false};

  // 1. Initial Full Bus Scan
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

  // 2. Double-Check Missing Devices
  for (int i = 0; i < deviceCount; i++) {
    if (!responded[i]) {
      // Try to reset the bus and specifically address this "missing" device
      ds.reset();
      ds.select(sensorList[i].address);
      
      // If the device responds to a direct select, it's actually there!
      // We check if the device pulls the bus low (presence pulse)
      if (ds.reset()) { 
        // We do a second search or a simple read attempt to verify
        responded[i] = true;
        sensorList[i].isOnline = true;
        liveCount++;
        Serial.print("Recovery: Device ");
        printAddress(sensorList[i].address);
        Serial.println(" found on retry.");
      } else {
        // Still missing after retry
        Serial.print("SECURITY ALERT: Missing device: ");
        printAddress(sensorList[i].address);
        Serial.println();
        currentMismatch = true;
        sensorList[i].isOnline = false;
      }
    }
  }

  // 3. Final Count Comparison
  if (liveCount != deviceCount) {
    currentMismatch = true;
  }

  // 4. State Management & Auto-Recovery
  if (currentMismatch) {
    if (!SealBroken) {
      Serial.println("BUS SEAL RUPTURED!");
      SealBroken = true;
    }
  } else {
    //If the bus layout is now perfect, go back to normal
    /*
    Taken out for now, fault states should be considered terminal and require a restart no matter what.
    if (SealBroken) {
      Serial.println("Bus integrity restored. Seal re-established.");
      SealBroken = false;
    }
    */
  }
}

void updateBusTemperatures() {
  //Scans through the bus, gets the temperature of every device.
  ds.reset();
  ds.skip();
  ds.write(0x44); 
  delay(750); 

  //Start by assuming everything is fine. 
  Overtemp = false; 

  for (int i = 0; i < deviceCount; i++) {
    byte data[9];
    if (readScratchpad(sensorList[i].address, data)) {
      sensorList[i].isOnline = true; 

      int16_t raw = (data[1] << 8) | data[0];
      sensorList[i].currentTemp = (float)raw / 16.0;
      
      if (sensorList[i].currentTemp >= (float)sensorList[i].highTempLimit) {
        sensorList[i].isAlarming = true;
        //If this specific sensor is alarming, the whole system is Overtemp
        Overtemp = true; 
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
