
#pragma once
#include <array>
#include <cstdint>
#include <format>
#include <string>

enum class CardTagType {
    SEVEN,
    FOUR,
};
std::string card_tag_type_to_string(CardTagType type);

struct CardTagID  {
    CardTagType type;
    std::array<uint8_t, 7> value;

    std::string to_string() const;
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

std::string io_state_to_string(IOState state);

enum class LogMessageType {
    NORMAL,
    DEBUG,
    ERROR,
};
std::string log_message_type_to_string(LogMessageType type);

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
std::string io_event_type_to_string(IOEventType type);

struct ButtonEvent {};

struct CardDetectedEvent {
    CardTagID card_tag_id;
    std::string to_string() const;
};

struct CardRemovedEvent {
    std::string to_string() const;
};

enum class NetworkCommandEventType {
    IDENTIFY,
    COMMAND_STATE,
};
std::string network_command_event_type_to_string(NetworkCommandEventType type);

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
