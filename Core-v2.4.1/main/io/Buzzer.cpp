#include "Buzzer.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

TaskHandle_t buzzer_thread;
#define BUZZER_TASK_STACK_SIZE 2000

void buzzer_task_fn(void *) {

}

int Buzzer::init() {
    
    xTaskCreate(buzzer_task_fn, "buzzer", BUZZER_TASK_STACK_SIZE, NULL, 0, &buzzer_thread);
    return 0;
}