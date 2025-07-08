#pragma once
#include "common/types.hpp"
#include "esp_err.h"
#include "cJSON.h"

namespace WSACS {
    struct AuthResponse{
        CardTagID user;
        IOState to_state;
        bool verified;
    };
    // Network -> Websocket
    enum class EventType {
        TryConnect,     // sent when we're online
        KeepAliveTimer, // sent every 10s if online to tell server our state

        AuthRequest,
        GenericJSON,
        Message,
        // Internal events
        ServerConnect,
        ServerDisconnect,
    };
    struct Event {
        Event(EventType type);
        Event(AuthRequest auth_request);
        Event(char *message);
        Event(EventType type, cJSON *cjson);
        
        EventType type;
        // if try connect
        union{ 
            AuthRequest auth_request;
            char *message; // ALLOCATED WITH NEW
            cJSON *cjson;
        };
    };

    int init();
    esp_err_t send_event(WSACS::Event ev);
} // namespace WSACS