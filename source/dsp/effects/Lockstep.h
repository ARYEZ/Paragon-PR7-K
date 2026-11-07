#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace effects
{
    // LOCKSTEP (0.81.0, the owner's name; NEW EDEN's signature, option 2 of two offered (the sound forced into lockstep,
    // over the sound taken into a church), chosen after hearing both sketched on one lead; our own design). One knob,
    // AMOUNT, drills the sound into a uniform:
    //  - SQUEEZE: the sound driven into a tanh (up to 8x) and brought back to its own level (scaled so 0.3 stays 0.3, then
    //    power in and out followed over 0.3 s, held to -12 to +12 dB): flattened, every note the same;
    //  - the march: cut into steps in time with the song (DIVISION 1/4, 1/8, 1/16, 1/8 T: never reorder
    //    `divisionNames`), each open for GATE of the step (1.5 ms raised-cosine edges in, 6 ms out), the steps off the
    //    beat ACCENT lower (down to 0.4); the cut's loss made up from the pattern itself (its average power over two
    //    steps, at most +6 dB), not from a follower that would pump;
    //  - STAMP: on every step a boot on stone: a 6 ms crack of band-passed noise and a 70 Hz thud over 45 ms, the off
    //    beats lower, as loud as the music lets it (a follower 10 ms up, 0.4 s down; none in silence).
    // In time with `ppq` while the song plays, its own count while stopped. AMOUNT is smoothed (30 ms); under 0.001 it
    // counts as 0, and at 0, once the stamps have died and the make-up is back to 1 for 0.3 s, the sound passes
    // untouched bit for bit (`isResting`) and everything is cleared.
    class Lockstep
    {
    public:
        static inline const juce::StringArray divisionNames { "1/4", "1/8", "1/16", "1/8 T" };
        static constexpr std::array<double, 4> divisionBeats { 1.0, 0.5, 0.25, 1.0 / 3.0 };

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;    // 0 to 1 (the macro's)
            int division = 1;       // divisionNames: 1/8
            float gate = 0.5f;      // 0 to 1: how long each step is open, 15% to 90% of it
            float accent = 0.5f;    // 0 to 1: the off-beat steps lower
            float squeeze = 0.5f;   // 0 to 1: how flat
            float stamp = 0.5f;     // 0 to 1: the boots
        };

        static float gateShare (float g) noexcept { return 0.15f + 0.75f * juce::jlimit (0.0f, 1.0f, g); }
        static float offBeatLevel (float accent) noexcept { return 1.0f - 0.6f * juce::jlimit (0.0f, 1.0f, accent); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { random = seed != 0 ? seed : 0x7f4a7c15u; }
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getAmount() const noexcept    { return amountShown.load (std::memory_order_relaxed); }
        double getSteps() const noexcept    { return stepsShown.load (std::memory_order_relaxed); }    // the song in steps
        float getStampLevel() const noexcept { return stampShown.load (std::memory_order_relaxed); }  // the last stamp, 0-1
        float getMakeUpDb() const noexcept  { return makeUpShown.load (std::memory_order_relaxed); }
        int getStampsStarted() const noexcept { return stamps.load (std::memory_order_relaxed); }

    private:
        float noise() noexcept
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return (float) (random >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }
        // The march's level at `steps` (the song in steps), at AMOUNT `a`.
        float marchLevel (double steps, float a, double stepSeconds, float gate, float offBeat, double beatsPerStep) const noexcept;

        double sampleRate = 48000.0;
        std::uint32_t random = 0x7f4a7c15u;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f;
        double ownBeats = 0.0, lastStep = -1.0;

        float powerIn = 0.0f, powerOut = 0.0f, levelCoef = 0.0f, squeezeGain = 1.0f;
        float makeUpNow = 1.0f;

        // The boots.
        float stampAge = 1.0e9f, stampLevel = 0.0f;
        double thudPhase = 0.0;
        float crackLow = 0.0f, crackHigh = 0.0f;
        float follower = 0.0f, followUp = 0.0f, followDown = 0.0f;
        int quietSamples = 0;

        std::atomic<float> amountShown { 0.0f }, stampShown { 0.0f }, makeUpShown { 0.0f };
        std::atomic<double> stepsShown { 0.0 };
        std::atomic<int> stamps { 0 };
    };
}
