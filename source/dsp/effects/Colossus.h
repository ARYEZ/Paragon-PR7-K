#pragma once

#include "PitchShifter.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace effects
{
    // COLOSSUS (0.82.0, the owner's choice and name; SANDSEA's signature, option B of two offered (the sound made
    // enormous, over the sound swallowed by a sandstorm), chosen after hearing both sketched from existing effects on
    // one lead; our own design). One knob, SIZE, makes the sound itself huge (unlike LEVIATHAN, which sings a new
    // voice under it):
    //  - UNDER: the sound an octave down (PR7-G's PitchShifter at 0.5, from the middle of the sound), darkened by TONE,
    //    as loud as DEPTH;
    //  - WEIGHT: the sound and its octave driven into a tanh (scaled so a 0.3 level stays 0.3), blended in;
    //  - HALL: a vast eight-line hall (2-10 s by HALL), fed the sound and its octave, swelling in behind each note,
    //    WIDTH by mid / side;
    //  - BOOM: on each new note played (`addNote`, from the synth's own key presses: a sound's level wobbling can't
    //    set one off) a deep impact: a sine falling 72 to 36 Hz over 0.35 s with a low thump of noise, as loud as the
    //    note is hard; notes within 60 ms of one another (a chord, a strum) make one;
    //  - the level kept: the whole (the sound, its octave and the hall; the booms aside) against what went in, power
    //    over 0.3 s, held to 0.35-1.6.
    // SIZE is smoothed (30 ms); under 0.001 it counts as 0 (the hall's output scales with it), and at 0, once the booms
    // have died and the gain is back to 1 for 0.3 s, the sound passes untouched bit for bit (`isResting`) and everything is cleared.
    class Colossus
    {
    public:
        struct Settings
        {
            bool on = false;
            float size = 0.0f;     // 0 to 1 (the macro's)
            float depth = 0.6f;    // 0 to 1: the octave under
            float weight = 0.4f;   // 0 to 1: how hard it's driven
            float hall = 0.6f;     // 0 to 1: 2 to 10 s
            float boom = 0.5f;     // 0 to 1: the impact under each note
            float tone = 0.5f;     // 0 to 1: dark to bright (the octave and the hall)
            float width = 0.7f;    // 0 to 1: the hall's width
        };

        static constexpr int numLines = 8;

        static float hallSeconds (float hall) noexcept { return 2.0f + 8.0f * juce::jlimit (0.0f, 1.0f, hall); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { random = seed != 0 ? seed : 0x5bd1e995u; }
        // A note pressed `offset` samples into the next block to process (velocity 0-1).
        void addNote (int offset, float velocity) noexcept
        {
            if (pendingCount < (int) pending.size())
                pending[(size_t) pendingCount++] = { offset, velocity };
        }
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getSize() const noexcept      { return sizeShown.load (std::memory_order_relaxed); }
        float getUnderLevel() const noexcept { return underShown.load (std::memory_order_relaxed); }   // 0-1
        float getHallLevel() const noexcept { return hallShown.load (std::memory_order_relaxed); }    // 0-1
        float getBoomLevel() const noexcept { return boomShown.load (std::memory_order_relaxed); }    // the last boom, 0-1
        int getBoomsStarted() const noexcept { return booms.load (std::memory_order_relaxed); }

    private:
        float noise() noexcept
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return (float) (random >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }

        double sampleRate = 48000.0;
        std::uint32_t random = 0x5bd1e995u;
        bool resting = true;
        float sizeNow = 0.0f, sizeCoef = 0.0f;

        PitchShifter shifter;
        float underLow = 0.0f, underLow2 = 0.0f;

        // The hall.
        std::array<std::vector<float>, numLines> lines;
        std::array<int, numLines> lengths {}, writes {};
        std::array<float, numLines> lineLow {};
        float swell = 0.0f, swellUp = 0.0f, swellDown = 0.0f;

        // The boom.
        struct PendingNote { int offset = 0; float velocity = 0.0f; };
        std::array<PendingNote, 32> pending {};
        int pendingCount = 0;
        int holdOff = 0;
        float boomAge = 1.0e9f, boomLevel = 0.0f, thumpLow = 0.0f;
        double boomPhase = 0.0;

        // The level.
        float powerIn = 0.0f, powerOut = 0.0f, levelCoef = 0.0f, gain = 1.0f;
        int quietSamples = 0;

        std::atomic<float> sizeShown { 0.0f }, underShown { 0.0f }, hallShown { 0.0f }, boomShown { 0.0f };
        std::atomic<int> booms { 0 };
    };
}
