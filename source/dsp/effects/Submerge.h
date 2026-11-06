#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // SUBMERGE (0.80.0, the owner's name; DROWNED DECO's signature, option 1 of two offered (the sound sinking under
    // water, over the sound trapped in a drowned hull), chosen after hearing both sketched on one lead; our own design,
    // no water measured or modelled). One knob, DEPTH, takes the sound from dry land to the sea floor:
    //  - the water closes over it: the sound through a 12 dB low-pass whose cutoff falls from 20 kHz to MURK's 4 kHz-
    //    400 Hz, blended in by DEPTH (at 0 the plain sound);
    //  - pressure: the sound read from a short delay that wanders unevenly (two slow sines at unrelated rates plus a
    //    random drift), so the pitch bends up to about +-20 cents at full CURRENT, never in a steady rhythm; the delay
    //    is DEPTH times the wander, so at 0 there's no delay at all (no jump turning it up);
    //  - bubbles: little sines rising in pitch as they go (a bubble's note climbs as it surfaces), 300-1400 Hz, rising
    //    by half again over 20-70 ms, at random (up to 14 a second at full BUBBLES and DEPTH), each its own place across
    //    WIDTH, as loud as the music lets them (a follower: silence stays silent);
    //  - the sea: a dark, wide wash (four long lines through a Householder mix, a 1.6 kHz one-pole in the loop, about
    //    4 s to die away) fed the darkened sound, at SEA's level times DEPTH.
    // The level is kept: power in and out (before the bubbles) followed over 0.3 s, the gain held to -6 to +4 dB and
    // glided. DEPTH is smoothed (30 ms); under 0.001 it counts as 0, and at 0, once the gain is back to 1 for 0.3 s and
    // the bubbles are gone, the sound passes untouched bit for bit (`isResting`) and everything is cleared.
    class Submerge
    {
    public:
        static constexpr int numLines = 4, maxBubbles = 12;

        struct Settings
        {
            bool on = false;
            float depth = 0.0f;     // 0 to 1: dry land to the sea floor (the macro's)
            float murk = 0.5f;      // 0 to 1: how dark at full depth, 4 kHz down to 400 Hz
            float current = 0.5f;   // 0 to 1: how far the pressure bends the pitch, up to about +-20 cents
            float bubbles = 0.5f;   // 0 to 1: how many bubbles rise
            float sea = 0.5f;       // 0 to 1: the sea's wash around the sound
            float width = 0.7f;     // 0 to 1: how wide the bubbles and the sea
        };

        static float murkHz (float m) noexcept { return 4000.0f * std::pow (0.1f, juce::jlimit (0.0f, 1.0f, m)); }
        static float bubbleRate (float bubbles, float depth) noexcept { return 14.0f * juce::jlimit (0.0f, 1.0f, bubbles) * juce::jlimit (0.0f, 1.0f, depth); }
        static constexpr float maxCents = 20.0f;   // CURRENT's pitch bend at full (about: the wander's steepest)

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { random = seed != 0 ? seed : 0x6d2b79f5u; }
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getDepth() const noexcept     { return depthShown.load (std::memory_order_relaxed); }
        float getSeaLevel() const noexcept  { return seaShown.load (std::memory_order_relaxed); }   // 0-1
        float getGainDb() const noexcept    { return gainShown.load (std::memory_order_relaxed); }
        float getBendCents() const noexcept { return bendShown.load (std::memory_order_relaxed); }  // the pressure's bend now
        int getBubblesStarted() const noexcept { return started.load (std::memory_order_relaxed); }   // a count, for tests
        // Bubble k now: its place across (-1 to 1) and how far it has risen (0-1; below 0 none).
        float getBubblePan (int k) const noexcept  { return bubblePan[(size_t) k].load (std::memory_order_relaxed); }
        float getBubbleRise (int k) const noexcept { return bubbleRise[(size_t) k].load (std::memory_order_relaxed); }

    private:
        float uniform() noexcept
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return (float) (random >> 8) * (1.0f / 16777216.0f);
        }

        struct Bubble
        {
            bool active = false;
            double phase = 0.0;
            float hz = 0.0f, rise = 0.0f, age = 0.0f, life = 0.0f, gainL = 0.0f, gainR = 0.0f, level = 0.0f, pan = 0.0f;
        };

        double sampleRate = 48000.0;
        std::uint32_t random = 0x6d2b79f5u;
        bool resting = true;
        float depthNow = 0.0f, depthCoef = 0.0f;

        // The pressure: a short delay per side, its length wandering.
        std::array<std::vector<float>, 2> delay;
        int delaySize = 0, delayWrite = 0;
        double wanderPhase1 = 0.0, wanderPhase2 = 0.0;
        float drift = 0.0f, driftTarget = 0.0f, driftCountdown = 0.0f, lastDelay = 0.0f;

        // The water: two one-poles a side.
        std::array<float, 2> darkA {}, darkB {};

        // The sea: four long lines.
        std::array<std::vector<float>, numLines> lines;
        std::array<int, numLines> writes {}, lengths {};
        int lineSize = 0;
        std::array<float, numLines> loopTone {};
        float seaPeak = 0.0f;

        // The bubbles.
        std::array<Bubble, maxBubbles> bubbles {};
        float follower = 0.0f, followUp = 0.0f, followDown = 0.0f, untilNext = 0.0f;

        float powerIn = 0.0f, powerOut = 0.0f, levelCoef = 0.0f, gainNow = 1.0f;
        int quietSamples = 0;

        std::atomic<float> depthShown { 0.0f }, seaShown { 0.0f }, gainShown { 0.0f }, bendShown { 0.0f };
        std::atomic<int> started { 0 };
        std::array<std::atomic<float>, maxBubbles> bubblePan {}, bubbleRise {};
    };
}
