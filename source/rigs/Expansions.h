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

    inline constexpr std::array<Expansion, 24> list
    {{
        { "NEON RAIN", "NR",
          "Inspired by the 1982 neo-noir sci-fi film score: CS-80-style brass, rain-soaked pads, smoky leads, glassy "
          "keys and slow neon pulses. Built round DOWNPOUR (0.57.0), rain on the glass whose drops ring on the notes you "
          "play, and MACRO 3, STORM: from dry to a downpour. Our own sounds." },
        { "HELLFORGE", "HF",
          "Inspired by modern industrial-metal game scores: synths as heavy as down-tuned guitars, leads played like a "
          "guitar, clean subs under noise that growls in tune, a screaming choir, a cathedral organ, a chainsaw. Built "
          "round INFERNAL (0.59.0), parallel destruction, and MACRO 3, DESTROY. Our own sounds." },
        { "STARCOURT", "SC",
          "Inspired by the dark '80s analog-synth score of a supernatural mystery series: rising arpeggios over minor "
          "chords, string-machine and polysynth pads, haunting leads, eighth-note basses. Built round AFTERGLOW (0.60.0), "
          "an old polysynth's glow, and MACRO 3, GLOW. Our own sounds." },
        { "SANDSEA", "SS",
          "Inspired by the score of an epic desert science-fiction film: vast low drones, chant, keening voices, "
          "enormous horn blasts, ritual percussion, wind and sand, melodies in the Phrygian dominant scale. Built round SIROCCO "
          "(0.58.0), a harmony answering every note in the bank's scale, and MACRO 3, SIROCCO: from one voice to a small "
          "choir. Our own sounds." },
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
        { "NIGHTDRIVE", "ND",
          "Inspired by the score and soundtrack of a 2011 neo-noir film about a getaway driver: late-night retro "
          "synthwave, all the way up: lush chorused polysynths, glassy '80s keys, pulsing octave basses, "
          "dreamy romantic leads, big reverbs and echoes. Our own sounds." },
        { "UNSPOKEN", "UN",
          "Inspired by the score of a 2016 film about first contact and a language that changes how you see time: "
          "human voices breathing, murmuring and stuttering in eerie layers, vast swelling drones, circular phrases "
          "that run forwards and back, cold fog, quiet awe and grief. Our own sounds." },
        { "CYBERSHRINE", "CS",
          "Inspired by the score of a 1995 animated cyberpunk film about a mind that may be a ghost in a machine: an "
          "ancient chorus of women's voices in close, clashing harmony, a huge, slow temple drum played as a bass, deep temple bells, and "
          "a cold, rainy, reverberant machine world. Our own sounds." },
        { "STOWAWAY", "SW",
          "Inspired by the score and sound of a 1979 science-fiction horror film about something hiding aboard a ship "
          "in deep space: echoing woodwinds, primitive horns, trembling string harmonics, low brass clusters, the "
          "ship's hum, hiss, chatter and alarms, and dread that never resolves. Our own sounds." },
        { "CHROMEFURY", "CF",
          "Inspired by the score of a 2015 post-apocalyptic chase film: relentless drive, sawing string ostinatos, "
          "guitar-like screaming leads stacked with distortion over growling low synths, engines, speed, dust and "
          "fire. Lead-heavy: 6 leads. Our own sounds." },
        { "ASCENSION", "AS",
          "Inspired by the score of a 2001 science-fiction game about an ancient ring-shaped world: a monks' chant "
          "growing into a vast, epic choir, heroic strings and brass, mystery and awe. Built on the VOICE "
          "filter for choirs that sound human. Our own sounds." },
        { "REFRACTION", "RF",
          "Inspired by the score of a 2018 science-fiction film about an expedition into a zone where everything is "
          "refracted and copied: a quiet fingerpicked string, light bending into colours, sounds that mutate and double "
          "as you hold them, and a vast, wailing alien swell. Otherworldly. Our own sounds." },
        { "PSI STORM", "PS",
          "Inspired by the score of a 1988 animated cyberpunk film about a biker gang and a boy whose psychic power "
          "breaks loose: shimmering bronze gamelan and deep bamboo, hypnotic group chant and slow monks' chant, a neon "
          "city at night, light trails, and power swelling out of control. Our own sounds." },
        { "BLOODWAVE", "BW",
          "Inspired by the soundtrack of a 2012 top-down action game set in a neon-soaked 1989 city: the dark, dirty "
          "side of the '80s, with driving distorted synths at club tempos, pumping basses, hazy, "
          "feverish detuned loops, worn tape and bit-crushed grit. Our own sounds." },
        { "DUSTFALL", "DF",
          "Inspired by the score of a 2017 science-fiction film, the sequel to a classic set thirty years on: colder, "
          "bigger and more brutal, with colossal distorted brass blasts and crushing sub drones, orange dust and grey "
          "snow over empty cities, machines at work, and, in between, fragile, lonely beauty. Our own sounds." },
        { "SUPERPOSITION", "SP",
          "Inspired by the score of a 2024 science-fiction series about a physicist thrown into other versions of his "
          "own life, searching for the way home: cold, cerebral and quietly dreadful, the same sound in many versions "
          "(copies drifting apart, notes never the same twice, patterns that never repeat), and under it the warm "
          "thread of home. Our own sounds." },
        { "SEVERED", "SV",
          "Inspired by the score of a 2022 series about office workers whose work memories are split from their home "
          "lives: a cheerful, sterile corporate surface (muzak, chimes, retro screens, humming lights, long white "
          "corridors) over quiet dread and a melancholy piano. Our own sounds." },
        { "SLINGBLADE", "SB",
          "Inspired by a trilogy of science-fiction novels about a miner who rises against a gilded ruling class: in "
          "three acts, the brutal heaviness of the mines below (drills, machinery, a fragile song in the dark), the "
          "opulence of a gilded court at war (brass fanfares, glass, a harpsichord), and the soaring dawn of a "
          "rebellion (anthems, a rising choir, victory bells). Built round the WAH: work it with the EXPR wheel. Our own "
          "sounds." },
        { "PARADOX", "PX",
          "Inspired by the score of a German series about four families in a small town where time runs in a loop, and "
          "past, present and future are tied together: crushing sub drones, walls of noise, bowed strings, a choir, "
          "ticking clocks and rain, rumbling underground passages, melancholy glitchy electronica. Built round ECHOES "
          "(your own playing coming back from the past) and MACRO 3, ERA: left the past (worn tape), right the future "
          "(cold and digital). Our own sounds." },
        { "MELTDOWN", "MD",
          "Inspired by the score of a 2019 series about a nuclear disaster, made almost entirely from the sounds of a "
          "power plant: pressure drones, groaning and ringing metal, pumps breathing, a counter's crackle, a lone voice "
          "rising out of the noise. Invisible danger, dread and grief. Built round CRACKLE, a counter's random clicks, "
          "and MACRO 3, DOSE: turn it up and the clicks thicken into a roar as the sound corrodes. Our own sounds." },
    }};
}
