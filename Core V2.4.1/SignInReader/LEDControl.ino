void LEDControl(void *pvParameters){
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 20 / portTICK_PERIOD_MS;
  
  xLastWakeTime = xTaskGetTickCount();
  
  while (1) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
    // Go through and check states to see what we should be doing
    byte red = 0;
    byte green = 0;
    byte blue = 0;

    // TODO: Validate this new ordering. Following logic based on previous conditions
    // Prob a micro op tho
    if (!digitalRead(Button)) {
      red = 0;
      green = 0;
      blue = 255;
    } else if (NetworkCheck) {
      red = 255;
      green = 255;
      blue = 255;
    } else if (InSystem) {
      green = 255;
      red = 0;
      blue = 0;
    } else if (NotInSystem || InvalidCard) {
      red = 255;
      green = 0;
      blue = 0;
    } else if (!Ready) {
      red = 255;
      green = 255;
      blue = 0;
    }

    LEDColor(red, green, blue);
  }
}