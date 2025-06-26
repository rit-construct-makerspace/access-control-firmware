#pragma once
#include <array>
#include <cstdint>
#include <string>

enum class CardTagType {
    SEVEN,
    FOUR,
};

struct CardTagID {
    CardTagType type;
    std::array<uint8_t, 7> value;
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

enum class LogMessageType {
    NORMAL,
    DEBUG,
    ERROR,
};

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

struct ButtonEvent {

};

struct CardDetectedEvent {
    CardTagID card_tag_id;
};

struct CardRemovedEvent {

};

enum class NetworkCommandEventType {
    IDENTIFY,
    COMMAND_STATE,
};

struct NetworkCommandEvent {
    NetworkCommandEventType type;
    IOState commanded_state; // Only valid if type is COMMAND_STATE
};

struct IOEvent {
    IOEventType type;
    union {
        ButtonEvent button;
        CardDetectedEvent card_detected;
        CardRemovedEvent card_removed;
        NetworkCommandEvent network_command;
    };
};