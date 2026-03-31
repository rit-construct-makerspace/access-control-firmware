#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

enum class CardTagType {
    FOUR = 4,
    SEVEN = 7,
};
const char* card_tag_type_to_string(CardTagType type);

struct CardTagID {
    CardTagType type = CardTagType::SEVEN;
    std::array<uint8_t, 10> value = {0};

    static std::optional<CardTagID> from_string(const char*);
    bool operator==(const CardTagID& other) const {
        if (type != other.type) {
            return false;
        }

        for (int i = 0; i < (int)type; i++) {
            if (value[i] != other.value[i]) {
                return false;
            }
        }

        return true;
    }
    std::string to_string() const;
};

using WifiSSID = std::array<uint8_t, 32>;
using WifiPassword = std::array<uint8_t, 64>;