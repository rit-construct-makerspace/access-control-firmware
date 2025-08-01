#include "common/types.hpp"

#include "string.h"

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
    static constexpr size_t buflen = 15; // 7*2 + null terminator
    char buf[buflen] = {0};

    std::size_t len = (this->type == CardTagType::FOUR) ? 4 : 7;
    for (int index = 0; index < len; index++) {
        snprintf(&buf[index * 2], 3, "%02x", value[index]);
    }

    return std::string{buf};
}

std::optional<CardTagID> CardTagID::from_string(const char* str) {
    if (str == NULL){
        return {};
    }
    int len = strnlen(str, 15);
    if (len == 15) {
        return {};
    }
    CardTagID id = {};

    int len_to_read = 0;
    if (len == 8) {
        id.type = CardTagType::FOUR;
        len_to_read = 8;
    } else if (len == 14) {
        id.type = CardTagType::SEVEN;
        len_to_read = 14;
    } else {
        return {};
    }
    for (int i = 0; i < len_to_read; i += 2) {
        char hex[3] = {str[i], str[i + 1], 0};
        errno = 0;
        long val = strtol(hex, NULL, 16);
        if (errno || val > 255 || val < 0) {
            return {};
        }
        id.value[i / 2] = (uint8_t)val;
    }
    return id;
}

std::optional<IOState> parse_iostate(const char* str) {
    constexpr size_t max_size = 50;
    int len = strnlen(str, max_size);

    if (len == max_size) {
        // return if bigger or not null terminated
        return {};
    }
    if (0 == strcasecmp(str, "idle")) {
        return IOState::IDLE;
    } else if (0 == strcasecmp(str, "unlocked")) {
        return IOState::UNLOCKED;
    } else if (0 == strcasecmp(str, "alwayson")) {
        return IOState::ALWAYS_ON;
    } else if (0 == strcasecmp(str, "lockout")) {
        return IOState::LOCKOUT;
    } else if (0 == strcasecmp(str, "nextcard")) {
        return IOState::NEXT_CARD;
    } else if (0 == strcasecmp(str, "startup")) {
        return IOState::STARTUP;
    } else if (0 == strcasecmp(str, "welcoming")) {
        return IOState::WELCOMING;
    } else if (0 == strcasecmp(str, "welcomed")) {
        return IOState::WELCOMED;
    } else if (0 == strcasecmp(str, "alwaysonwaiting")) {
        return IOState::ALWAYS_ON_WAITING;
    } else if (0 == strcasecmp(str, "alwaysonwaiting")) {
        return IOState::ALWAYS_ON_WAITING;
    } else if (0 == strcasecmp(str, "idlewaiting")) {
        return IOState::IDLE_WAITING;
    } else if (0 == strcasecmp(str, "awaitauth")) {
        return IOState::AWAIT_AUTH;
    } else if (0 == strcasecmp(str, "denied")) {
        return IOState::DENIED;
    } else if (0 == strcasecmp(str, "fault")) {
        return IOState::FAULT;
    } else if (0 == strcasecmp(str, "restart")) {
        return IOState::RESTART;
    }
    return {};
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
        case IOEventType::CARD_READ_ERROR:
            return "Card Read Error";
        default:
            return "Unknown IOEvent Type";
    }
}

std::string CardDetectedEvent::to_string() const {
    return "detected:" + card_tag_id.to_string();
}
std::string CardRemovedEvent::to_string() const {
    return "removed:" + card_tag_id.to_string();
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

StateChangeReason fault_reason_to_state_change_reason(FaultReason fault) {
    switch (fault) {
        case FaultReason::CARD_SWITCH:
            return StateChangeReason::CardSwitch;
        case FaultReason::TEMP_ERROR:
            return StateChangeReason::TemperatureError;
        case FaultReason::SERVER_COMMANDED:
            return StateChangeReason::ServerCommanded;
        default:
            // prolly shouldn't do this but idk waht the default should be
            return StateChangeReason::ServerCommanded;
    }
}

const char* fault_reason_to_string(FaultReason reason) {
    switch (reason) {
        case FaultReason::CARD_SWITCH:
            return "Card Switch";
        case FaultReason::TEMP_ERROR:
            return "Over Temperature";
        case FaultReason::SERVER_COMMANDED:
            return "Server Commanded";
        case FaultReason::START_FAIL:
            return "Startup Failure";
        case FaultReason::SOFTWARE_ERROR:
            return "Software Error";
        default:
            return "INVALID FAULT REASON";
    }
}