#pragma once

#include <cstdint>
#include <array>

namespace Animation {
    using RGB = std::array<uint32_t, 3>;

    using Frame = std::array<RGB, 4>;

    struct Animation {
        uint8_t length;
        std::array<Frame, 16> frames;

        bool operator==(const Animation &other) const {
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

    // *********************************
    // ANIMATIONS
    // *********************************

    const Animation IDLE_ANIMATION {
        .length = 2,
        .frames = {
            Frame{ORANGE, ORANGE, ORANGE, ORANGE},
            Frame{ORANGE, ORANGE, ORANGE, ORANGE},
        }
    };
    
    const Animation LOCKOUT_ANIMATION {
        .length = 2,
        .frames = {
            Frame{RED, RED, RED, RED},
            Frame{RED, RED, RED, RED},
        }
    };
    
    const Animation UNLOCKED_ANIMATION {
        .length = 2,
        .frames = {
            Frame{GREEN, GREEN, GREEN, GREEN},
            Frame{GREEN, GREEN, GREEN, GREEN},
        }
    };
    
    const Animation ALWAYS_ON_ANIMATION {
        .length = 8,
        .frames = {
            Frame {GREEN, GREEN_DIM, GREEN, GREEN_DIM},
            Frame {GREEN, GREEN_DIM, GREEN, GREEN_DIM},
            Frame {GREEN, GREEN_DIM, GREEN, GREEN_DIM},
            Frame {GREEN, GREEN_DIM, GREEN, GREEN_DIM},
            Frame {GREEN_DIM, GREEN, GREEN_DIM, GREEN},
            Frame {GREEN_DIM, GREEN, GREEN_DIM, GREEN},
            Frame {GREEN_DIM, GREEN, GREEN_DIM, GREEN},
            Frame {GREEN_DIM, GREEN, GREEN_DIM, GREEN},
        }
    };
    
    const Animation DENIED_ANIMATION {
        .length = 2,
        .frames = {
            Frame {RED, RED, RED, RED},
            Frame {RED_DIM, RED_DIM, RED_DIM, RED_DIM},
        }
    };
    
    const Animation STARTUP_ANIMATION {
        .length = 3,
        .frames = {
            Frame {RED, RED, RED, RED},
            Frame {GREEN, GREEN, GREEN, GREEN},
            Frame {BLUE, BLUE, BLUE, BLUE},
        }
    };
    
    const Animation IDLE_WAITING_ANIMATION {
        .length = 2,
        .frames = {
            Frame {OFF, ORANGE, ORANGE, OFF},
            Frame {OFF, OFF, OFF, OFF},
        }
    };
    
    const Animation ALWAYS_ON_WAITING_ANIMATION {
        .length = 2,
        .frames = {
            Frame {OFF, GREEN, GREEN, OFF},
            Frame {OFF, OFF, OFF, OFF},
        }
    };
    
    const Animation LOCKOUT_WAITING_ANIMATION {
        .length = 2,
        .frames = {
            Frame {OFF, RED, RED, OFF},
            Frame {OFF, OFF, OFF, OFF},
        }
    };
    
    const Animation RESTART_ANIMATION {
        .length = 3,
        .frames = {
            Frame {RED, GREEN, BLUE, RED},
            Frame {BLUE, RED, GREEN, BLUE},
            Frame {GREEN, BLUE, RED, GREEN},
        }
    };
    
    const Animation FAULT_ANIMATION {
        .length = 2,
        .frames = {
            Frame {RED, RED, RED, RED},
            Frame {OFF, OFF, OFF, OFF},
        }
    };
}