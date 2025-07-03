#include "common/types.hpp"

#include <iomanip>
#include <sstream>

#include "driver/gpio.h"

const char* card_tag_type_to_string(CardTagType type) {
    switch (type) {
    case CardTagType::FOUR:
        return "Four Byte UID";
    case CardTagType::SEVEN:
        return "Seven Byte UID";
    default:
        return "Unknown UID Type";
    }
}
std::string CardTagID::to_string() const {
    if (this->type != CardTagType::SEVEN && this->type != CardTagType::FOUR) {
        return "Unknown Card Tag";
    }
    std::size_t len = (this->type == CardTagType::FOUR) ? 4 : 7;

    std::stringstream ss;
    ss << std::hex << std::noshowbase << std::setw(2) << std::setfill('0');

    for (std::size_t i = 0; i < len; i++) {
        ss << this->value[i];
    }

    return ss.str();
}

const char* io_state_to_string(IOState state) {
    switch (state) {
    case IOState::IDLE:
        return "Idle";
    case IOState::UNLOCKED:
        return "Unlocked";
    case IOState::ALWAYS_ON:
        return "AlwaysOn";
    case IOState::LOCKOUT:
        return "Lockout";
    case IOState::NEXT_CARD:
        return "NextCard";
    case IOState::STARTUP:
        return "Startup";
    case IOState::WELCOMING:
        return "Welcoming";
    case IOState::WELCOMED:
        return "Welcomed";
    case IOState::ALWAYS_ON_WAITING:
        return "AlwaysOnWaiting";
    case IOState::LOCKOUT_WAITING:
        return "LockoutWaiting";
    case IOState::IDLE_WAITING:
        return "IdleWaiting";
    case IOState::AWAIT_AUTH:
        return "AwaitAuth";
    case IOState::DENIED:
        return "Denied";
    case IOState::FAULT:
        return "Fault";
    case IOState::RESTART:
        return "Restart";
    default:
        return "UNKNOWN IOSTATE";
    }
}

const char* log_message_type_to_string(LogMessageType type) {
    switch (type) {
    case LogMessageType::DEBUG:
        return "debug";
    case LogMessageType::ERROR:
        return "error";
    case LogMessageType::NORMAL:
        return "normal";
    default:
        return "UNKNOWN LOG TYPE";
    }
}

const char* io_event_type_to_string(IOEventType type) {
    switch (type) {
    case IOEventType::BUTTON_PRESSED:
        return "Button Pressed";
    case IOEventType::CARD_DETECTED:
        return "Card Detected";
    case IOEventType::CARD_REMOVED:
        return "Card Removed";
    case IOEventType::NETWORK_COMMAND:
        return "Network Command";
    default:
        return "Unknown IOEvent Type";
    }
}

std::string CardDetectedEvent::to_string() const {
    return "detected:" + card_tag_id.to_string();
}

const char* network_command_event_type_to_string(NetworkCommandEventType type) {
    switch (type) {
    case NetworkCommandEventType::COMMAND_STATE:
        return "Command State";
    case NetworkCommandEventType::IDENTIFY:
        return "Identify";
    default:
        return "UNKNOWN NETWORK COMMAND EVENT TYPE";
    }
}

std::string NetworkCommandEvent::to_string() const {
    if (type == NetworkCommandEventType::COMMAND_STATE) {
        return "Commanded:" + std::string{io_state_to_string(commanded_state)};
    } else if (type == NetworkCommandEventType::IDENTIFY) {
        return "Identify";
    } else {
        return "UNKNOWN NETWORK COMMAND";
    }
}

std::string IOEvent::to_string() const {
    switch (type) {
    case IOEventType::BUTTON_PRESSED:
        return "Button Pressed";
    case IOEventType::CARD_DETECTED:
        return "Card Detected" + card_detected.to_string();
    case IOEventType::CARD_REMOVED:
        return "Card Removed";
    case IOEventType::NETWORK_COMMAND:
        return "Network Command: " + network_command.to_string();
    default:
        return "INVALID IOEVENT";
    }
}