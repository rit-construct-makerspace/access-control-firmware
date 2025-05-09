// PlayWAV - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain December 2024.
//
// When BOOTSEL preseed, plays a small WAV file from ROM
// asynchronously with a single call.
// Hook up an earphone to pins 0, 1, and GND to hear the PWM output.
//
// Generate the ROM file by using "xxd -i file.wav file.h" and then
// editing the output header to make the array "const" so it stays
// only in flash.
//
// Intended as a simple demonstration of BackgroundAudio usage.

#include <BackgroundAudioWAV.h>
#include <WavData.h>

#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(9, 12, 14); // BCLK, LRCLK, DOUT (,MCLK)

ROMBackgroundAudioWAV BMP(audio);

void setup() {
  // Start the background player
  BMP.begin();
  BMP.setGain(0.5);

  //Write the file in the setup, so when the loop hits there's no delay.
  BMP.write(file, sizeof(file));
}

uint32_t last = 0;
void loop() {
  if (millis() - last > 40000) {
    Serial.printf("Runtime: %lu, %d\r\n", millis(), audio.frames());
    last = millis();
    BMP.flush(); // Stop any existing output, reset for new file
    BMP.write(file, sizeof(file));
  }
}
