#pragma once
#include "common/types.hpp"
#include "esp_netif_types.h"
#include "network/wsacs.hpp"

namespace Network {

    int init();

    /**
     * Send an event to the network task
     * anything not to do with networking should use this
     */
    int send_event(NetworkEvent ev);
    /**
     * @return true if we're connected to the server
     * @return false if not
     */
    bool is_online();

    // Called when network events happen that indicate online
    void network_watchdog_feed();
    // Used only by tasks on the network side of things to
    // communicate with the main network handler
    enum class InternalEventType {
        NetifUp,
        NetifDown,

        ServerSetTime,
        ServerSetState,

        WSACSAuthResponse,
        WSACSTimedOut,

        NetworkWatchdogExpire,

        ExternalEvent,
    };
    struct InternalEvent {
        InternalEventType type;
        union {
            esp_ip4_addr_t netif_up_ip;
            IOState server_set_state;
            uint64_t server_set_time;
            WSACS::AuthResponse wsacs_auth_response;
            NetworkEvent external_event;
        };
    };
    int send_internal_event(InternalEvent ev);

} // namespace Network
