#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "io/LEDControl.hpp"
#include "network/network.hpp"
#include "network/usb.hpp"
#include <stdint.h>

static const char* TAG = "example";

extern "C" void app_main(void) {
    USB::init();
    led_init();
    set_led_state(LEDDisplayState::IDLE);
    Network::init();
}