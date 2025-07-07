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
        Message,
        Log,

        // Internal events
        ServerConnect,
        ServerDisconnect,
    };
    struct Event {
        EventType type;
       // always included in messages to server, unused for internal events
        IOState current_state = IOState::IDLE;
        // if try connect
        union{ 
            int _ = 0;
            AuthRequest auth_request;
        };
    };

    int init();
    esp_err_t send_message(WSACS::Event ev);
} // namespace WSACS