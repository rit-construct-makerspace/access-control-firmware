/*

Message Report

This task handles sending plaintext to the server as debug messages.

Global Variables Used:
WritingMessage: bool, indicates a message is being written for sending.
Message: String, the message to be sent.
ReadyToSend: bool, indicates the other task has finished writing its message and it is ok to send.

*/

/* RETIRED
void MessageReport(void *pvParameters) {
  while(1){
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    if(ReadyToSend){
      //A message is ready to send
      if(DebugMode){
        xSemaphoreTake(DebugMutex, portMAX_DELAY);
        Serial.print(F("Sending Message: ")); Serial.println(Message);
        xSemaphoreGive(DebugMutex);
      }
      xSemaphoreTake(MessageMutex, portMAX_DELAY); 
      xSemaphoreTake(NetworkMutex, portMAX_DELAY);
      String ServerPath = Server + "/api/message/" + MachineID + "?message=" + Message;
      http.begin(client, ServerPath);
      if(DebugMode){
        xSemaphoreTake(DebugMutex, portMAX_DELAY);
        Serial.println(F("Sending message...."));
      }
      int response = http.GET();
      if(DebugMode){
        Serial.print(F("Got HTTP Response: "));
        Serial.println(response);
        xSemaphoreGive(DebugMutex);
      }
      http.end();
      xSemaphoreGive(NetworkMutex);
      xSemaphoreGive(MessageMutex);
      Serial.println(F("Done sending message."));
      //Set back to 0 to indiate it is OK to write a new message.
      ReadyToSend = 0;
      WritingMessage = 0;
    }
  }
}

*/