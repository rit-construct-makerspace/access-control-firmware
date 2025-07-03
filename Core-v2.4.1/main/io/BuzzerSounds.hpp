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
        const Note ACCEPTED_NOTES[2] = { A4, A5 };
        const Note DENIED_NOTES[2] = { A4, A2 };
        const Note LOCKOUT_NOTES[3] = { A3, OFF, A3 };
    };

    const Effect ACCEPTED_EFFECT = {
        .length = 2,
        .notes = detail::ACCEPTED_NOTES,
    };

    const Effect DENIED_EFFECT = {
        .length = 2,
        .notes = detail::DENIED_NOTES,
    };

    const Effect LOCKOUT_EFFECT = {
        .length = 3,
        .notes = detail::LOCKOUT_NOTES,
    };
}