#pragma once

#include "EffectUtils.h"

#include <array>
#include <atomic>

namespace effects
{
    // INFERNAL (0.59.0, the HELLFORGE expansion's signature, the owner's name): parallel destruction, after the way the
    // owner's reference scores were described being made (clean sine subs and noise through several chains of pedals
    // at once, mixed back together). Our own design, not a model of any gear.
    //
    // The sound is split at TIGHT: everything under it stays clean (the sub under the dirt), everything over it goes
    // through four chains side by side, each with its own blend, then a glue compressor:
    //  - FUZZ:  a hard, lopsided fuzz (first-order anti-aliased tanh with a bias, so it buzzes rather than fizzes).
    //  - CRUSH: few bits and a lowered sample rate: digital grind.
    //  - AMP:   a guitar amp's two gain stages with a mid push, into a speaker cabinet (low cut, a resonant fall above
    //           CAB, a cabinet's bump round 1.8 kHz): what makes a synth lead sound played on a guitar. FEEDBACK adds
    //           a bloom an octave over the notes held, swelling in as they sustain, as a guitar feeds back.
    //  - NOISE: noise pulsed at the pitch of the notes sounding (bursts once a cycle), so the noise growls in tune and
    //           in time with the riff instead of hissing.
    // DRIVE pushes every chain; MIX blends the whole (0: the dry sound exactly); LEVEL the destroyed part. The notes
    // come from the synth (`setNotes`), as DOWNPOUR's do. Silence in is silence out.
    class Infernal
    {
    public:
        struct Settings
        {
            bool on = false;
            float drive = 0.5f;      // 0 to 1: 0 to +42 dB into the chains
            float tight = 0.4f;      // 0 to 1: the clean split, 30 Hz to 400 Hz
            float fuzz = 0.6f;       // 0 to 1: each chain's blend
            float crush = 0.0f;
            float amp = 0.8f;
            float noise = 0.3f;
            float feedback = 0.3f;   // 0 to 1: the AMP's bloom
            float cab = 0.5f;        // 0 dark (2.5 kHz) to 1 bright (7.5 kHz)
            float mix = 0.0f;        // 0 to 1
            float levelDb = 0.0f;    // -24 to +12: the destroyed part
        };

        static constexpr int maxNotes = 4;
        static constexpr int numChains = 4;   // FUZZ, CRUSH, AMP, NOISE

        static float tightHz (float tight) noexcept;
        static float cabHz (float cab) noexcept;
        static float driveGain (float drive) noexcept;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { for (int c = 0; c < 2; ++c) states[(size_t) c] = (seed == 0 ? 0x9e3779b9u : seed) + 7919u * (std::uint32_t) c; }

        // The notes sounding now (MIDI notes); the lowest four are used. With none, the last ones stay.
        void setNotes (const float* notes, int count) noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;

        // For the page: each chain's level after its blend (dB, -100 when silent) and the bloom (0 to 1).
        float getChainDb (int chain) const noexcept { return chainDb[(size_t) juce::jlimit (0, numChains - 1, chain)].load (std::memory_order_relaxed); }
        float getBloom() const noexcept { return bloomShown.load (std::memory_order_relaxed); }
        float getGainReductionDb() const noexcept { return reductionShown.load (std::memory_order_relaxed); }
        bool isResting() const noexcept { return lastMix <= 0.0f; }   // MIX 0 and settled: the sound passes untouched

    private:
        // tanh with first-order anti-aliasing (the antiderivative's difference), in double.
        struct SoftClip
        {
            double last = 0.0;
            static double logCosh (double x) noexcept
            {
                const auto a = std::abs (x);
                return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531;
            }
            float process (double x) noexcept
            {
                const auto d = x - last;
                const auto y = std::abs (d) < 1.0e-5 ? std::tanh (0.5 * (x + last)) : (logCosh (x) - logCosh (last)) / d;
                last = x;
                return (float) y;
            }
        };

        struct Channel
        {
            float split = 0.0f, split2 = 0.0f;   // TIGHT: two one-pole low-passes
            SoftClip fuzzClip, ampClip1, ampClip2, noiseClip, crushClip;
            float fuzzTone = 0.0f, noiseTone = 0.0f;
            float crushHeld = 0.0f;
            int crushCount = 0;
            float cabLow = 0.0f;                 // the cab's low cut
            juce::dsp::StateVariableTPTFilter<float> mid, cab, bump;
        };

        float random (int c) noexcept   // xorshift32, -1 to 1
        {
            auto& s = states[(size_t) c];
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            return (float) (s >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }
        void quietReset() noexcept;

        double sampleRate = 48000.0;
        BypassFade bypass;
        std::array<std::uint32_t, 2> states { 0x9e3779b9u, 0x85ebca6bu };
        std::array<Channel, 2> channels;
        std::array<float, maxNotes> notes {};
        int numNotes = 0;
        std::array<double, maxNotes> notePhase {}, bloomPhase {};
        float follower = 0.0f, followAttack = 0.0f, followRelease = 0.0f;
        float bloom = 0.0f, bloomUp = 0.0f, bloomDown = 0.0f;
        float glueEnvelope = 0.0f, glueAttack = 0.0f, glueRelease = 0.0f;
        float lastMix = 0.0f;
        int crushStep = 4;
        int quietSamples = 0;
        float lastCabHz = -1.0f;
        std::array<std::atomic<float>, numChains> chainDb {};
        std::atomic<float> bloomShown { 0.0f }, reductionShown { 0.0f };
    };
}
