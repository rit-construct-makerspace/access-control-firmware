/*

LED Control

This task is responsible for running lighting animations on the front of the device.
Animation patterns are pre-defined, and swapped between as system states change. 
Many situations have a static LED color. 
The task independently monitors the system state, and sets the animations accordingly.

Animation 0: Flashing Red
* Overtemperature condition
* Unable to reach NFC reader after multiple attempts

Animation 1: Solid Red
* Lockout State
* Access Denied, until card removed

Animation 2: Solid Green
* Unlocked State
* AlwaysOn State

Animation 3: Solid Yellow
* Idle State

Animation 4: Flashing Yellow
* Card inserted, but not verified yet.

Animation 5: Cycle green/white
* Unlocked or AlwaysOn and no network

Animation 6: Solid white
* Idle and no network

Global Variables Used:
Red: LED Red Channel
Blue: LED Blue Channel
Green: LED Green Channel
NewLED: Indicates a new set of LED colors is ready to send

*/

void LEDControl(void *pvParameters) {
  byte LEDAnimation;
  byte OldLEDAnimation;
  uint64_t AnimationTime;
  bool AnimationBlock;
  while(1){
    vTaskDelay(3 / portTICK_PERIOD_MS);
    //First, set the animation state:
    //Reserve the State string for checks;
    xSemaphoreTake(StateMutex, portMAX_DELAY); 
    //Animation triggers are not exclusive, so if statements written in reverse-priority order.
    if(State.equals("Idle")){
      //Animation 3: Solid Yellow
      LEDAnimation = 3;
    }
    if((CardUnread || CardPresent) && !CardVerified && (PreState == "Idle")){
      //Animation 4: Flashing Yellow
      //We check the card pre-insert state so that if the state changes under us we don't start flashing
      LEDAnimation = 4;
    }
    if(NoNetwork){
      //Animation 6: Solid White
      LEDAnimation = 6;
    }
    if(State.equals("Unlocked") || State.equals("AlwaysOn") || (GamerStep == 1)){
      //Animation 2: Solid Green
      LEDAnimation = 2;
    }
    if((LEDAnimation == 2) && NoNetwork && !GamerMode){
      //Animation 5: Cycle green/white
      //Check we aren't in gamer mode to cause issues with startup.
      LEDAnimation = 5;
    }
    if((CardVerified && !CardStatus && CardPresent) || (State.equals("Lockout")) || (ReadFailed && CardUnread) || (GamerStep == 0)){
      //Animation 1: Solid Red
      LEDAnimation = 1;
    }
    if((!GamerMode && State.equals("Startup")) || (GamerStep == 2)){
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
    //Done checking the State, release mutex;
    xSemaphoreGive(StateMutex);
    if(TemperatureFault || Fault){
      //Animation 0: Flashing Red
      LEDAnimation = 0;
    }

    //Next, see if the animation changed;
    if(LEDAnimation != OldLEDAnimation){
      OldLEDAnimation = LEDAnimation;
      AnimationTime = 0; //Force an update of the animation block
    }
    if(AnimationTime <= millis64()){
      //It is time to advance to the next animation block
      AnimationBlock = !AnimationBlock;
      if(LEDAnimation == 5){
        //Animation 5 runs at a slower speed
        AnimationTime = millis64() + LEDBlinkTime;
      } else{
        AnimationTime = millis64() + LEDFlashTime;
      }
      //Set the animation block here;
      switch(LEDAnimation){
      case 0:
        //Flashing Red
        if(AnimationBlock){
          Red = 255;
          Green = 0;
          Blue = 0;
        } else{
          Red = 0;
          Green = 0;
          Blue = 0;
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
        if(AnimationBlock){
          Red = 255;
          Green = 255;
          Blue = 0;
        } else{
          Red = 0;
          Green = 0;
          Blue = 0;
        }
      break;
      case 5:
        //Cycle Green/White
        if(AnimationBlock){
          Red = 0;
          Green = 255;
          Blue = 0;
        } else{
          Red = 255;
          Green = 255;
          Blue = 255;
        }
      break;
      case 6:
        //Solid White
        Red = 255;
        Green = 255;
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
        if(AnimationBlock){
          Red = 0;
          Green = 0;
          Blue = 255;
        } else{
          Red = 0;
          Green = 0;
          Blue = 0;
        }
      break;
      }
      NewLED = 1;
    } else{
      //Animation doesn't need updating
      continue;
    }
  }
}