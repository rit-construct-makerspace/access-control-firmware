/*

Access Control Frontend Firmware

Version 1.2.0 for hardware version 2.3.2-x

This is the firmware that runs on the frontend MCU. This MCU handles anything time-critical for a good user experience.
Originally this MCU was the master of the system with the ESP32 acting as a network connection bridge, but now the ESP32 is the master
As such, this firmware turns the frontend MCU into a glorified IO expander, allow OTA updates on the ESP32 to change how the frontend works.

The frontend MCU responds to the following messages;
* L RED,GREEN,BLUE - This message sets the LED color to Red, Green, Blue values sent. The LED will stay this color/intensity until commanded otherwise.
* B FREQUENCY - Plays the requested frequency on the buzzer, will continue forever until commanded off with frequency = 0
* S (0/1) - Sets the switch output on or off, based on a 1 or 0
* D (0/1) - Sets the debug LED on or off
* P - Polls all states, will return all messages indicating the states of peripherals.

The frontend MCU sends the following messages;
* B (0/1) - Sends 1 when the button is pressed, 0 when it is released.
* S1 (0/1) - Sends 1 when the first card detect is pressed, 0 when it is released.
* S2 (0/1) - Sends 1 when the second card detect is pressed, 0 when it is released.
* K1 (0/1) - Sends the state of the first key switch pin when it changes.
* K2 (0/1) - Sends the state of the second key swtich pin when it changes.
* V (Version) - Sends the version of the frontend firmware

*/

#define Version "1.2.0"

#include <tinyNeoPixel.h> //Inherent to MegaTinyCore

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

tinyNeoPixel LED = tinyNeoPixel(1, LED_DATA, NEO_GRB);

//Variables:
byte Red; //Red channel of RGB LED
byte Green; //Green channel of RGB LED
byte Blue; //Blue channel of RGB LED
int Tone; //Frequency of buzzer
bool ACSSwitch; //Output to the ACS switch interface
bool DebugLED; //Output to the internal debug LED
bool Button; //State of front button
bool Card1; //State of card switch 1
bool Card2; //State of card switch 2
bool Key1; //State of key input 1
bool Key2; //State of key input 2

void setup() {
  // put your setup code here, to run once:

  //Startup the LEDs
  LED.begin();
  LED.clear();
  LED.show();

  //Set PinModes
  pinMode(FE_DEBUG, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(NO, OUTPUT);
  digitalWrite(NO, LOW);
  
  //Start serial:
  Serial.swap(1);
  Serial.begin(115200);

  //Read initial states;
  delay(100); //Wait for debounce circuits to set up
  Button = digitalRead(BUTTON);
  Card1 = digitalRead(SWITCH1);
  Card2 = digitalRead(SWITCH2);
  Key1 = digitalRead(KEY1);
  Key2 = digitalRead(KEY2);

}

void loop() {
  // put your main code here, to run repeatedly:

  //Check for any changes to states of peripherals. 
  if(digitalRead(BUTTON) != Button){
    Button = digitalRead(BUTTON);
    Serial.print("B "); Serial.println(Button);
    Serial.flush();
  }
  if(digitalRead(SWITCH1) != Card1) {
    Card1 = digitalRead(SWITCH1);
    Serial.print("S1 "); Serial.println(Card1);
    Serial.flush();
  }
  if(digitalRead(SWITCH2) != Card2) {
    Card2 = digitalRead(SWITCH2);
    Serial.print("S2 "); Serial.println(Card2);
    Serial.flush();
  }
  if(digitalRead(KEY1) != Key1) {
    Key1 = digitalRead(KEY1);
    Serial.print("K1 ");
    Serial.println(Key1);
    Serial.flush();
  }
  if(digitalRead(KEY2) != Key2) {
    Key2 = digitalRead(KEY2);
    Serial.print("K2 ");
    Serial.println(Key2);
    Serial.flush();
  }

  //Check for any new messages;
  if(Serial.available()){
    String incoming = Serial.readStringUntil('\n');
    incoming.trim();
    //Process the message:
    switch (incoming.charAt(0)){
      case 'L':
        //Set LED colors
        //L RRR,GGG,BBB
        //012??????????
        //Since numbers can be different lengths, need to search and find by commas
        Red = incoming.substring(2, incoming.indexOf(',')).toInt();
        incoming.remove(0, incoming.indexOf(',')+1);
        Green = incoming.substring(0, incoming.indexOf(',')).toInt();
        incoming.remove(0, incoming.indexOf(',')+1);
        Blue = incoming.toInt();
        LED.setPixelColor(0, Red, Green, Blue);
        LED.show();
      break;
      case 'B':
        //Set buzzer frequency
        //B FFFFFFFFF
        //0123...
        Tone = incoming.substring(2).toInt();
        if(Tone == 0){
          noTone(BUZZER);
        } else{
          tone(BUZZER, Tone);
        }
      break;
      case 'S':
        //Set switch on/off
        //S M
        //012
        if('1' == incoming.charAt(2)){
          ACSSwitch = 1;
        } else{
          ACSSwitch = 0;
        }
        digitalWrite(NO, ACSSwitch);
      break;
      case 'D':
        //Set debug LED on/off
        //D M
        //012
        if('1' == incoming.charAt(2)){
          DebugLED = 1;
        } else{
          DebugLED = 0;
        }
        digitalWrite(FE_DEBUG, DebugLED);
      break;
      case 'P':
        //Report all states
        Serial.print("B "); Serial.println(Button);
        Serial.print("S1 "); Serial.println(Card1);
        Serial.print("S2 "); Serial.println(Card2);
        Serial.print("K1 "); Serial.println(Key1);
        Serial.print("K2 "); Serial.println(Key2);
        Serial.print("V "); Serial.println(Version);
        Serial.flush();
      break;
    }
  }
}
