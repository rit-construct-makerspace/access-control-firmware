#pragma once
#include "common/types.hpp"
#include "network/wsacs.hpp"
#include "esp_netif_types.h"

namespace Network {
    int init();

    int send_event(NetworkEvent ev);
    bool is_online();

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
