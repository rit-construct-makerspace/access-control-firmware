#pragma once
#include "cJSON.h"
#include "common/types.hpp"
#include "esp_err.h"

namespace WSACS {
    int init();

    struct AuthResponse {
        CardTagID user;
        IOState to_state;
        bool verified;
    };
    // Network -> Websocket
    enum class EventType {
        // External Events
        TryConnect,  // sent by Network when we're online
        AuthRequest, // Ask server for authorization
        JSONLog,     // Send arbitrary json to server via Log: {} handler
        Message,     // Send text message to History

        // Internal events
        ServerConnect,    // sent when WS stack gets response to websocket upgrade
        ServerDisconnect, // sent when WS stack detects disconnect/close
        KeepAliveTimer,   // sent every 10s if online to tell server our state
    };
    struct Event {
        static Event auth_req(AuthRequest auth_request);
        static Event message(char* message);
        static Event json(cJSON* json);

        EventType type;
        union {
            AuthRequest auth_request;
            char* msg;    // message must have been allocated with new[], WSACS
                          // will delete[]
            cJSON* cjson; // lifetime is passed to WSACS. will cJSON_delete
        };
    };

    bool is_connected();

    /**
     * send an event to the websocket handler
     * Both external (from network) and internal (from esp networking stack)
     * flow through this
     * @param ev the event to send
     * @return ESP_OK on event submitted,
     * @return ESP_ERR_NO_MEM if unable to send to queue
     */
    esp_err_t send_event(WSACS::Event ev);
    /**
     * Shorthand version of send_event for events that are just a type with no
     * data associated
     * @param evtype type of event to send
     * @return value from send_event
     */
    esp_err_t send_event(WSACS::EventType evtype);
} // namespace WSACS