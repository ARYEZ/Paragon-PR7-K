#pragma once

#include "EffectUtils.h"
#include "dsp/synth/Cartridge.h"

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // CARTRIDGE's audio half (1.10.0; the engine's half, synth::cartridge, plays the chord as turns, snaps the tuning
    // and steps the loudness). Our own design, no console's circuit modelled.
    //  - CRUNCH (from AMOUNT 0.5, all of it at 1): the console's coarse output: each sample held for up to four samples
    //    (the sound read at as little as a quarter of the rate, so its top folds back as the chip's did) and stepped to
    //    as few as 32 levels a side (`levelsFor`).
    //  - GLITCH (from AMOUNT 0.75, all of it at 1): a wiggled cartridge: now and then (up to `maxGlitchesPerSecond`) the
    //    last frame (1/60 s) is repeated two to five times over what's playing, then the sound comes back.
    // Below AMOUNT 0.5, or off, untouched bit for bit (`isResting`). Random by xorshift (`seedRandom`).
    class Cartridge
    {
    public:
        static constexpr float maxGlitchesPerSecond = 2.5f;
        static int holdFor (float crunch) noexcept { return 1 + juce::roundToInt (3.0f * juce::jlimit (0.0f, 1.0f, crunch)); }
        static float levelsFor (float crunch) noexcept { return std::exp2 (12.0f - 7.0f * juce::jlimit (0.0f, 1.0f, crunch)); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { noise = seed == 0 ? 0x9e3779b9u : seed; }
        void process (juce::dsp::AudioBlock<float> block, const synth::cartridge::Settings&) noexcept;
        bool isResting() const noexcept { return resting; }
        int getGlitches() const noexcept { return glitches.load (std::memory_order_relaxed); }
        bool isGlitching() const noexcept { return glitching.load (std::memory_order_relaxed); }

    private:
        float random01() noexcept;
        double sampleRate = 48000.0;
        int frame = 800;
        std::array<std::vector<float>, 2> history;   // the last frame, each side
        int write = 0;
        std::array<std::vector<float>, 2> repeat;    // the frame being repeated
        int repeatLeft = 0, repeatPos = 0;
        std::array<float, 2> heldValue {};
        int holdCount = 0;
        bool resting = true;
        std::uint32_t noise = 0x9e3779b9u;
        std::atomic<int> glitches { 0 };
        std::atomic<bool> glitching { false };
    };
}
