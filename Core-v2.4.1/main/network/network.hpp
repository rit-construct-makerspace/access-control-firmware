#pragma once
#include "common/types.hpp"
#include "esp_netif_types.h"
#include "network/wsacs.hpp"

namespace Network {

    int init();

    /**
     * Send an event to the network task
     * anything not to do with networking should use this
     * @return true if submitted to queue. False otherwise
     */
    bool send_event(NetworkEvent ev);
    // shorthand for content less eventtype
    bool send_event(NetworkEventType ev);
    // shorthand for sent event type message
    bool send_message(char *);
    /**
     * @return true if we're connected to the server
     * @return false if not
     */
    bool is_online();

    // Called when network events happen that indicate online
    void network_watchdog_feed();

    // mark as complete so we dont time out and deny
    void mark_wsacs_request_complete();
    // Used only by tasks on the network side of things to
    // communicate with the main network handler
    enum class InternalEventType {
        // From Wifi Handler
        NetifUp,
        NetifDown,

        TryConnect,

        // From WS handler
        ServerUp, // websocket is open, time to auth
        ServerAuthed, // after auth, make accepts us
        ServerDown,

        // From timer
        WSACSTimedOut,

        KeepAliveTime,
        OtaUpdate,

        PollRestart,
        // From weirdos in IO
        ExternalEvent,
    };
    struct InternalEvent {
        InternalEventType type;
        union {
            esp_ip4_addr_t netif_up_ip;
            IOState server_set_state;
            uint64_t server_set_time;
            OTATag ota_tag;

            NetworkEvent external_event;
        };
    };
    bool send_internal_event(InternalEvent ev);
    bool send_internal_event(InternalEventType evtyp);

} // namespace Network
