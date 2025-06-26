#include "io/LEDControl.hpp"
#include "network/network.hpp"
extern "C" void app_main(void) {
    Network::init();
    led_init();
    // set_led_state(LEDDisplayState::DENIED);
}
