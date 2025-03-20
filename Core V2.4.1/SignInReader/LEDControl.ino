void LEDControl(void *pvParameters){
  while(1){
    delay(20);
    //Go through and check states to see what we should be doing;
    byte red = 0;
    byte green = 0;
    byte blue = 0;
    if(!Ready){
      red = 255;
      green = 255;
      blue = 0;
    }
    if(InSystem){
      green = 255;
      red = 0;
      blue = 0;
    }
    if(NotInSystem || InvalidCard){
      red = 255;
      green = 0;
      blue = 0;
    }
    if(NoNetwork){
      red = 255;
      green = 255;
      blue = 255;
    }
    if(!digitalRead(Button)){
      red = 0;
      green = 0;
      blue = 255;
    }
    LEDColor(red, green, blue);
  }
}