#include "types.hpp"

namespace Hardware {
    int identify();
    const char* get_serial_number();
    HardwareEdition get_edition();
    const char* get_edition_string();
} // namespace Hardware
