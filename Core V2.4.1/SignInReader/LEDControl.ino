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
    
    // TODO: Validate this new ordering. 
    if (!digitalRead(Button)) {
      red = 0;
      green = 0;
      blue = 255;
    } else if (networkCheck) {
      red = 255;
      green = 255;
      blue = 255;
    } else if (inSystem) {
      green = 255;
      red = 0;
      blue = 0;
    } else if (notInSystem || invalidCard) {
      red = 255;
      green = 0;
      blue = 0;
    } else if (!ready) {
      red = 255;
      green = 255;
      blue = 0;
    }

    LEDColor(red, green, blue);
  }
}