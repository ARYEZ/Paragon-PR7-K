#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

namespace effects
{
    // TRANCE GATE (1.7.0, the owner's choice of two offered: a gate of its own at the very end of the effects, rather
    // than a switch on the STEP SEQUENCER; our own design, no product modelled). The classic stepped volume gate, in time
    // with the song, chopping the finished sound: the reverb, the echoes and every signature effect with it (the
    // STEP SEQUENCER works inside the synth, so its gaps fill with the effects' tails; this one cuts them).
    //  - PATTERN: up to 32 steps, each a level (0 silent, 1 full, between a softer step): `Settings::left`; with STEREO
    //    the right side follows its own row (`right`), so the gate can bounce between the speakers.
    //  - DIVISION (never reorder `divisionNames`): each step's length, 1/8 to 1/32 and the triplets; LENGTH: how many
    //    steps before the pattern comes round (1-32); SWING: every second step starts up to half a step late.
    //  - GATE: how much of each step is open (5% to all of it: at 100% neighbouring open steps run together).
    //  - ATTACK (0.5-40 ms) and RELEASE (1-300 ms): how fast it opens and closes, a hard chop to a soft pump.
    //  - MIX: how deep it cuts (0 nothing, 1 the closed steps silent).
    // In time with `ppq` while the song plays (4/4, from the bar line); its own count while stopped. Each side its own
    // envelope (one-poles, the ATTACK coefficient rising, the RELEASE falling). MIX is smoothed over 30 ms and snapped
    // once within 1e-4 (a float glide can stall a hair short); at 0 (or off) the sound passes untouched bit for bit
    // (`isResting`).
    class TranceGate
    {
    public:
        static constexpr int maxSteps = 32;
        static inline const juce::StringArray divisionNames { "1/8", "1/16", "1/32", "1/8 T", "1/16 T" };   // never reorder
        static constexpr std::array<double, 5> divisionBeats { 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
        static inline const juce::StringArray stereoNames { "LINKED", "STEREO" };   // never reorder

        struct Settings
        {
            bool on = false;
            float mix = 1.0f;       // 0 to 1
            int division = 1;       // divisionNames: 1/16
            int length = 16;        // steps, 1 to 32
            float gate = 0.75f;     // 0.05 to 1: the share of each step open
            float attack = 0.15f;   // 0 to 1: 0.5 to 40 ms
            float release = 0.3f;   // 0 to 1: 1 to 300 ms
            float swing = 0.0f;     // 0 to 1: every second step up to half a step late
            int stereo = 0;         // stereoNames: LINKED (the left row on both sides)
            std::array<float, maxSteps> left {}, right {};   // each step's level, 0 to 1
        };

        // Starting patterns for the page's START FROM menu (left, right; levels 0 to 1).
        struct Pattern { const char* name; std::array<float, 16> left, right; bool stereo; };
        static const std::array<Pattern, 8>& patterns();
        // The default pattern (the first of `patterns`, repeated to 32 steps).
        static float defaultStep (int side, int step);

        static float attackMs (float a) noexcept  { return 0.5f * std::pow (80.0f, juce::jlimit (0.0f, 1.0f, a)); }
        static float releaseMs (float r) noexcept { return std::pow (300.0f, juce::jlimit (0.0f, 1.0f, r)); }
        static double beatsPerStep (int division) noexcept
        {
            return divisionBeats[(size_t) juce::jlimit (0, (int) divisionBeats.size() - 1, division)];
        }

        // Where `steps` (the song position counted in steps) falls: the pattern's step and how far into it (0 to 1).
        struct Place { int step = 0; double within = 0.0; };
        static Place placeAt (double steps, int length, float swing) noexcept;
        // The gate's target level (0 to 1) for one side at that place.
        static float targetAt (const Settings&, int side, Place) noexcept;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        int getStep() const noexcept       { return stepShown.load (std::memory_order_relaxed); }   // -1 resting
        float getLevel (int side) const noexcept { return levelShown[(size_t) juce::jlimit (0, 1, side)].load (std::memory_order_relaxed); }
        float getMix() const noexcept      { return mixShown.load (std::memory_order_relaxed); }

    private:
        double sampleRate = 48000.0;
        bool resting = true;
        float mixNow = 0.0f, mixCoef = 0.0f;
        double ownBeats = 0.0;
        std::array<float, 2> envelope { 1.0f, 1.0f };

        std::atomic<int> stepShown { -1 };
        std::array<std::atomic<float>, 2> levelShown { 1.0f, 1.0f };
        std::atomic<float> mixShown { 0.0f };
    };
}
