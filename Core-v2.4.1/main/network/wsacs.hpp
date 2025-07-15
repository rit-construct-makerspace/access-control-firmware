#pragma once
#include "cJSON.h"
#include "common/types.hpp"
#include "esp_err.h"

namespace WSACS {
    esp_err_t try_connect();
    void send_opening_message();
    void send_keepalive_message();

    esp_err_t send_message(char*);
    esp_err_t send_cjson(cJSON*);
    void send_auth_request(const AuthRequest&);

} // namespace WSACS