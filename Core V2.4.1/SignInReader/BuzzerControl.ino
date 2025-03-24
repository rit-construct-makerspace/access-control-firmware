void BuzzerControl(void *pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 100 / portTICK_PERIOD_MS; // 100ms period
  
  xLastWakeTime = xTaskGetTickCount();
  
  while(1){
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
    if (NoBuzzer){
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      continue;
    }

    if (!BuzzerStart){ // TODO: BuzzerStart is only 0? Check rest of code to see if this is set
      //Already played buzzer for this state.
      continue;
    }

    if (InSystem){
      BuzzerStart = 0;
      playSuccessTone();
    } else if (NotInSystem || InvalidCard){
      BuzzerStart = 0;
      playFailureTone();
    } else if(Fault){
      BuzzerStart = 0;
      playFaultTone();
    }
  }
}

void playSuccessTone() {
  tone(Buzzer, LOW_TONE);
  vTaskDelay(ToneTime / portTICK_PERIOD_MS);

  tone(Buzzer, HIGH_TONE);
  vTaskDelay(ToneTime / portTICK_PERIOD_MS);

  tone(Buzzer, 0);
}

void playFailureTone() {
  tone(Buzzer, HIGH_TONE);
  vTaskDelay(ToneTime / portTICK_PERIOD_MS);

  tone(Buzzer, LOW_TONE);
  vTaskDelay(ToneTime / portTICK_PERIOD_MS);

  tone(Buzzer, 0);
}

void playFaultTone() {
  tone(Buzzer, LOW_TONE);
  vTaskDelay(ToneTime / portTICK_PERIOD_MS);

  tone(Buzzer, 0);
  vTaskDelay(ToneTime / portTICK_PERIOD_MS);

  tone(Buzzer, LOW_TONE);
  vTaskDelay(ToneTime / portTICK_PERIOD_MS);

  tone(Buzzer, 0);
  vTaskDelay(ToneTime / portTICK_PERIOD_MS);

  tone(Buzzer, LOW_TONE);
  vTaskDelay(ToneTime / portTICK_PERIOD_MS);

  tone(Buzzer, 0);
}
