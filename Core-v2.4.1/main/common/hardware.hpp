#include "types.hpp"

namespace Hardware {
    int init();
    const char* get_serial_number();
    HardwareEdition get_edition();
    const char* get_edition_string();
} // namespace Hardware
