#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <cstdint>

// CARTRIDGE (1.10.0, GAME OVER's signature, the owner's choice after hearing a sketch; our own design, no console's
// chip modelled from a schematic). The synth forced through the limits of an old game console's sound chip. Two halves:
// this one in the engine (what's played), and effects::Cartridge after it (the crunch and the glitches).
// AMOUNT (MACRO 3) steps through the chip's limits, a stage at a time:
//  - from 0.001: the tuning snaps to the chip's coarse grid (`chipHz`: a 1.789773 MHz clock divided by a whole number,
//    so high notes come out audibly off), and each note's loudness moves in 16 steps instead of smoothly;
//  - from `arpFrom` 0.25: the chip can't sound a chord, so held notes take turns, one at a time, in time with the song
//    at RATE (`rateNames` 1/16, 1/32, 1/64), from the lowest up: the chord becomes an arpeggio;
//  - from `framesFrom` 0.75: the turns run at the console's 60 Hz frame rate instead (the buzzing "chip chord"), the
//    loudness changes only once a frame, and only the newest CHANNELS notes sound (2-8; a fifth note steals one);
//  - the crunch (from 0.5) and the glitches (from 0.75) are effects::Cartridge's.
// Off, or AMOUNT under 0.001: nothing touched, bit for bit. SIROCCO's answers and release tails aren't arpeggiated.
namespace synth::cartridge
{
    inline const juce::StringArray rateNames { "1/16", "1/32", "1/64" };   // never reorder
    inline constexpr float arpFrom = 0.25f, framesFrom = 0.75f;
    inline constexpr double frameHz = 60.0;
    inline constexpr double clockHz = 1789773.0;
    inline constexpr double fadeSeconds = 0.0005;   // a turn's edge (a hard edge clicks harder than the chip did)

    struct Settings
    {
        bool on = false;
        float amount = 0.0f;   // 0 to 1 (the macro's)
        int rate = 1;          // rateNames: 1/32
        int channels = 4;      // 2 to 8: notes at once at the top stage
        float crunch = 0.6f;   // 0 to 1: effects::Cartridge's DAC and sample rate
        float glitch = 0.5f;   // 0 to 1: effects::Cartridge's wiggled-cartridge repeats

        bool active() const noexcept { return on && amount >= 0.001f; }
        bool turns() const noexcept { return active() && amount >= arpFrom; }
        bool frames() const noexcept { return active() && amount >= framesFrom; }
        double beatsPerTurn() const noexcept { return 0.25 / (double) (1 << juce::jlimit (0, rateNames.size() - 1, rate)); }
        int voiceLimit() const noexcept { return frames() ? juce::jlimit (2, 8, channels) : 64; }
        float crunchNow() const noexcept { return active() ? crunch * juce::jlimit (0.0f, 1.0f, (amount - 0.5f) * 2.0f) : 0.0f; }
        float glitchNow() const noexcept { return active() ? glitch * juce::jlimit (0.0f, 1.0f, (amount - framesFrom) * 4.0f) : 0.0f; }
    };

    // The chip's nearest pitch to a note (a clock divided by 16 (t + 1), t whole, at most 11 bits): below what 11 bits
    // reach, the note as it is.
    inline double chipHz (double note) noexcept
    {
        const auto hz = 440.0 * std::exp2 ((note - 69.0) / 12.0);
        const auto t = std::round (clockHz / (16.0 * hz) - 1.0);
        if (t > 2047.0 || t < 1.0)
            return hz;
        return clockHz / (16.0 * (t + 1.0));
    }
    // ...as semitones from the note.
    inline float tuningSemis (double note) noexcept
    {
        return (float) (12.0 * std::log2 (chipHz (note) / (440.0 * std::exp2 ((note - 69.0) / 12.0))));
    }
    // A loudness in 16 steps.
    inline float stepped (float level) noexcept { return std::round (level * 15.0f) / 15.0f; }
}
