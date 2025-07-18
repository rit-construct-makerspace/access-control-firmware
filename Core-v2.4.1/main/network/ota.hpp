
#include "common/types.hpp"
#include <cstdint>

namespace OTA {
    void begin(OTATag tag);
    void finish();

    void mark_valid();
} // namespace OTA