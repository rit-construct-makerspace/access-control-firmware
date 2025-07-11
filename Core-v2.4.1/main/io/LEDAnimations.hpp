#pragma once

#include <array>
#include <cstdint>

namespace Animation {
    using RGB = std::array<uint32_t, 3>;

    using Frame = std::array<RGB, 4>;

    struct Animation {
        uint8_t length;
        std::array<Frame, 16> frames;

        bool operator==(const Animation& other) const {
            if (length != other.length) {
                return false;
            }

            for (int i = 0; i < length; i++) {
                if (frames[i] != other.frames[i]) {
                    return false;
                }
            }

            return true;
        }
    };

    // *********************************
    // COLORS
    // *********************************

    static const RGB WHITE = {15, 15, 15};
    static const RGB OFF = {0, 0, 0};
    static const RGB RED = {15, 0, 0};
    static const RGB RED_DIM = {5, 0, 0};
    static const RGB GREEN = {0, 15, 0};
    static const RGB GREEN_DIM = {0, 5, 0};
    static const RGB BLUE = {0, 0, 15};
    static const RGB ORANGE = {15, 10, 0};
    static const RGB ORANGE_DIM = {5, 4, 0};

    // *********************************
    // ANIMATIONS
    // *********************************

    const Animation IDLE{
        .length = 2,
        .frames =
            {
                Frame{ORANGE, ORANGE, ORANGE, ORANGE},
                Frame{ORANGE, ORANGE, ORANGE, ORANGE},
            },
    };

    const Animation LOCKOUT{
        .length = 2,
        .frames =
            {
                Frame{RED, RED, RED, RED},
                Frame{RED, RED, RED, RED},
            },
    };

    const Animation UNLOCKED{
        .length = 2,
        .frames =
            {
                Frame{GREEN, GREEN, GREEN, GREEN},
                Frame{GREEN, GREEN, GREEN, GREEN},
            },
    };

    const Animation ALWAYS_ON{
        .length = 8,
        .frames =
            {
                Frame{GREEN, GREEN_DIM, GREEN, GREEN_DIM},
                Frame{GREEN, GREEN_DIM, GREEN, GREEN_DIM},
                Frame{GREEN, GREEN_DIM, GREEN, GREEN_DIM},
                Frame{GREEN, GREEN_DIM, GREEN, GREEN_DIM},
                Frame{GREEN_DIM, GREEN, GREEN_DIM, GREEN},
                Frame{GREEN_DIM, GREEN, GREEN_DIM, GREEN},
                Frame{GREEN_DIM, GREEN, GREEN_DIM, GREEN},
                Frame{GREEN_DIM, GREEN, GREEN_DIM, GREEN},
            },
    };

    const Animation DENIED{
        .length = 2,
        .frames =
            {
                Frame{RED, RED, RED, RED},
                Frame{RED_DIM, RED_DIM, RED_DIM, RED_DIM},
            },
    };

    const Animation STARTUP{
        .length = 3,
        .frames =
            {
                Frame{RED, RED, RED, RED},
                Frame{GREEN, GREEN, GREEN, GREEN},
                Frame{BLUE, BLUE, BLUE, BLUE},
            },
    };

    const Animation IDLE_WAITING{
        .length = 2,
        .frames =
            {
                Frame{OFF, ORANGE, ORANGE, OFF},
                Frame{OFF, OFF, OFF, OFF},
            },
    };

    const Animation ALWAYS_ON_WAITING{
        .length = 2,
        .frames =
            {
                Frame{OFF, GREEN, GREEN, OFF},
                Frame{OFF, OFF, OFF, OFF},
            },
    };

    const Animation LOCKOUT_WAITING{
        .length = 2,
        .frames =
            {
                Frame{OFF, RED, RED, OFF},
                Frame{OFF, OFF, OFF, OFF},
            },
    };

    const Animation RESTART{
        .length = 3,
        .frames =
            {
                Frame{RED, GREEN, BLUE, RED},
                Frame{BLUE, RED, GREEN, BLUE},
                Frame{GREEN, BLUE, RED, GREEN},
            },
    };

    const Animation FAULT{
        .length = 2,
        .frames =
            {
                Frame{RED, RED, RED, RED},
                Frame{OFF, OFF, OFF, OFF},
            },
    };

    const Animation AWAIT_AUTH{
        .length = 6,
        .frames =
            {
                Frame{ORANGE, ORANGE_DIM, ORANGE_DIM, ORANGE_DIM},
                Frame{ORANGE_DIM, ORANGE, ORANGE_DIM, ORANGE_DIM},
                Frame{ORANGE_DIM, ORANGE_DIM, ORANGE, ORANGE_DIM},
                Frame{ORANGE_DIM, ORANGE_DIM, ORANGE_DIM, ORANGE},
                Frame{ORANGE_DIM, ORANGE_DIM, ORANGE, ORANGE_DIM},
                Frame{ORANGE_DIM, ORANGE, ORANGE_DIM, ORANGE_DIM},
            },
    };

    const Animation IDENTIFY{
        .length = 2,
        .frames =
            {
                Frame{BLUE, BLUE, BLUE, BLUE},
                Frame{OFF, OFF, OFF, OFF},
            },
    };

    const Animation WELCOMING{
        .length = 4,
        .frames =
            {
                Frame{OFF, OFF, OFF, OFF},
                Frame{OFF, OFF, OFF, OFF},
                Frame{OFF, OFF, OFF, OFF},
                Frame{ORANGE, OFF, OFF, OFF},
            },
    };

    const Animation WELCOMED{
        .length = 2,
        .frames =
            {
                Frame{GREEN, OFF, OFF, OFF},
                Frame{GREEN, OFF, OFF, OFF},
            },
    };

    const Animation NEXT_CARD{
        .length = 0,
        .frames =
            {
                Frame{GREEN, ORANGE, ORANGE, ORANGE},
                Frame{ORANGE, GREEN, ORANGE, ORANGE},
                Frame{ORANGE, ORANGE, GREEN, ORANGE},
                Frame{ORANGE, ORANGE, ORANGE, GREEN},
                Frame{ORANGE, ORANGE, GREEN, ORANGE},
                Frame{ORANGE, GREEN, ORANGE, ORANGE},
            },
    };
} // namespace Animation