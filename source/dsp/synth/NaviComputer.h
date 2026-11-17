#pragma once

#include "Sirocco.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>

// NAVICOMPUTER (0.86.0, the owner's name; HYPERDRIVE's, beside HYPERJUMP, option 2 of three offered for LIGHTSPEED's
// place; our own design). A pitch sequencer in key: in time with the song it moves every note sounding by a pattern of
// steps of the scale, so a held chord becomes a moving riff and a held note a computer's line. Not an audio effect: the
// notes themselves move (each by its own steps of KEY and SCALE, as SIROCCO's answers do), so nothing smears and a
// chord stays in key.
//  - PATTERN: 16 steps, each a move in steps of a seven-note scale (0 the note, 2 a third, 4 a fifth, 7 the octave;
//    scales of other sizes take the nearest step); never reorder `patternNames`;
//  - DIVISION: the steps' length (1/16, 1/8, 1/16 T, 1/4; never reorder);
//  - AMOUNT: how many of the steps move, in the order `revealOrder` gives (the last step of each four first, the
//    downbeats last): a little, an occasional jump; all the way, the whole pattern;
//  - CHAOS: that share of the steps replaced by another move, a new choice every bar (the same bar always gets the
//    same: a song plays back the same each time);
//  - GLIDE: each move slides over up to 40% of a step rather than jumping.
// In time with `ppq` while the song plays; its own count while stopped, started by the first key down. Off, or AMOUNT
// under 0.001, the notes are untouched (bit for bit: nothing is added to their pitch).
namespace synth::navi
{
    inline constexpr int numSteps = 16;
    inline const juce::StringArray patternNames { "ASCEND", "DESCEND", "FIFTHS", "OCTAVES", "TRILL", "GALLOP", "COMPUTER", "SIGNAL" };   // never reorder
    // Each step's move in steps of a seven-note scale.
    inline constexpr std::array<std::array<int, numSteps>, 8> patterns {{
        { 0, 2, 4, 7,  0, 2, 4, 7,  0, 2, 4, 7,  2, 4, 7, 9 },          // ASCEND: up the chord
        { 7, 4, 2, 0,  7, 4, 2, 0,  9, 7, 4, 2,  7, 4, 2, -3 },         // DESCEND: down it
        { 0, 0, 4, 0,  0, 4, 0, 4,  0, 0, 4, 0,  7, 4, 0, 4 },          // FIFTHS
        { 0, 7, 0, 7,  0, 7, 0, 7,  -7, 0, -7, 0,  0, 7, 0, 14 },       // OCTAVES
        { 0, 1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1,  2, 1, 2, 1 },          // TRILL
        { 0, 0, 0, 7,  0, 0, 0, 7,  0, 0, 4, 7,  0, 0, 4, 9 },          // GALLOP
        { 0, 5, 2, 9,  4, 0, 7, 3,  0, 6, 2, 8,  4, 1, 7, 11 },         // COMPUTER: a machine's line
        { 0, 0, 0, 0,  2, 2, 2, 2,  4, 4, 4, 4,  3, 3, 1, 1 },          // SIGNAL: a slow call
    }};
    inline const juce::StringArray divisionNames { "1/16", "1/8", "1/16 T", "1/4" };   // never reorder
    inline constexpr std::array<double, 4> divisionBeats { 0.25, 0.5, 1.0 / 6.0, 1.0 };
    // The order steps join in as AMOUNT rises: the pickups (the last of each four) first, the downbeats last.
    inline constexpr std::array<int, numSteps> revealOrder { 15, 7, 11, 3, 13, 5, 9, 1, 14, 6, 10, 2, 12, 4, 8, 0 };
    // The moves CHAOS chooses from.
    inline constexpr std::array<int, 10> chaosMoves { -7, -3, 0, 2, 3, 4, 5, 7, 9, 11 };

    struct Settings
    {
        bool on = false;
        float amount = 0.0f;   // 0 to 1: how many steps move (the macro's)
        int pattern = 0;       // patternNames: ASCEND
        int division = 0;      // divisionNames: 1/16
        int key = 0;           // sirocco::keyNames: C
        int scale = 2;         // arp::scaleNames: Minor
        float chaos = 0.0f;    // 0 to 1
        float glide = 0.0f;    // 0 to 1

        bool moving() const noexcept { return on && amount >= 0.001f; }
        double beatsPerStep() const noexcept { return divisionBeats[(size_t) juce::jlimit (0, (int) divisionBeats.size() - 1, division)]; }
    };

    // Does step `k` (0-15) move at AMOUNT `a`?
    inline bool stepActive (int k, float amount) noexcept
    {
        const auto count = juce::jlimit (0, numSteps, (int) std::floor (juce::jlimit (0.0f, 1.0f, amount) * numSteps + 1.0e-4f));
        for (int r = 0; r < count; ++r)
            if (revealOrder[(size_t) r] == k)
                return true;
        return false;
    }

    // A fixed hash (so a bar always rolls the same).
    inline std::uint32_t hash (std::int64_t a, int b) noexcept
    {
        auto x = (std::uint32_t) (a * 2654435761ll) ^ (std::uint32_t) (b * 40503 + 0x9e3779b9u);
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    // The move (steps of a seven-note scale) at song step `songStep`: the pattern's, or CHAOS's, or 0 where the step
    // doesn't move at this AMOUNT.
    inline int moveAt (const Settings& s, std::int64_t songStep) noexcept
    {
        const auto k = (int) (((songStep % numSteps) + numSteps) % numSteps);
        if (! stepActive (k, s.amount))
            return 0;
        const auto& pattern = patterns[(size_t) juce::jlimit (0, (int) patterns.size() - 1, s.pattern)];
        auto move = pattern[(size_t) k];
        if (s.chaos > 0.0f)
        {
            const auto bar = songStep >= 0 ? songStep / numSteps : (songStep - numSteps + 1) / numSteps;
            const auto h = hash (bar, k);
            if ((float) (h & 0xffff) / 65536.0f < s.chaos)
                move = chaosMoves[(size_t) ((h >> 16) % chaosMoves.size())];
        }
        return move;
    }

    // Steps of the scale for a move written for a seven-note scale.
    inline int scaleSteps (int move7, int scale) noexcept
    {
        std::array<int, 12> notes {};
        const auto count = sirocco::scaleNotes (scale, notes);
        if (count == 7 || count == 0)
            return move7;
        const auto octave = move7 >= 0 ? move7 / 7 : (move7 - 6) / 7;
        const auto rest = move7 - octave * 7;
        return octave * count + juce::jlimit (0, count - 1, juce::roundToInt ((float) rest * (float) count / 7.0f));
    }

    // The semitones `note` moves for `move7` in the scale.
    inline int semitonesFor (int note, int move7, const Settings& s) noexcept
    {
        if (move7 == 0)
            return 0;
        return sirocco::harmonyNote (note, scaleSteps (move7, s.scale), s.key, s.scale) - note;
    }
}
