#include "common/types.hpp"
#include <cstdint>

namespace OTA {
    void begin(OTATag tag);
    void feed_bytes(uint8_t* buf, std::size_t buflen);
    void finish();

} // namespace OTA