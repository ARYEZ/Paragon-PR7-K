#pragma once

#include <juce_dsp/juce_dsp.h>

#include "PitchShifter.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // OVERCHARGE (0.77.0, the owner's name; TWIN SPIRES' signature, option 2 of two offered (unstable magic over a
    // descent from a gilded city into a grimy one), the owner's choice after hearing both sketched on one lead; our own
    // design, nothing modelled or copied). One knob, CHARGE, takes the sound from rest through two stages:
    //  - the first half CHARGES it: crystal tones ring above the notes sounding (an octave, a fifth over that and two
    //    octaves up, from `setNotes`, so always in tune, shimmering, following how loud you play), and shimmering
    //    repeats climb an octave each (a delay with an octave-up shifter in its loop);
    //  - the second half OVERLOADS it: the energy goes unstable in bursts at random moments, more often and more
    //    violent with CHAOS: STUTTERS (a sliver of the last moments repeated), CRUSH (held samples, few bits) and JOLTS
    //    (the pitch thrown up or down for a moment), plus a burn (tanh drive) and sparks (clicks of noise).
    // GLOW sets the crystal tones' share, CHAOS the overload's violence and how often it bursts, TONE the glow's top,
    // WIDTH how far the glow spreads. The level is kept: power in and out followed over about 0.3 s, the gain held to
    // -6 to +3 dB and glided, so it gets wilder, not louder. CHARGE is smoothed (30 ms); under 0.001 it counts as 0, and
    // at 0, once the repeats and crystals have died away, the sound passes untouched bit for bit (`isResting`), with
    // everything cleared so it wakes clean. Random choices by xorshift (`seedRandom`; the plugin's `seedRandomForTest`).
    class Overcharge
    {
    public:
        static constexpr int maxNotes = 8;
        static constexpr int partials = 3;        // per note: an octave, an octave and a fifth, two octaves up
        static constexpr int maxCrystals = maxNotes * partials;

        struct Settings
        {
            bool on = false;
            float charge = 0.0f;   // 0 to 1: the macro's (the first half charges, the second overloads)
            float glow = 0.6f;     // 0 to 1: the crystal tones' share
            float chaos = 0.6f;    // 0 to 1: how violent and how often the overload bursts
            float tone = 0.6f;     // 0 to 1: the glow's top, 3 to 16 kHz
            float width = 0.7f;    // 0 to 1: how far the glow spreads
        };

        // The two stages of CHARGE: the charge (0-1 over its first half) and the overload (0-1 over its second).
        static float chargeOf (float charge) noexcept   { return juce::jlimit (0.0f, 1.0f, charge * 2.0f); }
        static float overloadOf (float charge) noexcept { return juce::jlimit (0.0f, 1.0f, charge * 2.0f - 1.0f); }
        // Bursts a second at a given overload and CHAOS (0 below the halfway point).
        static float burstRate (float overload, float chaos) noexcept
        {
            return overload <= 0.0f ? 0.0f : overload * (1.5f + 10.0f * juce::jlimit (0.0f, 1.0f, chaos));
        }
        static float toneHz (float t) noexcept { return 3000.0f * std::pow (16.0f / 3.0f, juce::jlimit (0.0f, 1.0f, t)); }
        // The crystals' intervals above the note, semitones, and their levels.
        static constexpr float partialSemis[partials] { 12.0f, 19.0f, 24.0f };
        static constexpr float partialLevel[partials] { 1.0f, 0.6f, 0.45f };

        enum class Burst { none, stutter, crush, jolt };

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { random = seed != 0 ? seed : 0x9e3779b9u; }
        void setNotes (const float* pitches, int count) noexcept;   // the notes sounding (MIDI pitches)
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getCharge() const noexcept   { return chargeShown.load (std::memory_order_relaxed); }
        float getOverload() const noexcept { return overloadShown.load (std::memory_order_relaxed); }
        float getGlow() const noexcept     { return glowShown.load (std::memory_order_relaxed); }    // the crystals' level now, 0-1
        float getFlash() const noexcept    { return flashShown.load (std::memory_order_relaxed); }   // 1 at a burst, fading
        int getBurstCount() const noexcept { return burstCount.load (std::memory_order_relaxed); }   // since reset
        Burst getBurst() const noexcept    { return (Burst) burstShown.load (std::memory_order_relaxed); }

    private:
        float next01() noexcept
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return (float) (random >> 8) * (1.0f / 16777216.0f);
        }
        float readHistory (int side, double delaySamples) const noexcept;
        void startBurst (float chaos) noexcept;

        double sampleRate = 48000.0;
        std::uint32_t random = 0x9e3779b9u;
        bool resting = true;
        float chargeNow = 0.0f, smoothCoef = 0.0f;

        // The notes and their crystals: each a sine at its pitch with a shimmer, its gain gliding to its target.
        struct Crystal
        {
            float hz = 0.0f, gain = 0.0f, target = 0.0f, pan = 0.0f;
            double phase = 0.0, wobble = 0.0;
            float wobbleHz = 6.0f, step = 0.0f;   // its phase step a sample, the wobble worked in every 32 samples
        };
        std::array<Crystal, maxCrystals> crystals {};
        std::array<float, maxNotes> notes {};
        int numNotes = 0;
        int wobbleCount = 0;
        float follower = 0.0f, followUp = 0.0f, followDown = 0.0f, crystalCoef = 0.0f;

        // The shimmer: per side the sound and the fed-back repeats through an octave-up shifter (PitchShifter, as
        // LIGHTTRAIL's: it splices where the waves line up, so it doesn't warble), then a delay before it's heard and fed
        // back.
        std::array<PitchShifter, 2> shifters;
        std::vector<float> echoLine[2];
        int echoMask = 0, echoWrite = 0, echoDelay = 0;
        std::array<float, 2> shimmerTone {}, glowTone {};

        // The overload: per side the last second heard (for stutters and jolts), the burst under way.
        std::vector<float> history[2];
        int historyMask = 0, historyWrite = 0;
        Burst burst = Burst::none;
        int burstLeft = 0, burstLength = 0, sliceLength = 0, slicePos = 0, sliceStart = 0, holdEvery = 1, holdCount = 0;
        float burstDepth = 0.0f, crushLevels = 16.0f, joltRatio = 1.0f;
        double joltDelay = 0.0;
        std::array<float, 2> held {};
        float sparkLeft = 0.0f, sparkGain = 0.0f;
        std::array<float, 2> sparkState {};

        float powerIn = 0.0f, powerOut = 0.0f, levelCoef = 0.0f, gainNow = 1.0f;
        int quietSamples = 0;

        std::atomic<float> chargeShown { 0.0f }, overloadShown { 0.0f }, glowShown { 0.0f }, flashShown { 0.0f };
        std::atomic<int> burstCount { 0 }, burstShown { 0 };
        float flash = 0.0f, flashCoef = 0.0f;
    };
}
