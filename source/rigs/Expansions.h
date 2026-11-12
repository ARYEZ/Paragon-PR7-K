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

    inline constexpr std::array<Expansion, 39> list
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
          "Inspired by the score of an epic desert science-fiction film, in three columns: the deep desert (the sand "
          "sea, a keening voice, what moves under it), the desert people (chant, reed, didgeridoo, hand drums, zither, oud) "
          "and the invaders (colossal horns, pipes, engines, war drums). Melodies in the Phrygian dominant scale. Built "
          "round COLOSSUS (0.82.0) and MACRO 3, COLOSSUS: the sound made enormous. Many play free sample libraries (VCSL, "
          "the orchestra, Big Rusty Drums, Gogodze Phu, Squidpipes) when downloaded. Our own sounds." },
        { "DATASTREAM", "DS",
          "Inspired by the scores of a digital-world science-fiction film and its sequel: precise glowing pulses, "
          "throbbing analog basses, orchestral-electronic swells, robotic voices and digital bleeps. Built round "
          "LIGHTTRAIL (0.61.0), every note's glowing trail, and MACRO 3, TRAIL. Our own sounds." },
        { "LAST LIGHT", "LL",
          "Inspired by the score of a film about space, time and leaving the people you love: a soft cinematic felt "
          "piano, an enormous church organ, aching strings, a ticking clock and vast cold spaces. Sad and longing. "
          "Built round BLACKHOLE (0.62.0), the last moments stretched into a haze, and MACRO 3, GRAVITY. Our own sounds." },
        { "SIMULACRUM", "SM",
          "Inspired by the score and soundtrack of a 1999 film about a simulated world: a dark, atonal orchestra with "
          "brass in clashing keys, late-'90s industrial and big-beat basses and breaks, fast trance arpeggios, falling "
          "digital rain and time slowing down. Built round DILATION (0.63.0), time itself slowing, and MACRO 3, DILATE. "
          "Our own sounds." },
        { "DREAMSTATE", "DR",
          "Inspired by the score of a 2010 film about dreams within dreams: a huge low brass blast, time slowed to a "
          "crawl and running at different speeds at once, relentless string ostinatos over heavy minor chords, a twangy "
          "reverb-soaked guitar, sparse piano and ticking clocks. Built round REMNANT (0.64.0), layers of your sound "
          "beneath it, each deeper one slower, and MACRO 3, DREAM. Our own sounds." },
        { "NEGATIVE SPACE", "NS",
          "Inspired by the modern dark-ambient film and TV scores of the 2010s: a fragile, intimate felt piano close up "
          "over cold, rumbling electronics, hypnotic analog pulses, glitchy broken textures and buzzing grit. Built round "
          "PROXIMITY (0.65.0), the sound of playing heard up close, and MACRO 3, CLOSER. Our own sounds." },
        { "NIGHTDRIVE", "ND",
          "Inspired by the score and soundtrack of a 2011 neo-noir film about a getaway driver: late-night retro "
          "synthwave, all the way up: lush chorused polysynths, glassy '80s keys, pulsing octave basses, "
          "dreamy romantic leads, big reverbs and echoes. Built round HAZE (0.66.0), your sound sweeping past like "
          "streetlights, and MACRO 3, HAZE. Our own sounds." },
        { "UNSPOKEN", "UN",
          "Inspired by the score of a 2016 film about first contact and a language that changes how you see time: "
          "human voices breathing, murmuring and stuttering in eerie layers, vast swelling drones, circular phrases "
          "that run forwards and back, cold fog, quiet awe and grief. Built round LEVIATHAN (0.68.0), a vast voice "
          "singing beneath what you play, and MACRO 3, LEVIATHAN. Our own sounds." },
        { "CYBERSHRINE", "CS",
          "Inspired by the score of a 1995 animated cyberpunk film about a mind that may be a ghost in a machine: an "
          "ancient chorus of women's voices in close, clashing harmony, a huge, slow temple drum played as a bass, deep temple bells, and "
          "a cold, rainy, reverberant machine world. Built round SPECTRE (0.67.0), a whispered ghost following your "
          "sound, and MACRO 3, SPECTRE. Our own sounds." },
        { "STOWAWAY", "SW",
          "Inspired by the score and sound of a 1979 science-fiction horror film about something hiding aboard a ship "
          "in deep space: echoing woodwinds, primitive horns, trembling string harmonics, low brass clusters, the "
          "ship's hum, hiss, chatter and alarms, and dread that never resolves. Our own sounds." },
        { "CHROMEFURY", "CF",
          "Inspired by the score of a 2015 post-apocalyptic chase film: war drums, sawing string ostinatos, brass blasts, "
          "a screaming guitar on a truck of speakers, a war-chant choir, engines, dust and fire. Three columns: the war "
          "convoy, the fortress and the wasteland. Rebuilt in 0.88.0 round REDLINE and MACRO 3, REDLINE: the sound as a "
          "revving engine in time with the song. Its guitar plays Black And Green Guitars, its drums Big Rusty Drums, "
          "its organ VCSL and its brass, choir and strings the Sonatina Symphonic Orchestra, once downloaded (free). Our "
          "own sounds." },
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
        { "ATOMIC AGE", "AA",
          "Inspired by the retro-futurist mid-century look of a 2024 series, played for its bright side: 1950s "
          "atomic-age optimism, space-age lounge and exotica, doo-wop \"oohs\", a B-movie sci-fi singing sine, cheerful "
          "jingles and a dusty desert-western twang. Built round NUCLEARSPRING (0.69.0), a vintage spring reverb tank, "
          "and MACRO 3, SPRING. Our own sounds." },
        { "HYPERDRIVE", "HD",
          "Inspired by the late-'70s and early-'80s space-opera film scores: heroic brass fanfares, a lonely horn over a "
          "desert horizon, a men's choir, shimmering strings, a marching low-brass ostinato, a smoky spaceport dance band, "
          "harps, racing strings and beeping computers. MACRO 3 (0.86.0) is NAVICOMPUTER on the held sounds (the notes "
          "moved by a pattern in key, in time: a held chord becomes a riff) and HYPERJUMP on the rhythmic ones (a build "
          "and a drop in time with the song). Its orchestral presets play the Sonatina Symphonic Orchestra once downloaded "
          "(free), and PR7-K's own synthesized orchestra until then. Our own sounds." },
        { "GRAND CINEMA", "GC",
          "The modern film and trailer piano: intimate felt pianos heard up close, vast halls where the notes bloom into "
          "each other, epic low octave hits and driving ostinatos, dark clusters and detuned, prepared pianos, a reversed "
          "swell. Built round BLOOM (0.74.0), the strings ringing along and a hall opening out, and MACRO 3, BLOOM. Every "
          "preset plays the Salamander Grand once it's downloaded (free), and PR7-K's own synthesized piano until then. "
          "Our own sounds." },
        { "MAESTRO", "MA",
          "The epic film-trailer orchestra: heroic horns, soaring violins, a lone trumpet, trembling strings, a vast "
          "choir, brass hits, driving cello ostinatos, timpani, celesta, harps and ticking strings. The mod wheel is the "
          "dynamics on every preset. Built round LEGION (0.76.0) and MACRO 3, LEGION: from one player to a full section. "
          "Every preset plays the Sonatina Symphonic Orchestra once it's downloaded (free), and PR7-K's own synthesized "
          "orchestra until then. Our own sounds." },
        { "TWIN SPIRES", "TS",
          "Inspired by an animated series about two sister cities, a gilded one of inventors above and a grimy, chemical "
          "one below: polished brass, strings, a harpsichord and clockwork for the city above; distorted machine basses, "
          "punk leads, toxic haze and a broken music box for the one below; crystal tones, a heartbroken piano and neon "
          "pulses where they meet. Built round OVERCHARGE (0.77.0) and MACRO 3, OVERCHARGE: unstable magic, charging the "
          "sound then breaking it apart. Its orchestral presets play the Sonatina Symphonic Orchestra once downloaded "
          "(free). Our own sounds." },
        { "STOLEN SKY", "SK",
          "Inspired by a series about a hidden town built inside a mountain after the world ended, a perfect suburb under "
          "a false sky: sweet whistles, warm pads, clean basses and a gentle piano for the town; concrete drones, "
          "machine basses, cold arps and steel drips for the truth underneath; grieving strings, a lonely lead, urgent "
          "arps and cold, emotional pianos for the end of the world. Built round AFTERMATH (0.79.0) and MACRO 3, "
          "AFTERMATH: from under the open sky into a sealed concrete bunker. Its pianos play the Salamander Grand, and its "
          "strings, solo violin, flute and low strings the Sonatina Symphonic Orchestra, once downloaded (free). Our own "
          "sounds." },
        { "DROWNED DECO", "DD",
          "Inspired by a city built under the sea in the 1940s and its fall: its gilded jazz age, already warped (a "
          "clarinet, chandelier strings, an upright bass, a cocktail piano, a vibraphone), its fall into madness (a "
          "theremin, flooding clusters, groaning steel, a skipping record, frantic pizzicato) and the silence of the sea "
          "after (a drowned trumpet, an abyssal choir, a trench drone, a sunken celesta, sonar). Built round SUBMERGE "
          "(0.80.0) and MACRO 3, SUBMERGE: the sound sinking under water. Its pianos play the Salamander Grand, and its "
          "clarinet, strings, choir, bass, celesta, vibraphone and pizzicato the Sonatina Symphonic Orchestra, once "
          "downloaded (free). Our own sounds." },
        { "NEW EDEN", "NE",
          "Inspired by a series about a near-future theocracy that took power by force: the church (a cantor, a "
          "sanctuary organ, pedal bourdon, chapel bells, psalm chimes, handbells), the regime (command brass, a state "
          "anthem, iron boots marching, a tolling bell, a snare roll, a drill snare) and the women who keep their own "
          "voice (a lone hymn, glass prayer, an undertone, a hidden piano, hidden letters, a thread of harp). Built round "
          "LOCKSTEP (0.81.0) and MACRO 3, LOCKSTEP: the sound drilled into a march. Its organ, bells, chimes, glockenspiel and "
          "snare play the Versilian Community Sample Library, its snare roll Sam's Sonor, its brass, choir, strings and "
          "harp the Sonatina Symphonic Orchestra and its piano the Salamander Grand, once downloaded (free). Our own "
          "sounds." },
        { "GUNSLINGER", "GS",
          "Inspired by the spaghetti-western sound: a lonesome whistle, a high-noon trumpet, a harmonica, the plain at "
          "dusk, men chanting, heat shimmer, an upright bass, a baritone twang, a galloping bass, a saloon piano, mission "
          "bells, a jaw harp, hoofbeats, a banjo roll, twang guitars, an archtop and spurs. Built round CANYON (0.84.0) "
          "and MACRO 3, CANYON: the sound's echoes off rock walls, the far wall replying in time with the song. Its "
          "guitars play Black And Green Guitars and Shinyguitar, its bass Meatbass, its banjos the Kay 5-String Banjo "
          "and ganjo, its harmonica, bells and woodblocks the Versilian Community Sample Library, its trumpet, strings "
          "and choir the Sonatina Symphonic Orchestra and its piano the Salamander Grand, once downloaded (free). Our "
          "own sounds and phrases." },
        { "MACHINE SUPREMACY", "MS",
          "Inspired by the sound of a neon megacity's future: a street lead's sneer, a cold corporate sync lead, a signal "
          "breaking up in the wires; smog, penthouse glass and a corrupted choir; a pumping club bass, a smooth reese and "
          "an FM growl; a punk guitar, glass-elevator keys and dead-pixel keys; a street beat, a boardroom sequence and a "
          "packet storm. Three columns: the street, the towers and the net. Built round JACK-IN (0.85.0) and MACRO 3, "
          "JACK-IN: something breaks into the sound and takes the phrase over, slice by slice in time with the song. Its "
          "guitar plays Black And Green Guitars, its beat Big Rusty Drums and its choir the Sonatina Symphonic "
          "Orchestra, once downloaded (free). Our own sounds and phrases." },
        { "LAST DAY", "LD",
          "Inspired by the 1970s science-fiction films of sealed, perfect cities: a golden gliding lead, a hunter's sync "
          "scream and a theremin's wind; a string machine, a choir played back from tape and a sunrise outside; a pulsing "
          "sequencer bass, a dark enforcer and a pedal drone; a phaser piano, ring-modulated bells and a rusted organ; "
          "sample-and-hold bleeps, a mainframe's chatter and a Berlin-school run. Three columns: the sealed city, the "
          "system that runs it, and outside. Built round SWEEP (0.87.0) and MACRO 3, SWEEP: a huge resonant analogue sweep "
          "locked to the song. Its strings, choir and harp play the Sonatina Symphonic Orchestra once downloaded (free). "
          "Our own sounds." },
        { "PLAYGROUND", "PG",
          "Inspired by a survival drama played as children's games: a child's recorder, a cold masked trumpet and a "
          "lonely harmonica; a pastel toy organ, sterile dormitory rows and a warm night market; an oompah toy tuba, a "
          "guards' march and a debt collector's sub; a music box, a salon waltz on harpsichord and a pawn-shop piano; a "
          "clapping game, marching numbers and a countdown clock. Sweet sing-song tunes with dread underneath. Three "
          "columns: the playground, the masked staff and the debt outside. Built round FREEZE TAG (0.89.0) and MACRO 3, "
          "FREEZE TAG: in time with the song the sound plays, then freezes, a trembling ghost of the moment holding the "
          "silence. Its recorders, glockenspiel, xylophone, claps and harmonica play VCSL, its trumpet, harpsichord and "
          "strings the Sonatina Symphonic Orchestra and its piano the Salamander Grand, once downloaded (free). Our own "
          "sounds." },
        { "FINAL DIVIDE", "FD",
          "Inspired by a story of a plague and the survivors who split into two camps: a lonely tone on a dying radio, a "
          "porch slide guitar and a smoky desert-lounge reed; wind in an empty city, a gospel choir and a neon mirage; a "
          "respirator's drone, a walking church bass and a velvet menace; hospital intercom bells, a revival organ and "
          "casino electric piano; a fever's clock, a hand-clap revival and a slot machine; a broken radio, a front-porch "
          "banjo and a rattlesnake. Three columns: the plague, the free side and the dark side. Built round CROSSROADS "
          "(0.90.0) and MACRO 3, CROSSROADS: the sound pulled between light and dark in time with the song, the dark "
          "side pulling every note down the scale, in key. Its guitar, choir, bass, claps and banjo play Shinyguitar, the "
          "Sonatina Symphonic Orchestra, Meatbass, VCSL and the Kay banjo once downloaded (free). Our own sounds." },
        { "DARK DESCENT", "DK",
          "Inspired by a story of the last people living in a buried tower a hundred levels deep: a pressure valve's "
          "screaming sync lead, a whistle up the stairwell and a voice on a suit's helmet radio; the generator hall, the "
          "people of the levels singing and the grey horizon outside; a piston bass, a bass walking down the levels and "
          "an airlock sealing; foundry bells, a worn piano on a landing and the great wall screen; an assembly line, "
          "footsteps down the spiral stairs and a suit's telemetry; a bolt driver, mended strings and frost on the "
          "glass. Three columns: the machine depths, the stairs and the outside. Built round GEARWORKS (0.91.0) and "
          "MACRO 3, GEARWORKS: the sound caught in the machinery, two gears in time with the song cutting it, ringing it "
          "as metal, steam and clanks on every tooth. Its choir, bass, bells, piano and guitar play the Sonatina "
          "Symphonic Orchestra, Meatbass, VCSL, the Salamander Grand and Black And Green Guitars once downloaded (free). "
          "Our own sounds." },
        { "HIGHNOON", "HN",
          "Inspired by a story of a frontier theme park run by machines and the hosts who wake inside it: a fiddle at "
          "the saloon door, the pure voice of the machine and a lonely cello slipping out of tune; dawn over the town, "
          "a choir that's human but not quite and a memory on warped tape; the saloon's upright, a servo bass and the "
          "ground giving way; the saloon's player piano, calibration bells and a piano coming apart; a galloping banjo, "
          "a body being made and a loop breaking; a dusty guitar, a wind-up music box and a string giving out. Three "
          "columns: the park, under the park and waking up. Built round RAGTIME (0.92.0) and MACRO 3, RAGTIME: the "
          "park's player piano striking every note you hold on a roll in time with the song, stride bass and all. Its "
          "fiddle, cello, strings, choir, bass, pianos, banjo, guitar and music box play the Sonatina Symphonic "
          "Orchestra, Meatbass, the Salamander Grand, the Kay banjo, Shinyguitar and VCSL once downloaded (free). Our "
          "own sounds." },
    }};
}
