//ACS Data Logger

//This code is used to read the serial debug information from an ACS Core.

#include "USB.h"
#include "USBCDC.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

USBCDC USBSerial;
HardwareSerial TXIn(1);
File file;

//Pins
#define RXPin 16
#define MISOPIN 6
#define CLKPIN 4
#define MOSIPIN 2
#define CSPIN 1

void setup() {
  // put your setup code here, to run once:

  delay(100);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  delay(100);
  USB.begin();
  USBSerial.begin();
  TXIn.begin(115200, SERIAL_8N1, RXPin, 18);
  USBSerial.println(F("Starting!"));
  SPI.begin(CLKPIN, MISOPIN, MOSIPIN);
  if(!SD.begin(CSPIN)){
    USBSerial.println(F("NO SD CARD FOUND!"));
    while(1){
      digitalWrite(15, LOW);
      delay(250);
      digitalWrite(15, HIGH);
      delay(250);
    }
  } else{
    USBSerial.println(F("Mounted SD Card."));
  }
  file = SD.open("/log.txt", FILE_APPEND);
  file.println("Startup");
  file.close();
  USBSerial.println(F("Setup Complete."));
  digitalWrite(15, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(TXIn.available() > 0){
    digitalWrite(15, HIGH);
    TXIn.setTimeout(5);
    while(TXIn.available() > 0){
      file = SD.open("/log.txt", FILE_APPEND);
      String temp = TXIn.readString();
      file.println(temp);
    }
    file.close();
    digitalWrite(15, LOW);
  }
}
