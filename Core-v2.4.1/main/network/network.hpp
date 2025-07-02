#pragma once
#include "common/types.hpp"
#include "network/wsacs.hpp"
#include "esp_netif_types.h"

namespace Network {

    enum class EventType {
        // Connection Management
        WifiUp,
        WifiDown,

        ServerConnectionUp,
        ServerConnectionDown,

        // Doing the job management
        WSACSEvent,
        GetFile,

        // Debug Stuff
        UploadCoredump,
    };


    struct Event {
        EventType type;
        union {
            esp_ip_addr_t wifi_up_ip;
            WSACS::Event wsacs_event;
        };
    };

    int init();

    void report_state_transition(IOState from, IOState to);
    bool is_online();

} // namespace Network
