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

USBCDC USBSerial;

#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(9, 12, 14); // BCLK, LRCLK, DOUT (,MCLK)

#include "USB.h"

ROMBackgroundAudioWAV BMP(audio);

unsigned int track;

QueueHandle_t queue;

void setup() {
  // Start the background player
  USB.begin();
  USBSerial.begin();
  BMP.begin();
  BMP.setGain(0.50);

  BMP.write(ACSStarting, sizeof(ACSStarting));

  xTaskCreate(Speaker, "Speaker", 2048, NULL, 5, NULL);

}

void loop() {
  USBSerial.setTimeout(3);
  if(USBSerial.available() > 0){
    String temp = USBSerial.readStringUntil(',');
    unsigned int tosend = temp.toInt();
    xQueueSend(queue, (void *)&tosend, 0);
  }
}

void Speaker(void *pvParameters){

  queue = xQueueCreate(10, sizeof(track));
  while(1){
    delay(1);
    //Wait for the current audio to stop playing;
    while(!BMP.done()){
      delay(1);
    }
    //There is no audio playing, check for new audio.
    xQueueReceive(queue, &track, (TickType_t)5);
    if(track != 0){
      USBSerial.print(F("Playing Track: "));
      USBSerial.println(track); 
      BMP.flush();
    }
    switch (track){
      case 0:
        break;
      case 1:
        BMP.write(AccessDenied, sizeof(AccessDenied));
        break;
      case 2:
        BMP.write(AccessGranted, sizeof(AccessGranted));
        break;
      case 3:
        BMP.write(AccountHold, sizeof(AccountHold));
        break;
      case 4:
        BMP.write(ACSStarting, sizeof(ACSStarting));
        break;
      case 5:
        BMP.write(Alert, sizeof(Alert));
        break;
      case 6:
        BMP.write(AlwaysOn, sizeof(AlwaysOn));
        break;
      case 7:
        BMP.write(CardNotRecognized, sizeof(CardNotRecognized));
        break;
      case 8:
        BMP.write(CardReadError, sizeof(CardReadError));
        break;
      case 9:
        BMP.write(Eight, sizeof(Eight));
        break;
      case 10:
        BMP.write(Error, sizeof(Error));
        break;
      case 11:
        BMP.write(Fifteen, sizeof(Fifteen));
        break;
      case 12:
        BMP.write(Five, sizeof(Five));
        break;
      case 13:
        BMP.write(Four, sizeof(Four));
        break;
      case 14:
        BMP.write(HardwareFault, sizeof(HardwareFault));
        break;
      case 15:
        BMP.write(IdleMode, sizeof(IdleMode));
        break;
      case 16:
        BMP.write(Lockout, sizeof(Lockout));
        break;
      case 17:
        BMP.write(MachineLockout, sizeof(MachineLockout));
        break;
      case 18:
        BMP.write(MachineUnlocked, sizeof(MachineUnlocked));
        break;
      case 19:
        BMP.write(Minutes, sizeof(Minutes));
        break;
      case 20:
        BMP.write(MissingSignIn, sizeof(MissingSignIn));
        break;
      case 21:
        BMP.write(MissingStaffApproval, sizeof(MissingStaffApproval));
        break;
      case 22:
        BMP.write(MissingTrainings, sizeof(MissingTrainings));
        break;
      case 23:
        BMP.write(ModeChangedTo, sizeof(ModeChangedTo));
        break;
      case 24:
        BMP.write(NetworkError, sizeof(NetworkError));
        break;
      case 25:
        BMP.write(Nine, sizeof(Nine));
        break;
      case 26:
        BMP.write(One, sizeof(One));
        break;
      case 27:
        BMP.write(Overtemperature, sizeof(Overtemperature));
        break;
      case 28:
        BMP.write(PleaseSpeakToAMemberOfStaff, sizeof(PleaseSpeakToAMemberOfStaff));
        break;
      case 29:
        BMP.write(Proceed, sizeof(Proceed));
        break;
      case 30:
        BMP.write(Reason, sizeof(Reason));
        break;
      case 31:
        BMP.write(RemoteLockout, sizeof(RemoteLockout));
        break;
      case 32:
        BMP.write(Restarting, sizeof(Restarting));
        break;
      case 33:
        BMP.write(SessionEnded, sizeof(SessionEnded));
        break;
      case 34:
        BMP.write(Seven, sizeof(Seven));
        break;
      case 35:
        BMP.write(Six, sizeof(Six));
        break;
      case 36:
        BMP.write(Ten, sizeof(Ten));
        break;
      case 37:
        BMP.write(Thirty, sizeof(Thirty));
        break;
      case 38:
        BMP.write(Three, sizeof(Three));
        break;
      case 39:
        BMP.write(Two, sizeof(Two));
        break;
      case 40:
        BMP.write(UpdateInProgress, sizeof(UpdateInProgress));
        break;
      case 41:
        BMP.write(Warning, sizeof(Warning));
        break;
      case 42:
        BMP.write(Welcome, sizeof(Warning));
        break;
    }
  }
}
