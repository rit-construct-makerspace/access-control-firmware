/*

Status Report

This task is reposible for sending status reports to the server
Some are periodic, some are event-based

Global Variables:
DebugMode: Turns on verbose outputs
DebugPrinting: Used to reserve the debug output
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
StateChange: flag to indicate a change of the state string is underway.

*/

void StatusReport(void *pvParameters){
  xTaskCreate(RegularReport, "RegularReport", 1024, NULL, 2, NULL);
  bool StatusSuccess = 0;
  bool StatusFailed = 0;
  while(1){
    if(StartupStatus){
      SendReport("Startup");
      StartupStatus = 0;
      continue;
    }
    if(TemperatureStatus){
      SendReport("Temperature");
      TemperatureStatus = 0;
      continue;
    }
    if(StartStatus){
      SendReport("Session Start");
      StartStatus = 0;
      continue;
    }
    if(EndStatus){
      SendReport("Session End");
      EndStatus = 0;
      LogoffSent = 1;
      continue;
    }
    if(ChangeStatus){
      SendReport("State Change");
      ChangeStatus = 0;
      continue;
    }
    if(RegularStatus){
      SendReport("Scheduled");
      RegularStatus = 0;
      continue;
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
  while(StatusSuccess == 0){
    //First, we gather all the data needed for a status report.
    JsonDocument status;
    status["Type"] = "Status";
    status["Machine"] = MachineID;
    status["MachineType"] = MachineType;
    status["Zone"] = Zone;
    while (TemperatureUpdate) {
      //Delay to wait for temperature measurement to complete
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    status["Temp"] = String(SysMaxTemp, 3);
    status["BEVer"] = Version;
    status["FEVer"] = FEVer;
    status["HWVer"] = Hardware;
    if(StateChange){
      //Wait for a state change to finish
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    status["State"] = State;
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
    while (UsingNetwork) {
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    UsingNetwork = 1;
    if (DebugMode && !DebugPrinting) {
      DebugPrinting = 1;
      Debug.println(F("Sending Status Message: "));
      Debug.println(statuspayload);
      DebugPrinting = 0;
    }
    HTTPClient http;
    String ServerPath = Server + "/api/status";
    http.begin(client, ServerPath);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.PUT(statuspayload);
    if (DebugMode && !DebugPrinting) {
      DebugPrinting = 1;
      Debug.print(F("HTTP Response: "));
      Debug.println(httpCode);
      DebugPrinting = 0;
    }
    http.end();
    UsingNetwork = 0;
    if (httpCode == 200) {
      LastServer = millis();
      StatusSuccess = 1;
    } else {
      //Bad HTTP response? Try only once more.
      if(StatusFailed){
        //Failed twice. 
        CheckNetwork = 1;
        break;
      }
      StatusFailed = 1;
    }
  }
}