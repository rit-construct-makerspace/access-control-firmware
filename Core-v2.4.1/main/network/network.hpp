#pragma once
#include "common/types.hpp"
#include "network/wsacs.hpp"
#include "esp_netif_types.h"

namespace Network {

    int init();


    int send_event(NetworkEvent ev);
    bool is_online();

    // Used only by tasks on the network side of things to
    // communicate with the main network handler 
    enum class InternalEventType{
        NetifUp,
        NetifDown,
        
        ServerSetTime,
        ServerSetState,

        WSACSAuthResponse,

        ExternalEvent,
    };
    struct InternalEvent{
        InternalEventType type;
        union {
            int _ = 0;
            esp_ip4_addr_t netif_up_ip;
            IOState server_set_state;
            uint64_t server_set_time;
            WSACS::AuthResponse wsacs_auth_response;
            NetworkEvent external_event;
        };
    };
    int send_internal_event(InternalEvent ev);

} // namespace Network
