/* 
These tasks are responsible for communicating with the frontend, handling things like switch states, LED and buzzer control, etc.
There are 3 tasks;
  AVControl - Converts flags from other tasks into a series of LED lights and buzzer tones.
  RestartController - Listens for the front button to be held for 5 seconds, restarts the device. All other reset source throughout the code are handled by this task as well.
*/

void AVControl(void *pvParameters){
  unsigned long long OkToLED = 0;
  byte LEDAnimation;
  byte OldLEDAnimation;
  uint64_t AnimationTime;
  byte AnimationBlock;
  byte Melody;
  byte OldMelody;
  byte DonePlaying;
  byte MelodyStep;
  uint64_t MelodyTime = 0;
  bool AnimationHelper = false; //Generic global bool for animations to use
  //Serial.println(F("AVControl Started."));
  while(1){
    vTaskDelay(10 / portTICK_PERIOD_MS);
    //First, set the animation state:
    //Animation triggers are not exclusive, so if statements written in reverse-priority order.

    if(WelcomeMode){
      //For now it is solid yellow, TODO switch to a slow blink yellow 10% duty cycle or so to catch user attention.
      LEDAnimation = 12;
    }
    if(WelcomingPending){
      //Animation 4: Flashing Yellow
      LEDAnimation = 4;
    }
    if(UserWelcomed){
      //The user was welcomed!
      //Act like we are giving access;
      //(Solid Green)
      LEDAnimation = 2;
    }
    if(NoNetwork){
      //Animation 7: Solid Blue
      LEDAnimation = 7;
    }
    if(AccessDenied){
      //Animation 1: Solid Red
      LEDAnimation = 1;
    }
    if(Identify){
      //Animation 9: Flashing Blue
      LEDAnimation = 9;
    }
    if(ResetLED){
      //Animation 8: Solid Purple
      LEDAnimation = 8;
    }
    if(GamerMode){
      LEDAnimation = 10;
    }
    //Next, see if the animation changed;
    if(LEDAnimation != OldLEDAnimation){
      OldLEDAnimation = LEDAnimation;
      AnimationTime = 0; //Force an update of the animation block
      AnimationBlock = 0;
      AnimationHelper = false; 
    } 
    if(AnimationTime <= millis64()){
      //It is time to advance to the next animation block
      AnimationBlock++;
      if(LEDAnimation == 5){
        //Animation 5 runs at a slower speed
        AnimationTime = millis64() + 3000;
      } else if(LEDAnimation == 12){
        //12 runs way faster
        AnimationTime = millis64() + 10;
      }else{
        AnimationTime = millis64() + 400;
      }
      //Set the animation block here;
      switch(LEDAnimation){
      case 0:
        //Flashing Red
        if(AnimationBlock == 1){
          Red = 255;
          Green = 0;
          Blue = 0;
        } else{
          Red = 0;
          Green = 0;
          Blue = 0;
          AnimationBlock = 0;
        }
      break;
      case 1:
        //Solid Red
        Red = 255;
        Green = 0;
        Blue = 0;
      break;
      case 2:
        //Solid Green
        Red = 0;
        Green = 255;
        Blue = 0;
      break;
      case 3:
        //Solid Yellow
        Red = 255;
        Green = 255;
        Blue = 0;
      break;
      case 4:
        //Flashing Yellow
        if(AnimationBlock == 1){
          Red = 255;
          Green = 255;
          Blue = 0;
        } else{
          Red = 0;
          Green = 0;
          Blue = 0;
          AnimationBlock = 0;
        }
      break;
      case 5:
        //Cycle Green/Blue
        if(AnimationBlock == 1){
          Red = 0;
          Green = 255;
          Blue = 0;
        } else{
          Red = 0;
          Green = 0;
          Blue = 255;
          AnimationBlock = 0;
        }
      break;
      case 6:
        //Solid White
        //Red = 255;
        //Green = 255;
        //Blue = 255;
        //This break the led. go for blue instead.
        Red = 0;
        Green = 0;
        Blue = 255;
      break;
      case 7:
        //Solid Blue
        Red = 0;
        Green = 0;
        Blue = 255;
      break;
      case 8:
        //Solid Purple
        Red = 255;
        Green = 0;
        Blue = 255;
      break;
      case 9:
        //Flashing blue
        if(AnimationBlock == 1){
          Red = 0;
          Green = 0;
          Blue = 255;
        } else{
          Red = 0;
          Green = 0;
          Blue = 0;
          AnimationBlock = 0;
        }
      break;
      case 10:
        //Red - Green - Blue
        if(AnimationBlock == 1){
          Red = 255;
          Green = 0;
          Blue = 0;
        }
        else if(AnimationBlock == 2){
          Red = 0;
          Green = 255;
          Blue = 0;
        }
        else{
          Red = 0;
          Green = 0;
          Blue = 255;
          AnimationBlock = 0;
        }
      break;
      case 11:
        //Alternate red-green
        if(AnimationBlock == 1){
          Red = 255;
          Green = 0;
          Blue = 0;
        } else{
          Red = 0;
          Green = 255;
          Blue = 0;
          AnimationBlock = 0;
        }
      break;
      case 12:
        //Breathe up and down yellow;
        AnimationBlock++;
        byte DimColor = 0;
        if (AnimationBlock == 0) {
          //This means we rolled over, so we should invert the AnimationHelper
          AnimationHelper = !AnimationHelper;
        }
        if(AnimationHelper){
          //This means that we are at max brightness, and are now decreasing...
          DimColor = 255 - AnimationBlock;
        } else{
          //This means we are not at max brightness, so we should increase...
          DimColor = AnimationBlock;
        }
        if (DimColor <= 50) {
          //Don't get too dim!
          DimColor = 50;
        }
        //Write the values;
        Red = DimColor;
        Green = DimColor;
        Blue = 0;

      break;
      }

      CBI.setPixelColor(0, Red, Green, Blue);
      CBI.show();
      /*
      //If we are here, there is a new LED to send.
      if(OkToLED <= millis64()){
        //We enforce updates slower than 3Hz here to ensure no strobing or race conditions
        OkToLED = millis64() + 333;
        CBI.setPixelColor(0, Red, Green, Blue);
        CBI.show();
      }
      */
    }
    
    //After the LED, we need to set up the buzzer.

    if(UnlockedBeep || UserWelcomed){
      //The machine has been unlocked
      Melody = 1;
    } else if(AccessDenied){
      Melody = 2;
    } else if(FaultBeep){
      Melody = 3;
    } else if(SingleBeep){
      Melody = 4;
    } else if(Identify){
      //Play a constant tone to identify the device
      Melody = 5;
    } else if(DonePlaying){
      //If none of these apply, turn off the buzzer
      Melody = 0;
    }
    if(Melody != OldMelody){
      //Melody has changed!
      OldMelody = Melody;
      DonePlaying = 0;
      NewBuzzer = 0;
      MelodyTime = 0;
      MelodyStep = 0;
    } else if((MelodyTime >= millis64()) || DonePlaying){
      //Melody didn't change and it's not time to advance to the next tone or the system is done playing a tone
      continue;
    }
    //If we make it here, there is a tone to be changed.
    MelodyTime = millis64() + 250; //Set the time to change the note again.
    switch (Melody){
      case 1:
        //Approved tone
        switch (MelodyStep){
          case 0:
            Tone = 1500;
          break;
          case 1:
            Tone = 2000;
          break;
          case 2:
            Tone = 0;
            UnlockedBeep = 0;
            DonePlaying = 1;
          break;
        }
      break;
      case 2:
        //Denied tone
        switch (MelodyStep){
          case 0: 
            Tone = 880;
          break;
          case 1:
            Tone = 440;
          break;
          case 2:
            Tone = 0;
            DonePlaying = 1;
          break;
        }
      break;
      case 3:
        //Fault tone
        switch (MelodyStep){
          case 0:
            Tone = 1000;
          break;
          case 1:
            Tone = 0;
          break;
          case 2:
            Tone = 1000;
          break;
          case 3:
            Tone = 0;
          break;
          case 4:
            Tone = 1000;
          break;
          case 5:
            Tone = 0;
            DonePlaying = 1;
            FaultBeep = 0;
          break;
        }
      break;
      case 4:
        //Single beep
        switch(MelodyStep){
          case 0:
            Tone = 1500;
          break;
          case 1:
            Tone = 0;
            DonePlaying = 1;
            SingleBeep = 0;
          break;
        }
      break;
      case 5:
        switch(MelodyStep){
          //No case 0, this basically makes the sound delay before it starts playing.
          case 1:
            Tone = 2000;
          break;
          case 2:
            Tone = 1500;
            MelodyStep = 0;
            //This one ends when we command it to, so we don't put DonePlaying here.
            //Instead, the Identify state ends with the change beep.
          break;
        }
      break;
    }
    //If we make it here, we should update the buzzer;
    if(Tone == 0){
      noTone(BUZZER);
    } else{
      tone(BUZZER, Tone);
    }
    MelodyStep++;
  }
}


void RestartController(void *pvParameters){
  unsigned long long ButtonTime = 0;
  //Serial.println(F("RestartController Started"));
  while(1){
    //First, check if the button is being held to trigger a restart;
    delay(100);
    if(digitalRead(BUTTON)){
      //Button is not being pressed
      ButtonTime = millis64() + 3000;
      ResetLED = 0;
    } else{
      ResetLED = 1;
      //ALSO: Turn off identify tone if playing. 
      Identify = 0;
      if(ButtonTime <= millis64()){
        //Button has been held long enough to trigger a restart.
        ResetReason = "Restart Button";
        RequestReset = true;
      }
    }
    //Next, check if anything has asked for the device to be restarted.
    if(RequestReset){
      vTaskSuspendAll(); //Stop all other tasks
      //Serial.print(F("Restarting. Source: "));
      //Serial.println(ResetReason);
      //Serial.flush();
      CBI.setPixelColor(0, 255, 0, 0);
      CBI.show();
      settings.putString("ResetReason",ResetReason);
      delay(50);
      ESP.restart();
    }
  }
}