#include "io/LEDControl.hpp"

extern "C" void app_main(void) {
    led_init();
    set_led_state(LEDDisplayState::DENIED);
}
