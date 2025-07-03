#pragma once
#include "common/types.hpp"
#include "network/wsacs.hpp"
#include "esp_netif_types.h"

namespace Network {
    int init();

    int send_event(NetworkEvent ev);
    bool is_online();

    enum class InternalEventType{
        ExternalEvent,
        NetifUp,
        NetifDown,
    };
    struct InternalEvent{
        InternalEventType type;
        union {
            NetworkEvent external_event;
            esp_ip4_addr_t netif_up_ip;
        };
    };
    int send_internal_event(InternalEvent ev);

} // namespace Network
