void BuzzerControl(void *pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 100 / portTICK_PERIOD_MS; // 100ms period
  
  xLastWakeTime = xTaskGetTickCount();
  
  while(1){
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
    if (noBuzzer){
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      continue;
    }

    if (!buzzerStart){ 
      //Already played buzzer for this state.
      continue;
    }

    if (inSystem){
      buzzerStart = false;
      shouldPlaySuccess = true;
      playSuccessTone();

    } else if (notInSystem || invalidCard){
      buzzerStart = false;
      playFailureTone();

    } else if(fault){
      buzzerStart = false;
      playfaultTone();
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

void playfaultTone() {
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