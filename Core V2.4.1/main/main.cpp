#include "io/LEDControl.hpp"

extern "C" void app_main(void) {
    LED::init();
    LED::set_state(LED::DisplayState::DENIED);
}
