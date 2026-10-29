#pragma once

#include <array>

// The built-in expansions: themed sets of presets (3 leads, 3 pads, 3 basses, 3 keys, 3 arps), each in a bank of
// its own after the user's banks A-E (bank Rig::numBanks + its index here). Only factory presets live there: a
// preset the user saves is always filed in A-E. Never reorder: a preset's bank is its place in this list (the
// browser and the slot names read it; saved files never hold an expansion bank).
namespace expansions
{
    struct Expansion
    {
        const char* name;          // the bank's name, shown in the browser
        const char* code;          // two letters for the slot name ("NR01")
        const char* description;
    };

    inline constexpr std::array<Expansion, 1> list
    {{
        { "NEON RAIN", "NR",
          "Inspired by the 1982 neo-noir sci-fi film score: CS-80-style brass, rain-soaked pads, smoky leads, glassy "
          "keys and slow neon pulses. Our own sounds." },
    }};
}
