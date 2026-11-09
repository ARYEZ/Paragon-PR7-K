#pragma once

#include "PitchShifter.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace effects
{
    // HYPERJUMP (0.86.0, the owner's name; HYPERDRIVE's signature in place of LIGHTSPEED, which the owner had removed:
    // "it doesn't have any actual value"; option 1 of three offered; our own design). A build and a drop in time with
    // the song, the riser you'd otherwise draw by hand, always landing on a bar line. One knob, AMOUNT, sets how big:
    //  - THE BUILD lasts LENGTH (1, 2 or 4 bars; never reorder `lengthNames`). Through it:
    //     - CLIMB: the sound steps up a semitone count each beat (PR7-G's `PitchShifter`), reaching RISE (0 to +12
    //       semitones) x AMOUNT on the last beat;
    //     - CHOP: the sound is gated, faster in four stages (quarters, eighths, sixteenths, thirty-seconds), deeper as it
    //       goes (CHOP x AMOUNT), the gate's loss made up from its own pattern (at most +3 dB);
    //     - the lows thin (two high-passes rising to about 800 Hz) and the image narrows towards a point;
    //  - THE DROP, on the bar line: everything snaps back wide in 5 ms, with a HIT: a boom (a sine falling 70 to 38 Hz
    //    over 0.35 s and a 25 ms thump), as loud as the music lets it (none in silence), and a burst of space (four lines
    //    through a Householder mix, about 1.5 s, fed the first quarter second after the drop).
    // MODE (never reorder `modeNames`): LOOP builds in the second half of every 2 x LENGTH bars and drops on the next
    // (with LENGTH 2: builds through bars 3-4, drops on bar 5); ONCE builds from the next bar line after AMOUNT comes up,
    // drops, then stays landed until AMOUNT goes back to 0.
    // In time with `ppq` while the song plays (4/4); its own count while stopped, starting a build at once when it
    // wakes. AMOUNT is smoothed (30 ms); under 0.001 it counts as 0, and at 0, once the build has faded, the boom has
    // died and the space has rung out, the sound passes untouched bit for bit (`isResting`).
    class HyperJump
    {
    public:
        static inline const juce::StringArray lengthNames { "1 BAR", "2 BARS", "4 BARS" };   // never reorder
        static constexpr std::array<int, 3> lengthBars { 1, 2, 4 };
        static inline const juce::StringArray modeNames { "LOOP", "ONCE" };                  // never reorder

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;   // 0 to 1: how big (the macro's)
            int length = 1;        // lengthNames: 2 BARS
            int mode = 0;          // modeNames: LOOP
            float rise = 0.5f;     // 0 to 1: the climb's top, 0 to +12 semitones
            float chop = 0.6f;     // 0 to 1: how deep the gate
            float hit = 0.6f;      // 0 to 1: the drop's boom and space
        };

        static constexpr float maxRiseSemitones = 12.0f;
        // The climb in semitones on beat `beat` of a build `beats` long (whole semitones, the last beat at the top).
        static float climbFor (int beat, int beats, float rise, float amount) noexcept
        {
            if (beats <= 1)
                return 0.0f;
            const auto top = maxRiseSemitones * juce::jlimit (0.0f, 1.0f, rise) * juce::jlimit (0.0f, 1.0f, amount);
            return std::round (top * (float) juce::jlimit (0, beats - 1, beat) / (float) (beats - 1));
        }
        // The chop's step in beats at build progress `p` (0 to 1): quarters, eighths, sixteenths, thirty-seconds.
        static double chopBeats (double p) noexcept
        {
            constexpr double steps[] { 1.0, 0.5, 0.25, 0.125 };
            return steps[juce::jlimit (0, 3, (int) std::floor (p * 4.0))];
        }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { random = seed != 0 ? seed : 0x5bd1e995u; }
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getAmount() const noexcept     { return amountShown.load (std::memory_order_relaxed); }
        float getProgress() const noexcept   { return progressShown.load (std::memory_order_relaxed); }   // 0-1 through the build, -1 not building
        float getClimb() const noexcept      { return climbShown.load (std::memory_order_relaxed); }      // semitones now
        float getBeatsToDrop() const noexcept { return toDropShown.load (std::memory_order_relaxed); }    // -1: none coming
        float getHitLevel() const noexcept   { return hitShown.load (std::memory_order_relaxed); }        // the boom now, 0-1
        int getDrops() const noexcept        { return dropsShown.load (std::memory_order_relaxed); }
        int getBuildBars() const noexcept    { return barsShown.load (std::memory_order_relaxed); }

    private:
        float noise() noexcept
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return (float) (random >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }
        // Where the build is at song position `beats`: progress 0-1 (-1 not building), and whether a drop falls here.
        double buildAt (double beats, const Settings&, double buildBeats, bool& drop) noexcept;
        void startDrop() noexcept;

        double sampleRate = 48000.0;
        std::uint32_t random = 0x5bd1e995u;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f, snapCoef = 0.0f, levelCoef = 0.0f;
        float target = 0.0f;
        double ownBeats = 0.0, lastBeats = -1.0e9;
        bool wasBuilding = false;
        // ONCE: idle, waiting for the bar line, building, landed.
        enum class Once { idle, waiting, building, landed };
        Once once = Once::idle;
        double onceStart = 0.0;

        // The build's shaping, glided: how much of it (5 ms), the shifter's share.
        float shapeMix = 0.0f, shiftMix = 0.0f;
        std::array<PitchShifter, 2> shifters;
        std::array<std::array<float, 2>, 2> highPass {};   // [side][stage]
        float hpCoef = 0.0f;
        int coefCountdown = 0;
        float follower = 0.0f, followUp = 0.0f, followDown = 0.0f;

        // The drop.
        float dropAge = 1.0e9f, hitLevel = 0.0f, boomPhase = 0.0f, thump = 0.0f, thumpCoef = 0.0f;
        std::array<std::vector<float>, 4> lines;
        std::array<int, 4> lineLength {}, lineIndex {};
        std::array<float, 4> damp {};
        float dampCoef = 0.0f, spaceLevel = 0.0f, tailEnergy = 0.0f;
        int quietSamples = 0;

        std::atomic<float> amountShown { 0.0f }, progressShown { -1.0f }, climbShown { 0.0f }, toDropShown { -1.0f }, hitShown { 0.0f };
        std::atomic<int> dropsShown { 0 }, barsShown { 2 };
    };
}
