#pragma once
#include <array>
#include <cstdint>
#include <string>

enum class CardTagType {
    SEVEN,
    FOUR,
};
const char* card_tag_type_to_string(CardTagType type);

struct CardTagID {
    CardTagType type;
    std::array<uint8_t, 10> value;

    std::string to_string() const;
};

struct OperatorPermissions {
    bool can_set_state;
    bool can_operate;
};

enum class IOState {
    IDLE,
    UNLOCKED,
    ALWAYS_ON,
    LOCKOUT,
    NEXT_CARD,
    STARTUP,
    WELCOMING,
    WELCOMED,
    ALWAYS_ON_WAITING,
    LOCKOUT_WAITING,
    IDLE_WAITING,
    AWAIT_AUTH,
    DENIED,
    FAULT,
    RESTART,
};
const char* io_state_to_string(IOState state);

enum class LogMessageType {
    NORMAL,
    DEBUG,
    ERROR,
};
const char* log_message_type_to_string(LogMessageType type);

struct LogMessage {
    LogMessageType type;
    std::string message;
};

enum class IOEventType {
    BUTTON_PRESSED,
    CARD_DETECTED,
    CARD_REMOVED,
    NETWORK_COMMAND,
};
const char* io_event_type_to_string(IOEventType type);

struct CardDetectedEvent {
    CardTagID card_tag_id;
    std::string to_string() const;
};

struct CardRemovedEvent {
    std::string to_string() const;
};

enum class ButtonEventType {
    CLICK,
    HELD,
    RELEASED,
};

struct ButtonEvent {
    ButtonEventType type;
};


enum class NetworkCommandEventType {
    IDENTIFY,
    COMMAND_STATE,
};
const char* network_command_event_type_to_string(NetworkCommandEventType type);

struct NetworkCommandEvent {
    NetworkCommandEventType type;
    IOState commanded_state; // Only valid if type is COMMAND_STATE
    std::string to_string() const;
};

struct IOEvent {
    IOEventType type;
    union {
        ButtonEvent button;
        CardDetectedEvent card_detected;
        CardRemovedEvent card_removed;
        NetworkCommandEvent network_command;
    };
    std::string to_string() const;
};

using WifiSSID     = std::array<uint8_t, 32>;
using WifiPassword = std::array<uint8_t, 64>;

namespace WSACS {
    // Things to tell wsacs system
    enum class EventType {
        StateChange,
        StateChangeCommand,
        Message,
        AuthRequest,
        AuthResponse,
        Identify,

        WifiUp,
    };

    struct StateChange {
        IOState from;
        IOState to;
    };
    struct Event {
        EventType type;
        union {
            StateChange state_change;
            IOState state_command;
        };
    };

} // namespace WSACS