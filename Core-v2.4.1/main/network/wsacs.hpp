#pragma once
#include "esp_err.h"
#include "common/types.hpp"

namespace WSACS {
    int init();
    esp_err_t send_message(WSACS::Event);
} // namespace WSACS