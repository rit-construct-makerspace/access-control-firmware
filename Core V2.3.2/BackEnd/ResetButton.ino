//Reset Button

//This task just handles the reset button. 

//Also the startup lighting sequence runs here, but is executed by LEDControl. 

void ResetButton(void *pvParameters){

  uint64_t ButtonTime = millis64() + 5000;

  while(1){
    delay(3);
    //Check the button on the front panel. If it has been held down for more than 3 seconds, restart. 
    //Constantly set the reset time 5 seconds in the future when the button isn't pressed.
    if(Button){
      ButtonTime = millis64() + 3000;
      ResetLED = 0;
    } else{
      ResetLED = 1;
      //ALSO: Turn off identify tone if playing. Makes it easy to ID
      //TODO: This doesn't inform the server so we get a desync between what the server thinks and actuality.
      Identify = 0;
    }

    //Incrementing the startup "Gamer Mode" lighting for startup handled here.
    if(GamerMode){
      if((GamerModeTime + LEDFlashTime) <= millis64()){
        GamerModeTime = millis64();
        GamerStep++;
        if(GamerStep >= 3){
          GamerStep = 0;
        }
      }
    } else{
      GamerStep = 4; //Invalid option, turns off lighting animation
    }


    if(millis64() >= ButtonTime){
      //It has been 3 seconds, restart.
      xSemaphoreTake(StateMutex, portMAX_DELAY);
      settings.putString("LastState", State);
      delay(10);
      State = "Restart";
      //Turn off the internal write task so that it doesn't overwrite the restart led color.
      vTaskSuspend(xHandle);
      delay(100);
      Serial.println(F("RESTARTING. Source: Button."));
      settings.putString("ResetReason","Restart-Button");
      Internal.println("L 255,0,0");
      Internal.println("S 0");
      Internal.flush();
      ESP.restart();
    }
  }
}
