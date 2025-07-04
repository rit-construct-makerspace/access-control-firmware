#pragma once
#include "common/types.hpp"
#include "esp_netif_types.h"

namespace Network {

    int init();

    /// @return true if the event was sent, false otherwise 
    bool send_event(NetworkEvent ev);

    /// @return true if the network is online
    bool is_online();

    // Used only by tasks on the network side of things to
    // communicate with the main network handler 
    enum class InternalEventType{
        NetifUp,
        NetifDown,
        
        ServerSetTime,
        ServerSetState,

        ExternalEvent,
    };
    struct InternalEvent{
        InternalEventType type;
        union {
            esp_ip4_addr_t netif_up_ip = {0};
            IOState server_set_state;
            uint64_t server_set_time;
            NetworkEvent external_event;
        };
    };
    int send_internal_event(InternalEvent ev);

} // namespace Network
