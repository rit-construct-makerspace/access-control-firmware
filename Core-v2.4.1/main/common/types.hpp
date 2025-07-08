#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <optional>

enum class CardTagType {
    FOUR = 4,
    SEVEN = 7,
};
const char* card_tag_type_to_string(CardTagType type);

struct CardTagID {
    CardTagType type = CardTagType::SEVEN;
    std::array<uint8_t, 10> value = {0};

    static std::optional<CardTagID> from_string(const char *);
    bool operator==(const CardTagID &other) const {
        if (type != other.type) {
            return false;
        }

        for (int i = 0; i < (int) type; i++) {
            if (value[i] != other.value[i]) {
                return false;
            }
        }

        return true;
    }
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
std::optional<IOState> parse_iostate(const char* str);


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
    CARD_READ_ERROR,
    NETWORK_COMMAND,
};
const char* io_event_type_to_string(IOEventType type);

struct CardDetectedEvent {
    CardTagID card_tag_id;
    std::string to_string() const;
};

struct CardRemovedEvent {
    CardTagID card_tag_id;
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
    DENY,
};
const char* network_command_event_type_to_string(NetworkCommandEventType type);

struct NetworkCommandEvent {
    NetworkCommandEventType type;
    IOState commanded_state; // Only valid if type is COMMAND_STATE
    bool requested; // true if we asked to auth. false if command came from on
                    // high
    CardTagID for_user;
    std::string to_string() const;
};

// From other things, to IO
struct IOEvent {
    IOEventType type;
    union {
        int _ = 0;
        ButtonEvent button;
        CardDetectedEvent card_detected;
        CardRemovedEvent card_removed;
        NetworkCommandEvent network_command;
    };
    std::string to_string() const;
};

using WifiSSID     = std::array<uint8_t, 32>;
using WifiPassword = std::array<uint8_t, 64>;

enum class StateChangeReason {
    ButtonPress,
    OverTermperature,
    CardRemoved,
    CardActivated,
    ServerCommanded,
};

struct StateChange {
    IOState from;
    IOState to;
    StateChangeReason reason;
    std::optional<CardTagID> who;
};

struct AuthRequest {
    CardTagID requester;
    IOState to_state;
};
struct AuthResponse{
    CardTagID requester;
    bool verified;
};

// Things to tell network task
enum class NetworkEventType {
    AuthRequest,
    Message,
    StateChange,
    PleaseRestart,
};

struct NetworkEvent {
    NetworkEventType type;
    union {
        int _ = 0;
        AuthRequest auth_request;
        char *message; // allocated using new[]. Sending this also sends lifetime (network will delete[])
        StateChange state_change;
    };
};
enum class HardwareEdition {
    LITE,
    STANDARD,
};

enum class FaultReason {
    SERVER_COMMANDED,
    OVER_TEMP,
    START_FAIL,
    CARD_SWITCH,
};
const char* fault_reason_to_string(FaultReason type);
