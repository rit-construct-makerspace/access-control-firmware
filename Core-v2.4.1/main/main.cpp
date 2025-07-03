#include "io/IO.hpp"
#include "network/network.hpp"
#include "network/usb.hpp"
#include "common/types.hpp"
#include "driver/gpio.h"
#include "common/pins.hpp"

HardwareEdition edition;

HardwareEdition get_hardware_edition() {
    return edition;
};

extern "C" void app_main(void) {

    gpio_input_enable(MODE);

    edition = (HardwareEdition) gpio_get_level(MODE);

    USB::init();
    IO::init();
    Network::init();
}
