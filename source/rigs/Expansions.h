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

    inline constexpr std::array<Expansion, 9> list
    {{
        { "NEON RAIN", "NR",
          "Inspired by the 1982 neo-noir sci-fi film score: CS-80-style brass, rain-soaked pads, smoky leads, glassy "
          "keys and slow neon pulses. Our own sounds." },
        { "HELLFORGE", "HF",
          "Inspired by modern industrial-metal game scores: synths played like down-tuned guitars through brutal "
          "distortion, sub drops, noise walls, a dark choir and syncopated chugs. Our own sounds." },
        { "STARCOURT", "SC",
          "Inspired by the dark '80s analog-synth score of a supernatural mystery series: rising arpeggios over minor "
          "chords, chorused polysynth pads, haunting leads, cassette wobble and big '80s drums. Our own sounds." },
        { "SANDSEA", "SS",
          "Inspired by the score of an epic desert science-fiction film: vast low drones, chant, keening voices, "
          "enormous horn blasts, ritual percussion, wind and sand, melodies in the Phrygian dominant scale. Our own sounds." },
        { "DATASTREAM", "DS",
          "Inspired by the scores of a digital-world science-fiction film and its sequel: precise glowing pulses, "
          "throbbing analog basses, orchestral-electronic swells, robotic voices and digital bleeps. Our own sounds." },
        { "LAST LIGHT", "LL",
          "Inspired by the score of a film about space, time and leaving the people you love: a soft cinematic felt "
          "piano, an enormous church organ, aching strings, a ticking clock and vast cold spaces. Sad and longing. "
          "Our own sounds." },
        { "SIMULACRUM", "SM",
          "Inspired by the score and soundtrack of a 1999 film about a simulated world: a dark, atonal orchestra with "
          "brass in clashing keys, late-'90s industrial and big-beat basses and breaks, fast trance arpeggios, falling "
          "digital rain and time slowing down. Our own sounds." },
        { "DREAMSTATE", "DR",
          "Inspired by the score of a 2010 film about dreams within dreams: a huge low brass blast, time slowed to a "
          "crawl and running at different speeds at once, relentless string ostinatos over heavy minor chords, a twangy "
          "reverb-soaked guitar, sparse piano and ticking clocks. Our own sounds." },
        { "NEGATIVE SPACE", "NS",
          "Inspired by the modern dark-ambient film and TV scores of the 2010s: a fragile, intimate felt piano close up "
          "over cold, rumbling electronics, hypnotic analog pulses, glitchy broken textures and buzzing grit. Our own "
          "sounds." },
    }};
}
