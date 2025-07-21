
#include "common/types.hpp"
#include <cstdint>

namespace OTA {
    void begin(OTATag tag);
    void mark_valid();

    void init();
    std::string running_app_version();
    std::string next_app_version();
    
} // namespace OTA