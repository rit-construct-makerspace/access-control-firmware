#pragma once
#include "common/types.hpp"
#include "esp_err.h"

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

        // Internal events
        ServerConnect,
        ServerDisconnect,
    };
    struct Event {
        Event(EventType type);
        Event(EventType type, AuthRequest auth_request);

        EventType type;
        // if try connect
        union{ 
            AuthRequest auth_request;
        };
    };

    int init();
    esp_err_t send_event(WSACS::Event ev);
} // namespace WSACS