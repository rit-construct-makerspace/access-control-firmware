#include "WavData.h"

#include <ESP_I2S.h>

I2SClass i2s;

void setup() {
  // put your setup code here, to run once:
  i2s.setPins(9, 16, 14, -1);
  i2s.begin(I2S_MODE_STD, 8000, I2S_DATA_BIT_WIDTH_8BIT, I2S_SLOT_MODE_MONO);
  i2s.configureTX(8000, I2S_DATA_BIT_WIDTH_8BIT,  I2S_SLOT_MODE_MONO);
}

void loop() {
  // put your main code here, to run repeatedly:
  i2s.playWAV(AccessGranted, 20306);
  delay(5000);
}
