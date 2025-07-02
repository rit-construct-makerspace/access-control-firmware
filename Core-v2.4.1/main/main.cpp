#include "io/IO.hpp"
#include "network/network.hpp"
#include "network/usb.hpp"

extern "C" void app_main(void) {
    USB::init();
    IO::init();
    //Network::init();
}
