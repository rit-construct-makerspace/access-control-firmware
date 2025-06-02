/* 

Internal Write

This task sends serial messages to the frontend based on flags

The frontend MCU responds to the following messages;
* L RED,GREEN,BLUE - This message sets the LED color to Red, Green, Blue values sent. The LED will stay this color/intensity until commanded otherwise.
* B FREQUENCY - Plays the requested frequency on the buzzer, will continue forever until commanded off with frequency = 0
* S (0/1) - Sets the switch output on or off, based on a 1 or 0
* D (0/1) - Sets the debug LED on or off
* P - Polls all states, will return all messages indicating the states of peripherals.

Global Variables Used:
DebugMode: Enables verbose output
PollCheck: Flag to do an all-call check of the frontend.
NewLED: Flag that there is new LED information to send
Red: Red channel of LED
Green: Green channel of LED
Blue: Blue channel of LED
NewBuzzer: Flag that there is new buzzer to send
Tone: Frequency of buzzer to play
Switch: Value of the switch to send to frontend
DebugLED: Turns on/off the debugLED on the frontend
*/

void InternalWrite(void *pvParameters) {
  xTaskCreate(PollTimer, "PollTimer", 1024, NULL, 2, NULL);
  while(1){
    //Check every 30mS
    vTaskDelay(3 / portTICK_PERIOD_MS);
    Internal.flush();
    //First, check for any of the flags indicating there's a new message to send;
    if(PollCheck){
      Internal.println("P");
      PollCheck = 0;
    }
    if(NewLED){
      //Check when the last time we updated the lighting was - we don't want to set it too often to prevent rapid flashes
      //333ms is 3Hz, the cutoff for epileptic triggers.
      if((LastLightChange + 333)  <= millis64()){
        LastLightChange = millis64();
        //Apply the brightness override here;
        byte DimRed = map(Red, 0, 255, 0, Brightness);
        byte DimGreen = map(Green, 0, 255, 0, Brightness);
        byte DimBlue = map(Blue, 0, 255, 0, Brightness);
        Internal.println("L " + String(DimRed) + "," + String(DimGreen) + "," + String(DimBlue));
        NewLED = 0;
      } else{
        if(DebugMode){
          Serial.println(F("Commanded to change LED to soon! Ignoring."));
        }
      }
    }
    if(NewBuzzer){
      Internal.println("B " + String(Tone));
      NewBuzzer = 0;
    }
    //Some items are constantly written and re-asserted.
    Internal.println("S " + String(Switch));
    Internal.println("D " + DebugLED);
    vTaskDelay(10 / portTICK_PERIOD_MS);  //Only run once every 10mS
  }
}

void PollTimer(void *pvParameters) {
  //This sub-task simply acts as a timer for the regular frontend checks.
  while(1){
    PollCheck = 1;
    vTaskDelay(FEPollRate / portTICK_PERIOD_MS);
  }
}