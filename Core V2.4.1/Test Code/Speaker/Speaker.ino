#include <Arduino.h>
#include "XT_I2S_Audio.h"
#include "WavData.h"
#include "MusicDefinitions.h"

//    I2S
#define I2S_DOUT 14 // i2S Data out oin
#define I2S_BCLK 9 // Bit clock
#define I2S_LRC 16  // Left/Right clock, also known as Frame clock or word select


XT_I2S_Class I2SAudio(I2S_LRC, I2S_BCLK, I2S_DOUT, I2S_NUM_0);

void setup()
{
  Serial.begin(115200); // Used for info/debug
}

void loop()
{
  I2SAudio.Beep(1500, 250, 5);
  I2SAudio.Beep(1000, 250, 5);
  delay(1000);
}