void BuzzerControl(void *pvParameters){
  while(1){
    delay(100);
    if(NoBuzzer){
      delay(10000);
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
