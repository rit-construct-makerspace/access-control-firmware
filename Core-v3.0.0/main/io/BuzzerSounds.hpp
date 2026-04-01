#pragma once

#include <cstdint>

namespace SoundEffect {
    struct Note {
        uint32_t frequency;
        uint16_t duration; // in MS
    };

    struct Effect {
        uint16_t length;
        const Note* notes;
    };

    /*******************************
     * BEGIN NOTE DEFS
     ******************************/

    const Note OFF = {
        .frequency = 0,
        .duration = 100,
    };

    const Note A2 = {
        .frequency = 110,
        .duration = 200,
    };

    const Note A3 = {
        .frequency = 220,
        .duration = 200,
    };

    const Note A4 = {
        .frequency = 440,
        .duration = 200,
    };

    const Note A5 = {
        .frequency = 880,
        .duration = 200,
    };

    const Note DS7 = {
        .frequency = 2489,
        .duration = 200,
    };

    /********************************
     * BEGIN EFFECTS
     *******************************/
    namespace detail {
        const Note ACCEPTED_NOTES[2] = {A4, A5};
        const Note DENIED_NOTES[2] = {A4, A2};
        const Note LOCKOUT_NOTES[3] = {A3, OFF, A3};
        const Note FAULT_NOTES[6] = {A4, A2, A4, A2, A4, A2};

        // https://musescore.com/balloon12/super-mario-bros-victory-theme-ssbu
        const Note MARIO_VICTORY[32] = {
            Note{196, 200},
            Note{262, 200},
            Note{330, 200},
            Note{392, 200},
            Note{523, 200},
            Note{659, 200},
            Note{784, 600},
            Note{659, 300},
            Note{0, 300},
            // Bar 2
            Note{208, 200},
            Note{262, 200},
            Note{311, 200},

            Note{415, 200},
            Note{523, 200},
            Note{622, 200},

            Note{831, 600},
            Note{622, 300},
            Note{0, 300},
            // Bar 3
            Note{233, 200},
            Note{294, 200},
            Note{349, 200},

            Note{466, 200},
            Note{587, 200},
            Note{698, 200},

            Note{932, 600},
            Note{0, 10},
            Note{932, 200},
            Note{0, 10},
            Note{932, 200},
            Note{0, 10},
            Note{932, 200},

            Note{1047, 2400},
        };
    }; // namespace detail

    const Effect ACCEPTED = {
        .length = 2,
        .notes = detail::ACCEPTED_NOTES,
    };

    const Effect DENIED = {
        .length = 2,
        .notes = detail::DENIED_NOTES,
    };

    const Effect LOCKOUT = {
        .length = 3,
        .notes = detail::LOCKOUT_NOTES,
    };

    const Effect FAULT = {
        .length = 6,
        .notes = detail::FAULT_NOTES,
    };

    const Effect IDENTIFY = {

    };

    const Effect MARIO_VICTORY = {
        .length = 32,
        .notes = detail::MARIO_VICTORY,
    };
} // namespace SoundEffect