/*

Status Report

This task is reposible for sending status reports to the server
Some are periodic, some are event-based

Global Variables:
DebugMode: Turns on verbose outputs
UsingNetwork: Set to 1 to reserve the network connection.

Global Variables Representing Message Types:
RegularStatus: Set to 1 to indicate it is time to send a regular status report.
TemperatureStatus: Set to 1 to indicate to send a temperature status report.
StartupStatus: Set to 1 to indicate to send a report that the system has powered on and completed startup..
StartStatus: Set to 1 to indicate to send a report that a session has started (card inserted and approved).
EndStatus: Set to 1 to indicate to send a report that a session has ended (card removed).
ChangeStatus: Set to 1 to iindicate to send a report that the machine's state has changed, outside of a session start/end.
SessionTime: Tracks in milliseconds how long the machine has been used for.
CheckNetwork: Flag indicating a potential network failure to investigate.
State: Plaintext representation of the system, for use in status updates.

*/

void StatusReport(void *pvParameters){
  while(1){
    //Check for a status reason every 500mS
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if(StartupStatus){
      SendReport("Startup");
      StartupStatus = 0;
    }
    if(TemperatureStatus){
      SendReport("Temperature");
      TemperatureStatus = 0;
    }
    if(StartStatus){
      SendReport("Session Start");
      StartStatus = 0;
    }
    if(EndStatus){
      SendReport("Session End");
      EndStatus = 0;
      LogoffSent = 1;
    }
    if(ChangeStatus){
      SendReport("State Change");
      ChangeStatus = 0;
    }
    if(RegularStatus){
      SendReport("Scheduled");
      RegularStatus = 0;
    }
  }
}

void RegularReport(void *pvParameters){
  //This sub-task is just to set the regular report flag periodically
  while(1){
    RegularStatus = 1;
    vTaskDelay(Frequency*1000 / portTICK_PERIOD_MS);
  }
}

void SendReport(String Reason){
  //This function is what specifically handles sending a status report.
  bool StatusSuccess = 0;
  bool StatusFailed = 0;
  while(StatusSuccess == 0){
    //First, we gather all the data needed for a status report.
    JsonDocument status;
    status["Type"] = "Status";
    status["Machine"] = MachineID;
    status["MachineType"] = MachineType;
    status["Zone"] = Zone;
    //Grab the onewire mutex, so we know the temperatures are not being updated as we read it
    xSemaphoreTake(OneWireMutex, portMAX_DELAY); 
    status["Temp"] = String(SysMaxTemp, 3);
    xSemaphoreGive(OneWireMutex);
    status["BEVer"] = Version;
    status["FEVer"] = FEVer;
    status["HWVer"] = Hardware;
    //Reserve the state so it doesn't change;
    xSemaphoreTake(StateMutex, portMAX_DELAY); 
    status["State"] = State;
    xSemaphoreGive(StateMutex);
    if (CardPresent) {
      status["UID"] = UID;
    } else {
      status["UID"] = "";
    }
    status["Time"] = SessionTime / 1000;
    status["Source"] = Reason;
    status["Frequency"] = Frequency;
    status["Key"] = Key;
    String statuspayload;
    serializeJson(status, statuspayload);
    //Before we can transmit, we need to reserve the network peripehral.
    xSemaphoreTake(NetworkMutex, portMAX_DELAY); 
    if(DebugMode){
      if(xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
        Debug.println(F("Sending Status Message: "));
        Debug.println(statuspayload);
        xSemaphoreGive(DebugMutex);
      }
    }
    HTTPClient http;
    String ServerPath = Server + "/api/status";
    if(DebugMode){
      if(xSemaphoreTake(DebugMutex,portMAX_DELAY) == pdTRUE){
        Debug.print(F("To: "));
        Debug.println(ServerPath);
        xSemaphoreGive(DebugMutex);
      }
    }
    http.begin(client, ServerPath);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.PUT(statuspayload);
    if(DebugMode){
      if(xSemaphoreTake(DebugMutex,(5/portTICK_PERIOD_MS)) == pdTRUE){
        Debug.print(F("HTTP Response: "));
        Debug.println(httpCode);
        xSemaphoreGive(DebugMutex);
      }
    }
    http.end();
    xSemaphoreGive(NetworkMutex);
    if (httpCode == 200) {
      LastServer = millis();
      StatusSuccess = 1;
    } else {
      //Bad HTTP response? Try only once more.
      if(httpCode < 0){
        //Error code
        xSemaphoreTake(DebugMutex, portMAX_DELAY);
        Debug.print(F("HTTP ERROR: "));
        Debug.println(http.errorToString(httpCode));
        xSemaphoreGive(DebugMutex);
      }
      if(StatusFailed){
        //Failed twice. 
        CheckNetwork = 1;
        break;
        vTaskDelay(10000 / portTICK_PERIOD_MS);
      }
      StatusFailed = 1;
    }
  }
}