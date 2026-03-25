/* 
These tasks are responsible for communicating with the frontend, handling things like switch states, LED and buzzer control, etc.
There are 3 tasks;
  InternalSerial - Handles sending data to and receiving data from the frontend controller.
  AVControl - Converts flags from other tasks into a series of LED lights and buzzer tones.
  RestartController - Listens for the front button to be held for 5 seconds, restarts the device. All other reset source throughout the code are handled by this task as well.
*/

//Variables - Inter-Task Communication (Outside Frontend)
bool Button = 0;  //Set to 1 when frontend button pressed.
bool Switch1 = 0;  //Set to 1 when switch 1 of card detect is pressed.
bool Switch2 = 0;  //Set to 1 when switch 2 of the card detect is pressed.

//Variables - Inter-Task Communication (Inside Frontend)
bool NewLED = 0; //Set to 1 when there is new valid data to send to the frontend for the LEDs.
byte Red = 0; //Tracks the red channel light intensity
byte Green = 0; //Tracks the green channel light intensity
byte Blue = 0; //Tracks the blue channel light intensity
bool NewBuzzer = 0; //Set to 1 when there is a new valid buzzer tone to send.
unsigned int Tone = 0; //Set to the tone that the buzzer should play.
bool ResetLED = 0; //Set to 1 to take priority over the LED controller, to indicate the restart is imminent.

#define HIGH_TONE 2000 //Hz
#define LOW_TONE 1500 //Hz

void InternalSerial(void *pvParameters){
  unsigned long long PollTime = 0;
  unsigned long long OkToLED = 0;
  while(1){

    //1: Check if it has been long enough to send a poll message. This lets us periodically get information from the frontend for InternalRead to use.
    if(PollTime <= millis64()){
      Polltime = millis64() + 1000;
      Internal.println("P");
    }

    //2: See if there is new LED information to send.
    if(NewLED){
      //Check if it has been too soon since we last sent an update, so we prevent flashing lights rapidly.
      if(OkToLED <= millis64()){
        //It has been long enough, we can send this LED update.
        OkToLED = millis64() + 333; //No more than 3 updates a second
        Internal.println("L " + String(Red) + "," + String(Green) + "," + String(Blue));
        NewLED = 0;
      }
    }

    //3: See if there is new buzzer information to send.
    if(NewBuzzer){
      Internal.println("B " + String(Tone));
      NewBuzzer = 0;
    }

    //4. Re-assert the state of the ACS output constantly.
    Internal.println("S " + String(Access));

    //Wait a bit to give the frontend time to send us data.
    vTaskDelay(20 / portTICK_PERIOD_MS);

    //5: See if there is anything from the frontend;
    while(Internal.available()){
      //Loop until we clear the input;
      String incoming = Internal.readStringUntil('\n');
      incoming.trim();
      if(incoming.charAt(0) == 'B'){
        //Message pertains to the buttons
        if(incoming.charAt(2) == '1'){
          Button = 0;
        } else{
          Button = 1;
        }
      }
      if(incoming.charAt(0) == 'S'){
        //Message pertains to the card detect switches
        bool state;
        if(incoming.charAt(3) == '1'){
          //Inverted logic, button press is 0
          state = 0;
        } else{
          state = 1;
        }
        if(incoming.charAt(1) == '1'){
          //This is about switch 1
          Switch1 = state;
        } else{
          //This is about switch 2
          Switch2 = state;
        }
      }
    }

    //Some downtime until we loop again
    vTaskDelay(50 / portTICK_PERIOD_MS);

  }
}

void AVControl(void *pvParameters){
  byte LEDAnimation;
  byte OldLEDAnimation;
  uint64_t AnimationTime;
  byte AnimationBlock;
  byte Melody;
  byte OldMelody;
  byte DonePlaying;
  byte MelodyStep;
  uint64_t MelodyTime = 0;
  while(1){
    vTaskDelay(20 / portTICK_PERIOD_MS);
    //First, set the animation state:
    //Animation triggers are not exclusive, so if statements written in reverse-priority order.
    if(State.equals("IDLE")){
      //Animation 3: Solid Yellow
      LEDAnimation = 3;
    }
    if((PendingApproval){
      //Animation 4: Flashing Yellow
      LEDAnimation = 4;
    }
    if(State.equals("UNLOCKED") || State.equals("ALWAYS_ON")){
      //Animation 2: Solid Green
      LEDAnimation = 2;
    }
    if(State.equals("LOCKED_OUT") || AccessDenied){
      //Animation 1: Solid Red
      LEDAnimation = 1;
    }
    if(State.equals("UNKNOWN")){
      //Animation 7: Solid Blue
      LEDAnimation = 7;
    }
    if(Identify){
      //Animation 9: Flashing Blue
      LEDAnimation = 9;
    }
    if(ResetLED){
      //Animation 8: Solid Purple
      LEDAnimation = 8;
    }
    if(State.equals("FAULT")){
      LEDAnimation = 0;
    }
    if(GamerMode){
      LEDAnimation = 10;
    }
    //Next, see if the animation changed;
    if(LEDAnimation != OldLEDAnimation){
      OldLEDAnimation = LEDAnimation;
      AnimationTime = 0; //Force an update of the animation block
    } 
    if(AnimationTime <= millis64()){
      //It is time to advance to the next animation block
      AnimationBlock++;
      if(LEDAnimation == 5){
        //Animation 5 runs at a slower speed
        AnimationTime = millis64() + 3000;
      } else{
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
        //Cycle Green/White
        if(AnimationBlock == 1){
          Red = 0;
          Green = 255;
          Blue = 0;
        } else{
          Red = 255;
          Green = 255;
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
        if(AnimationBlock == 2){
          Red = 0;
          Green = 255;
          Blue = 0;
        }
        if(AnimationBlock == 3){
          Red = 0;
          Green = 0;
          Blue = 255;
          AnimationBlock = 0;
        }
      break;
      }
      NewLED = 1;
    }
    
    //After the LED, we need to set up the buzzer.

    Buzzer todos
    AccessDenied
    NFCBroken


    Melody = 0; //Set to 0 so if no situations apply, we stop playing
    //Reserve the State string so it doesn't change while we are comparing it.
    xSemaphoreTake(StateMutex, portMAX_DELAY); 
    if(State.equals("Unlocked")){
      //The machine has been unlocked
      //Since we only play a tone on a melody change, constalty checking this should be fine...
      Melody = 1;
    }
    xSemaphoreGive(StateMutex);
    if(CardVerified && !CardStatus && CardPresent){
      Melody = 2;
    }
    if(TemperatureFault || Fault || ReadError){
      Melody = 3;
    }
    if(VerifiedBeep && (State != "Idle" || NoNetwork)){
      Melody = 4;
    } else{
      VerifiedBeep = 0;
    }
    if(ChangeBeep){
      Melody = 4;
    }
    if(Identify){
      //Play a constant tone to identify the device
      Melody = 5;
    }
    if(Melody == 0){
      //No tone to play
      Tone = 0;
      NewBuzzer = 1;
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
        switch (MelodyStep){
          case 0:
            Tone = LOW_TONE;
          break;
          case 1:
            Tone = HIGH_TONE;
          break;
          case 2:
            Tone = 0;
            DonePlaying = 1;
          break;
        }
      break;
      case 2:
        switch (MelodyStep){
          case 0: 
            Tone = HIGH_TONE;
          break;
          case 1:
            Tone = LOW_TONE;
          break;
          case 2:
            Tone = 0;
            DonePlaying = 1;
          break;
        }
      break;
      case 3:
        switch (MelodyStep){
          case 0:
            Tone = LOW_TONE;
          break;
          case 1:
            Tone = 0;
          break;
          case 2:
            Tone = LOW_TONE;
          break;
          case 3:
            Tone = 0;
          break;
          case 4:
            Tone = LOW_TONE;
          break;
          case 5:
            Tone = 0;
            ReadError = 0; //Turn off this flag in case that's the trigger.
            DonePlaying = 1;
          break;
        }
      break;
      case 4:
        switch(MelodyStep){
          case 0:
            Tone = HIGH_TONE;
          break;
          case 1:
            Tone = 0;
            VerifiedBeep = 0;
            ChangeBeep = 0;
            DonePlaying = 1;
          break;
        }
      break;
      case 5:
        switch(MelodyStep){
          //No case 0, this basically makes the sound delay before it starts playing.
          case 1:
            Tone = HIGH_TONE;
          break;
          case 2:
            Tone = LOW_TONE;
            MelodyStep = 0;
          break;
        }
      break;
    }
    NewBuzzer = 1;
    MelodyStep++;
  }
}

void RestartController(void *pvParameters){
  unsigned long long ButtonTime = 0;
  while(1){
    //First, check if the button is being held to trigger a restart;
    if(!Button){
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
      vTaskSuspendAll(); //Don't let other tasks take over.
      internal.println("L 0,0,255"); //Set LED blue
      internal.flush();
      settings.putString("ResetReason",ResetReason);
      delay(50);
      ESP.restart();
    }
  }
}