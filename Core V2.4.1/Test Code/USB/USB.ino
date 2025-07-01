//This code is used to test the custom USB CDC implementation

#include "USB.h"

USBCDC USBSerial;

void setup() {
  // put your setup code here, to run once:
  USB.PID(0x82D0);
  USB.firmwareVersion(0x220);
  USB.productName("ACS Core");
  USB.manufacturerName("SHED Makerspace");
  USB.serialNumber("2.4.1-LE");
  USB.begin();
  USBSerial.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  USBSerial.print(F("Test "));
  USBSerial.println(millis());
  delay(1000);

}
