void BuzzerControl(void *pvParameters){
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = 100 / portTICK_PERIOD_MS; // 100ms period
  
  xLastWakeTime = xTaskGetTickCount();
  
  while(1){
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
    if(NoBuzzer){
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      continue;
    }
    if(!BuzzerStart){
      //Already played buzzer for this state.
      continue;
    }
    if(InSystem){
      BuzzerStart = 0;
      tone(Buzzer, LOW_TONE);
      delay(ToneTime);
      tone(Buzzer, HIGH_TONE);
      delay(ToneTime);
      tone(Buzzer, 0);

    }
    if(NotInSystem || InvalidCard){
      BuzzerStart = 0;
      tone(Buzzer, HIGH_TONE);
      delay(ToneTime);
      tone(Buzzer, LOW_TONE);
      delay(ToneTime);
      tone(Buzzer, 0);
    }
    if(Fault){
      BuzzerStart = 0;
      tone(Buzzer, LOW_TONE);
      delay(ToneTime);
      tone(Buzzer, 0);
      delay(ToneTime);
      tone(Buzzer, LOW_TONE);
      delay(ToneTime);
      tone(Buzzer, 0);
      delay(ToneTime);
      tone(Buzzer, LOW_TONE);
      delay(ToneTime);
      tone(Buzzer, 0);
    }
  }
}
