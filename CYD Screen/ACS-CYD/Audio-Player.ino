/*

Audio Player

Handles playing of tones and audio, either internally-developed or from SD card.


void audioPlayer(void *pvParameters){

  //Start I2C and I2S
  //Wire.begin(I2C_SDA, I2C_SCL);
  audio.setPinout(I2S_SCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK);
  audio.setVolume(volume);

  String lastState = state;

  while(1){

    delay(20);

    //Check for things that should result in a tone playing
    if (lastState != state){
      lastState = state;
      if (state == "UNLOCKED"){
        //Play an unlocking approved beep
        audio.connectToFS(SD_MMC, "beep.wav");
      }
      else if (isDenied = true){
        //Denied, play a disapproved beep.
        audio.connectToFS(SD_MMC, "denied.wav");
      }
      else {
        //Play a single beep
        audio.connectToFS(SD_MMC, "beep.wav");
      }
    }

  }

}

*/