#pragma once

#include "EffectUtils.h"

#include <array>
#include <atomic>

namespace effects
{
    // CRACKLE (0.56.0, the MELTDOWN expansion's signature, the owner's ask): the crackle of a radiation counter,
    // added to the sound. Our own design. Clicks come at random moments, as a counter's do (a Poisson process: each
    // sample has the same small chance of a click, so there's no rhythm in them), RATE of them a second on average.
    //
    //  - CLUSTER: the rate wanders slowly, up and down by up to about 12 times, so clicks bunch into bursts and lulls.
    //  - TONE:    each click is a tiny burst of noise (about a millisecond) through a band-pass at TONE: a dull tick low,
    //             a sharp snap high.
    //  - WIDTH:   how far each click is placed across left and right (each its own place).
    //  - FOLLOW:  how much the clicks follow the sound coming in: at 1, they come only while it plays (and as fast as
    //             it's loud), so a preset still falls silent; at 0, they never stop.
    //  - LEVEL:   how loud the clicks are, added to the sound (which passes unchanged).
    class Crackle
    {
    public:
        struct Settings
        {
            bool on = false;
            float rate = 20.0f;      // clicks a second, on average (0.5 to 2000)
            float cluster = 0.4f;    // 0 to 1
            float toneHz = 3000.0f;  // 300 to 12000
            float width = 0.7f;      // 0 to 1
            float follow = 1.0f;     // 0 to 1
            float levelDb = -12.0f;  // -48 to +6
        };

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { state = seed == 0 ? 0x9e3779b9u : seed; }

        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;

        // For the page and the unit: clicks counted in the last `binSeconds` slices (newest last), and the average
        // rate over the last second.
        static constexpr int numBins = 128;
        static constexpr double binSeconds = 0.02;
        std::array<int, numBins> getHistory() const noexcept;
        float getRecentRate() const noexcept;
        // The density the clicks follow now (RATE x CLUSTER's wander x FOLLOW's share), clicks a second.
        float getCurrentRate() const noexcept { return currentRate.load (std::memory_order_relaxed); }
        long long getTotalClicks() const noexcept { return totalClicks.load (std::memory_order_relaxed); }

    private:
        float random01() noexcept   // xorshift32, 0 to 1
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) (state >> 8) * (1.0f / 16777216.0f);
        }
        void startClick (const Settings&) noexcept;

        double sampleRate = 48000.0;
        BypassFade bypass;
        std::uint32_t state = 0x9e3779b9u;

        // The clicks sounding: noise bursts each with its own level and place.
        struct Click { float level = 0.0f, gainL = 0.0f, gainR = 0.0f; int left = 0; };
        std::array<Click, 16> clicks {};
        float burstDecay = 0.0f;   // per sample
        int burstSamples = 0;

        // CLUSTER's wander: a slow random walk between targets, in log rate.
        float wander = 0.0f, wanderTarget = 0.0f, wanderStep = 0.0f;
        int wanderLeft = 0;

        float follower = 0.0f, followAttack = 0.0f, followRelease = 0.0f;
        std::array<juce::dsp::StateVariableTPTFilter<float>, 2> tone;   // per side
        float lastToneHz = -1.0f;

        // Display.
        std::array<std::atomic<int>, numBins> bins {};
        std::atomic<int> binIndex { 0 };
        int binSamplesLeft = 0;
        std::atomic<float> currentRate { 0.0f };
        std::atomic<long long> totalClicks { 0 };
    };
}
