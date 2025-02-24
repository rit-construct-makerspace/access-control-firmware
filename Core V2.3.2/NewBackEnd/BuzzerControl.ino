/*

Buzzer Control

This task is responsible for buzzer tones played to indicate certain states.

Melody 1:
* Used for happy situations, like a card being approved
* Low - High

Melody 2:
* Used for sad situations, like a card being denied
* High - Low

Melody 3:
* Used for error conditions, like an internal fault
* Low - Off - Low - Off - Low

Melody 4:
* Single beep for minor notifications
* High

Global Variables Used:

*/

#define HIGH_TONE 2000 //Hz
#define LOW_TONE 1500 //Hz

void BuzzerControl(void *pvParameters) {
  byte Melody;
  byte OldMelody;
  byte DonePlaying;
  byte MelodyStep;
  unsigned long MelodyTime = 0;
  while (1) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    //First, check the situations to see if we should be playing any tones right now:
    Melody = 0; //Set to 0 so if no situations apply, we stop playing
    if(State.equals("Unlocked")){
      Melody = 1;
    }
    if(CardVerified && !CardStatus && CardPresent){
      Melody = 2;
    }
    if(TemperatureFault || NFCFault || ReadError){
      ReadError = 0;
      Melody = 3;
    }
    if(VerifiedBeep){
      VerifiedBeep = 0;
      Melody = 4;
    }
    if(Melody == 0){
      //No tone to play
      Tone = 0;
      NewBuzzer = 1;
      continue;
    }
    if(Melody != OldMelody){
      //Melody has changed!
      OldMelody = Melody;
      DonePlaying = 0;
      NewBuzzer = 0;
      MelodyTime = 0;
      MelodyStep = 0;
    } else if((MelodyTime <= millis()) || DonePlaying){
      //Melody didn't change and it's not time to advance to the next tone or the system is done playing a tone
      continue;
    }
    //If we make it here, there is a tone to be changed.
    MelodyTime = millis() + BuzzerNoteTime; //Set the time to change the note again.
    switch (Melody){
      case 1:
        switch (MelodyStep){
          case 0:
            Tone = LOW_TONE;
          break;
          case 1:
            Tone = HIGH_TONE;
          break;
          case 2:
            Tone = 0;
            DonePlaying = 1;
          break;
        }
      break;
      case 2:
        switch (MelodyStep){
          case 0: 
            Tone = HIGH_TONE;
          break;
          case 1:
            Tone = LOW_TONE;
          break;
          case 2:
            Tone = 0;
            DonePlaying = 1;
          break;
        }
      break;
      case 3:
        switch (MelodyStep){
          case 0:
            Tone = LOW_TONE;
          break;
          case 1:
            Tone = 0;
          break;
          case 2:
            Tone = LOW_TONE;
          break;
          case 3:
            Tone = 0;
          break;
          case 4:
            Tone = LOW_TONE;
          break;
          case 5:
            Tone = 0;
            DonePlaying = 1;
          break;
        }
      break;
      case 4:
        switch(MelodyStep){
          case 0:
            Tone = HIGH_TONE;
          break;
          case 1:
            Tone = 0;
            DonePlaying = 1;
          break;
        }
      break;
    }
    NewBuzzer = 1;
    MelodyStep++;
  }
}