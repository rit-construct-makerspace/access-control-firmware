#include "esp_err.h"
namespace HTTPManager {
    // Init HTTP to one server in particular
    // (we're not made of memory)
    void init(const char* server);

    using upload_cb_t = esp_err_t (*)(esp_err_t err, uint8_t* dst, int* len);
} // namespace HTTPManager