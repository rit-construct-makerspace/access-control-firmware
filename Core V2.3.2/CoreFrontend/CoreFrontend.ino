#define Version "V232-FE-250117"

/* 

----- ACCESS CONTROL CORE: FRONTEND FIRMWARE -----

For hardware version 2.3.2

Written by Jim Heaney, RIT SHED Makerspace

Licensed CC-BY-SA-NC 4.0

Documentation: https://github.com/rit-construct-makerspace/access-control-hardware

This firmware is intended to run on the 8 bit microcontroller handling the frontend of the Access Control Core. 

Theory of Operation: n 
V2.3.2 of the Access Control core implemented many improvements discovered in testing previous iterations. One of these changes that manifested
was dividing the Core's duties into a lightweight frontend system, and a blocking, slower backend system. These two would then communicate over
UART to work in unison, with the frontend serving the role of master when needed.  

The frontend handles anything and everything that someone physically standing in front of the machine can perceive. This focus allows an increase
in responsiveness, and will lead to a better user experience. Blocking, long-duration tasks (like sending HTTPS messages) is left to the backend.

The frontend is responsible for the key switch and button on the front interface, the indicator light and buzzer, the card detection switches,
and the switch signal for the connected Access Control Switches. 

The backend is reponsible for managing and reading the NFC reader, gathering diagnostic data on the state of the system, and sending API requests to the server. 

*/

//TODO:
  //Minor bug that you can get the lights to go yellow by inserting and removing a card during startup

//Pin Definitions:
#define UART_MODE PIN_PA4
  //If UART_MODE is high, AtTiny re-assigns location of RX/TX to communicate with ESP32. By default and/or with no special code running, UART goes to USB
#define DIP5 PIN_PA5
  //DIP5 is a generic settings option, at the time of writing it is used to disable/enable the buzzer.
#define DIP4 PIN_PA6
  //DIP4 is a generic settings option, at the time of writing it is unused.
#define BUTTON PIN_PB5
  //Button is the normally open signal from the help button on the front of the core. The debounce circuitry makes this negative logic.
#define KEY1 PIN_PB4
  //Key1 is one of the two circuits used to read the override key switch.
//TINY_RX is on PIN_PB3
  //This is the default position of UART, so the system will default to debug UART unless programmed otherwise.
//TINY_TX is on PIN_PB2
  //This is the default position of UART, so the system will default to debug UART unless programmed otherwise.
#define KEY2 PIN_PB1
  //Key2 is one of the two circuits used to read the override key switch.
#define BUZZER PIN_PA3
  //Buzzer is a passive buzzer driven with an NPN.
//HV_RX is on PIN_PA2
  //This is the alternative position of UART, so programatically we can swap from debug UART to ESP32 internal UART.
//HV_TX is on PIN_PA1
  //This is the alternative position of UART, so programatically we can swap from debug UART to ESP32 internal UART.
//UPDI is on PIN_PA0
  //UPDI connects to an internal UPDI programmer, built around a CH340C.
#define FE_DEBUG PIN_PC3
  //FE_DEBUG is an onboard red LED for debug purposes, mounted near the buzzer.
#define SWITCH1 PIN_PC2
  //This is one of the two switches used to detect if there is a card present. Due to the debounce circuitry, it is negative logic. 
#define LED_DATA PIN_PC1
  //LED_DATA is the WS2812-style serial data output for the LEDs in the help button.
#define SWITCH2 PIN_PC0
  //This is one of the two switches used to detect if there is a card present. Due to the debounce circuitry, it is negative logic.   
#define NO PIN_PB0
  //NO is the normally open signal that drives the access switches. Writing it high will turn on the attached electronics.

//Libraries:
#include <tinyNeoPixel.h>
#include <TaskScheduler.h>

//Global Variables:
bool debug = 1; 
  //If debug is 1, more verbose messages are sent. This is turned on automatically when the UART_MODE is set to debug routing.
bool OldKey1 = 0;
bool OldKey2 = 0;
bool OldButton = 0;
byte OldState = 0;
bool OldCard1 = 0;
bool OldCard2 = 0;
  //All the above "Old" variables save the last state of these, to detect changes and act on them.
byte State = 0;
  //Statae stores the current state of the system;
    //0 is idle
    //1 is overriden on
    //2 is locked out
byte NewState = 0;
  //Stores the state temporarily while being set by the override, then
bool IsConnected = 1;
  //Stores if we are currently connected to the network, if not it will override the lights and ignore card requests.
byte LEDAnimation = 1;
  //The style of animation currently played by the animation task.
byte Step = 0;
  //What step in the animation cycle the system is in for the LED.
byte BuzzerSequence = 0;
  //The style of buzzer sequence played by the task.
byte BuzzerStep = 0;
  //What step in the buzzer sequence the system is in.
bool Unlocked = 0;
  //1 if the machine is unlocked by a keycard (e.g. in idle state);
bool CardPresent = 0;
  //1 if there is a card present in the system currently.
bool GracePeriod = 0;
  //Set to 1 if a card has been removed, and we are currently in the grace period.
unsigned long GraceTime = 0;
  //What time, in millis, beyond which we should treat the card as properly removed.
bool Help = 0;
  //When 1, indicates that the system is in the request help state.
bool NoBuzzer = 0;
  //If 1, the buzzer is not working.
bool Starting = 1;
  //Set to 0 when startup is done, to indicate to start functioning normally.
bool isvalid = 0;
  //1 when a valid ID card is present, for key switch stuff

//Objects:
Scheduler scheduler;
tinyNeoPixel LED = tinyNeoPixel(1, LED_DATA, NEO_GRB);

//Prototype Functions:
void UpdateLED();
  //Handles task-based animations for the LED
void UpdateBuzzer();
  //Handles task-based animations for the Buzzer
void ReceiveSerial();
  //Called when serial data is detected, to read in, parse, and act on.

//Tasks:
//Tasks are used to run the LED and Buzzer animations. Animations are broken into 200ms blocks, and every time the task is called, an animation step is incremented by 1. 
Task TaskUpdateLED(200, TASK_FOREVER, &UpdateLED);
Task TaskUpdateBuzzer(200, TASK_FOREVER, &UpdateBuzzer);

void setup() {
  // put your setup code here, to run once:
  //Check the state of UART_MODE. If LOW, route UART properly and disable debug mode.
  if(digitalRead(UART_MODE)){
    Serial.swap(1);
  }
  if(digitalRead(DIP5)){
    NoBuzzer = 1;
  }
  Serial.begin(115200);
  if(debug){
    Serial.println(F("Entering Debug Mode!"));
    Serial.println(F("This means that normal serial data to the ESP32 will not be sent."));
  }

  //Startup the LEDs
  LED.begin();

  //Initialize the scheduler and tasks
  scheduler.init();
  scheduler.addTask(TaskUpdateLED);
  TaskUpdateLED.enable();
  scheduler.addTask(TaskUpdateBuzzer);
  TaskUpdateBuzzer.enable();

  //Set pinmodes
  pinMode(FE_DEBUG, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(NO, OUTPUT);
  digitalWrite(NO, LOW);

  //Read the initial state of the key switch, card switch, and buzzer;
  OldKey1 = digitalRead(KEY1);
  OldKey2 = digitalRead(KEY2);
  OldButton = digitalRead(BUTTON);
  OldCard1 = digitalRead(SWITCH1);
  OldCard2 = digitalRead(SWITCH2);

  //Set the initial state of the system based on the switch read;
  if((digitalRead(KEY1)) && (digitalRead(KEY2))){
    //Locked on
    State = 1;
  }
  if(!digitalRead(KEY1)){
    //Locked off
    State = 2;
  }
  if(!digitalRead(KEY2)){
    //Normal mode - let startup run as normal
  }

  if(debug){
    Serial.print(F("Initial state set to: ")); Serial.println(State);
  }

  SetAccess();
  ResetLED();

  if(debug){
    Serial.println(F("Setup complete!"));
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  scheduler.execute();

  //Force the LED to flash blue if waiting for help
  if(Help && IsConnected){
    LEDAnimation = 7;
  }

  //Read the state of the key switches, but only if there is a valid ID present
  if((OldKey1 != digitalRead(KEY1)) || (OldKey2 != digitalRead(KEY2))){
    //One of the key switch pins is the not in the right state. Something must have changed. 
    if(debug){
      Serial.println(F("Change to key switch detected."));
    }
    OldKey1 = digitalRead(KEY1);
    OldKey2 = digitalRead(KEY2);

    //Determine what the new state is, set the NewState variable
    if((digitalRead(KEY1)) && (digitalRead(KEY2)) && isvalid){
      //Locked on, can only enter this state if there is a valid card present.
      NewState = 1;
    }
    if(!digitalRead(KEY1)){
      //Locked off
      NewState = 2;
    }
    if(!digitalRead(KEY2) && isvalid){
      //Normal mode, can only enter if there is a valid card present.
      NewState = 0;
    }
    State = NewState;
    SetAccess();
    if(debug){
      Serial.print(F("State Set: ")); Serial.println(NewState);
    }
  } 

  //Check for a key card with switches
  bool CardCheck = (!digitalRead(SWITCH1) && !digitalRead(SWITCH2));
  if(CardCheck != CardPresent){
    //The state is different than last loop, let's see what's up
    CardPresent = CardCheck;
    if(CardCheck == 1){
      if(debug){
        Serial.println(F("New Card Detected."));
      }

      if(State == 0 && IsConnected == 1){
        //Authenticate the card, since we are idle and not offline
        CardPresent = 1;
        Serial.println(F("a"));
        LEDAnimation = 8; //Blinking yellow
      }
      else{
        //If not in the right state and a card is inserted, buzz a warning/error message. But still tell the backend there is a card present
        Serial.println(F("p"));
        BuzzerSequence = 2;
      }
    }
    if(CardCheck == 0){
      //A card was removed
      CardPresent = 0;
      isvalid = 0;
      Unlocked = 0;
      Serial.println("r");
      SetAccess();
    }
  }

  //Check for any messagesd from the ESP32
  if(Serial.available() > 0){
    readagain:
    //New serial message in buffer
    Serial.setTimeout(1);
    String incoming = Serial.readStringUntil('\n');
    if(debug){
      Serial.print(F("Received Message: "));
      Serial.println(incoming);
    }
    //Process the message;
    switch (incoming.charAt(0)){
      case 'q':
        //valid ID card found
        if(CardPresent){
          isvalid = 1;
        }
      break;
      case 'a':
        //Authorization Granted
        if(State == 0 && CardPresent == 1){
          Unlocked = 1;
          Serial.println("u,1");
          LEDAnimation = 3;
          BuzzerSequence = 1;
          BuzzerStep = 0;
          SetAccess();
        }
      break;
      case 'd':
        //Authorization Denied
        if(CardPresent){
          LEDAnimation = 6;
          BuzzerSequence = 3;
          BuzzerStep = 0;
        }
      break;
      case 'l':
        //lockout command, likely due to a temperature issue
        State = 2;
        Unlocked = 0;
        BuzzerSequence = 2;
        BuzzerStep = 0;
        LEDAnimation = 6;
        Step = 0;
        digitalWrite(NO, LOW);
        if(debug){
          Serial.println(F("Received remote shutdown command!"));
        }
      break;
      case 'r':
        //Request for status
        Serial.print("s,"); Serial.println(State);
        Serial.print("u,"); Serial.println(Unlocked);
        Serial.print("h,"); Serial.println(Help);
      break;
      case 'i':
        //Disconnected from the server
        IsConnected = 0;
      break;
      case 'c':
        //Re-connected to the server
        IsConnected = 1;
        ResetLED();
      break;
      case 'g':
        //Put the lights in "gamer mode", used to indicate a restart or similar. 
        LEDAnimation = 1;
        Step = 0;
      break;
      case 's':
        //Turn off the lights on the front screen, to enter "sleep mode"
        if(Unlocked == 0){
          LEDAnimation = 0;
        }
      break;
      case 'w':
        //Exit sleep mode, set things how they should be.
        //Can also be used when exiting startup or wakeup.
        //Read the state of the key switch to set an initial state;
        Starting = 0;
        if((OldKey1 != digitalRead(KEY1)) || (OldKey2 != digitalRead(KEY2))){
          //One of the key switch pins is the not in the right state. Something must have changed. 
          if(debug){
            Serial.println(F("Change to key switch detected."));
          }
          OldKey1 = digitalRead(KEY1);
          OldKey2 = digitalRead(KEY2);

          //Determine what the new state is, set the NewState variable
          if((digitalRead(KEY1)) && (digitalRead(KEY2))){
            //Locked on
            NewState = 1;
          }
          if(!digitalRead(KEY1)){
            //Locked off
            NewState = 2;
          }
          if(!digitalRead(KEY2)){
            //Normal mode
            NewState = 0;
          }
          State = NewState;
          if(debug){
            Serial.print(F("New State Set: ")); Serial.println(State);
          }
        }
        SetAccess();
        ResetLED();
      break;
      case 'v':
        //Report the version of the firmware
        Serial.print("v,"); Serial.println(Version);
      break;
    }
    //Check if there is another message in the buffer;
    if(Serial.available() > 0){
      //Make sure it is not the same message.
      if(Serial.peek() == incoming.charAt(0)){
        //Same message character, flush the buffer.
        while(Serial.available()){
          Serial.read();
        }
      }
      else{
        //Read the new message
        goto readagain;
      }
    }
  }
}

void UpdateLED(){
  //Handles playing LED animations based on an animation value.

  //If the server is not connected, override all but animations 2, 3, and 10 to white.
  if(!IsConnected && (LEDAnimation != 2 && LEDAnimation != 3 && LEDAnimation != 10 && LEDAnimation != 12 && LEDAnimation != 11)){
    LED.setPixelColor(0, 255, 255, 255);
    LED.show();
    return;
  }
  //If help is requested, and we are connected, override the LED to blinking blue.
  if(Help && IsConnected){
    LEDAnimation = 7;
  }
  if(!IsConnected && LEDAnimation == 2){
    //Set special oscillating red/white setting;
    LEDAnimation = 11;
  }
  if(!IsConnected && LEDAnimation == 3){
    //Set special oscillating green/white setting:
    LEDAnimation = 12;
  }
  //Animation 0: No lights
  if(LEDAnimation == 0){
    LED.setPixelColor(0, 0, 0, 0);
  }
  //Animation 1 (Default): Cycle RGB
  if(LEDAnimation == 1){
    switch(Step){
      case 0:
        LED.setPixelColor(0, 255, 0, 0);
        Step++;
        break;
      case 1:
        LED.setPixelColor(0, 0, 255, 0);
        Step++;
        break;
      case 2:
        LED.setPixelColor(0, 0, 0, 255);
        Step = 0;
        break;
    }
  }
  //Animation 2: Solid Red
  if(LEDAnimation == 2){
    LED.setPixelColor(0, 255, 0, 0);
  }
  //Animation 3: Solid Green
  if (LEDAnimation == 3) {
    LED.setPixelColor(0, 0, 255, 0);
  }
  //Animation 4: Solid Blue
  if (LEDAnimation == 4) {
    LED.setPixelColor(0, 0, 0, 255);
  }
  //Animation 5: Solid Yellow
  if (LEDAnimation == 5) {
    LED.setPixelColor(0, 255, 255, 0);
  }
  //Animation 6: Blinking 50% Red
  if (LEDAnimation == 6) {
    switch (Step) {
      case 0:
        LED.setPixelColor(0, 0, 0, 0);
        Step++;
        break;
      case 1:
        LED.setPixelColor(0, 255, 0, 0);
        Step = 0;
        break;
    }
  }
  //Animation 7: Blinking 50% Blue
  if (LEDAnimation == 7) {
    switch (Step) {
      case 0:
        LED.setPixelColor(0, 0, 0, 0);
        Step++;
        break;
      case 1:
        LED.setPixelColor(0, 0, 0, 255);
        Step = 0;
        break;
    }
  }
  //Animation 8: Blinking 50% Yellow
  if (LEDAnimation == 8) {
    switch (Step) {
      case 0:
        LED.setPixelColor(0, 0, 0, 0);
        Step++;
        break;
      case 1:
        LED.setPixelColor(0, 255, 255, 0);
        Step = 0;
        break;
    }
  }
  //Animation 9: Turn red for a bit, then back to yellow.
  if (LEDAnimation == 9) {
    Step++;
    switch (Step) {
      case 1:
        LED.setPixelColor(0, 255, 0, 0);
        break;
      case 2:
        LED.setPixelColor(0, 255, 0, 0);
        break;
      case 3:
        LED.setPixelColor(0, 255, 255, 0);
        Step++;
        break;
      case 4:
        ResetLED();
        break;
    }
  }
  //Animation 10: Solid purple
  if(LEDAnimation == 10){
    LED.setPixelColor(0, 128, 0, 128);
  }
  //Animation 11: Oscillating red/white
  if(LEDAnimation == 11){
    Step++;
    if(Step < 10){
      LED.setPixelColor(0, 255, 0, 0);
    }
    if(Step > 10){
      LED.setPixelColor(0, 255, 255, 255);
    }
    if(Step == 20){
      Step = 0;
    }
  }
  //Animation 11: Oscillating red/white
  if(LEDAnimation == 12){
    Step++;
    if(Step < 10){
      LED.setPixelColor(0, 0, 255, 0);
    }
    if(Step > 10){
      LED.setPixelColor(0, 255, 255, 255);
    }
    if(Step == 20){
      Step = 0;
    }
  }
  LED.show();
}

void UpdateBuzzer(){
 //Plays a series of tones on the buzzer when instructed
  if(NoBuzzer){
    return;
  }
  //If BuzzerSequence is 0, nothing to play.
  if(BuzzerSequence == 0){
    noTone(BUZZER);
  }
  //Buzzer 1: Approved "Happy" Sound
  if(BuzzerSequence == 1){
    switch (BuzzerStep){
      case 0:
        tone(BUZZER, 1500);
        BuzzerStep++;
        break;
      case 1:
        noTone(BUZZER);
        BuzzerStep++;
        break;
      case 2:
        tone(BUZZER, 2000);
        BuzzerStep++;
        break;
      case 3:
        noTone(BUZZER);
        BuzzerStep = 0;
        BuzzerSequence = 0;
        break;
    }
  }
  //Buzzer 2: Error "Sad" Sound
  if (BuzzerSequence == 2) {
    switch (BuzzerStep) {
      case 0:
        tone(BUZZER, 1500);
        BuzzerStep++;
        break;
      case 1:
        noTone(BUZZER);
        BuzzerStep++;
        break;
      case 2:
        tone(BUZZER, 1000);
        BuzzerStep++;
        break;
      case 3:
        noTone(BUZZER);
        BuzzerStep = 0;
        BuzzerSequence = 0;
        break;
    }
  }
  //Buzzer 3: Denied Sound
  if (BuzzerSequence == 3) {
    switch (BuzzerStep) {
      case 0:
        tone(BUZZER, 1000);
        BuzzerStep++;
        break;
      case 1:
        noTone(BUZZER);
        BuzzerStep++;
        break;
      case 2:
        tone(BUZZER, 1000);
        BuzzerStep++;
        break;
      case 3:
        noTone(BUZZER);
        BuzzerStep++;
        break;
      case 4:
        tone(BUZZER, 1000);
        BuzzerStep++;
        break;
      case 5:
        noTone(BUZZER);
        BuzzerStep = 0;
        BuzzerSequence = 0;
        break;
    }
  }
}

void ResetLED(){
  //Returns the front panel LED to the proper state, based on the State and Unlocked variables
  if(Starting){
    LEDAnimation = 1;
    return;
  }
  if(LEDAnimation == 9 && Step <= 3){
    return;
  }
  if(Help){
    LEDAnimation = 7;
    return;
  }
  switch (State){
    case 0:
      //Idle
      if(Unlocked){
        LEDAnimation = 3; //Solid Green
      } else{
        LEDAnimation = 5; //Solid Yellow
      }
    break;
    case 1:
      //Always On
      LEDAnimation = 3; //Solid Green
    break;
    case 2:
      //Locked Out
      LEDAnimation = 2; //Solid Red
    break;
  }
  UpdateLED();
}

void SetAccess(){
  //Sets the access switch properly according to state.
  if(Unlocked == 1 || State == 1){
    digitalWrite(NO, HIGH);
    if(debug){
      Serial.println(F("Machine Unlocked."));
    }
  } else{
    digitalWrite(NO, LOW);
    if(debug){
      Serial.println(F("Machine Locked."));
    }
  }
  ResetLED();
}