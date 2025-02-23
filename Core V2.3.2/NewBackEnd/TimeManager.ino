/*

Time Manager

The time manager handles anything that has to happen with respect to real-world time. 

Currently, this is only an automatic restart of the system every Sunday at 4am local.
This reset will make sure that software updates are pulled weekly, as well as that millis() never overflows.


*/

void TimeManager(void *pvParameters) {
  while (1) {
    vTaskDelay(1800000 / portTICK_PERIOD_MS); //Only need to run this task once every 30 minutes
    if(millis() < 3600000){
      //The device has been on for less than an hour, no need to restart.
      continue;
    }
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      //Time server hasn't synced yet.
      continue;
    }
    char cstrHour[3];
    strftime(cstrHour, 3, "%H", &timeinfo);
    char cstrWeekDay[2];
    strftime(cstrWeekDay, 10, "%w", &timeinfo);
    if((cstrHour[0] == '0') && (cstrHour[1] == '4') && (cstrWeekDay[0] == 0) && (Switch == 0)){
      //The time is 4:xx AM on a Sunday, and the machine is not currently in use
      //Restart the system to check for software updates and reset millis();
      ESP.restart();
    }
  }
}