#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // FREEZE TAG (0.89.0, the owner's name; PLAYGROUND's signature, the owner's choice of two offered (a freeze game in
    // time with the song, over a clockwork toy running down), chosen after hearing both sketched on one lead; our own
    // design). The playground game: everybody moves, then freezes. One knob, AMOUNT, brings it in:
    //  - in time with the song each CYCLE (`cycleNames` 1 BAR, 2 BARS, 4 BARS: never reorder) plays, then freezes for its
    //    last HOLD (`holdNames` 1 BEAT, 2 BEATS, 4 BEATS: never reorder; never more than the cycle less a beat);
    //  - frozen, the live sound is cut (to 1 - AMOUNT) and in its place hangs a ghost of the moment it stopped: the last
    //    SLICE (40-250 ms) caught when the freeze starts and played as three overlapping grains, each from its own
    //    random place in the slice (raised-cosine windows, half the slice long), at LEVEL x AMOUNT;
    //  - TREMBLE: the ghost can't keep still: its pitch wavers (a 5.5 Hz sine, up to 25 cents) and its level shakes;
    //  - FROST: the ghost darkens (a one-pole from 16 kHz down to 1.5 kHz);
    //  - CAUGHT: that chance (x AMOUNT) a freeze gets caught moving: the ghost sinks an octave over the freeze and
    //    crumbles (held samples, 6 bits);
    //  - edges: the cut 3 ms in, the return 4 ms out, so nothing clicks.
    // In time with `ppq` while the song plays (4/4); its own count while stopped. AMOUNT smoothed 30 ms; under 0.001 it
    // counts as 0, and at 0, the gate closed, the sound passes untouched bit for bit (`isResting`; the history keeps
    // recording, so a freeze straight after waking still has a moment to hold). Random by xorshift (`seedRandom`).
    class FreezeTag
    {
    public:
        static inline const juce::StringArray cycleNames { "1 BAR", "2 BARS", "4 BARS" };     // never reorder
        static constexpr std::array<double, 3> cycleBeats { 4.0, 8.0, 16.0 };
        static inline const juce::StringArray holdNames { "1 BEAT", "2 BEATS", "4 BEATS" };   // never reorder
        static constexpr std::array<double, 3> holdBeatsOf { 1.0, 2.0, 4.0 };

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;    // 0 to 1 (the macro's)
            int cycle = 1;          // cycleNames: 2 BARS
            int hold = 1;           // holdNames: 2 BEATS
            float slice = 0.4f;     // 0 to 1: 40 to 250 ms
            float tremble = 0.3f;   // 0 to 1
            float frost = 0.3f;     // 0 to 1
            float caught = 0.2f;    // 0 to 1: the chance a freeze is caught
            float level = 0.8f;     // 0 to 1: the ghost's level
        };

        static float sliceSeconds (float s) noexcept { return 0.04f + 0.21f * juce::jlimit (0.0f, 1.0f, s); }
        static float frostHz (float f) noexcept { return 16000.0f * std::pow (1500.0f / 16000.0f, juce::jlimit (0.0f, 1.0f, f)); }
        // The hold in beats, never more than the cycle less a beat.
        static double holdBeats (int cycle, int hold) noexcept
        {
            const auto c = cycleBeats[(size_t) juce::jlimit (0, (int) cycleBeats.size() - 1, cycle)];
            return juce::jmin (holdBeatsOf[(size_t) juce::jlimit (0, (int) holdBeatsOf.size() - 1, hold)], c - 1.0);
        }
        // Frozen `beats` into the song?
        static bool frozenAt (double beats, int cycle, int hold) noexcept
        {
            const auto c = cycleBeats[(size_t) juce::jlimit (0, (int) cycleBeats.size() - 1, cycle)];
            const auto into = beats - c * std::floor (beats / c);
            return into >= c - holdBeats (cycle, hold);
        }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (uint32_t seed) noexcept { random = seed != 0 ? seed : 1u; }
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page, the unit and the tests.
        float getAmount() const noexcept   { return amountShown.load (std::memory_order_relaxed); }
        float getGate() const noexcept     { return gateShown.load (std::memory_order_relaxed); }       // 0 playing, 1 frozen
        float getPosition() const noexcept { return positionShown.load (std::memory_order_relaxed); }   // 0-1 through the cycle
        bool wasCaught() const noexcept    { return caughtShown.load (std::memory_order_relaxed); }
        int getFreezes() const noexcept    { return freezesShown.load (std::memory_order_relaxed); }

    private:
        static constexpr int numGrains = 3;
        struct Grain
        {
            double position = 0.0;   // in the slice, samples
            double start = 0.0;
            int age = 0;             // samples into the grain
        };
        struct Side
        {
            std::vector<float> history;   // the last second
            std::vector<float> slice;     // the moment caught
            float low = 0.0f;             // FROST's state
            float held = 0.0f;            // CAUGHT's held sample
        };

        uint32_t nextRandom() noexcept;
        void catchMoment (const Settings&) noexcept;

        double sampleRate = 48000.0;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f;
        double ownBeats = 0.0;
        int historyMask = 0, writeIndex = 0;
        std::array<Side, 2> sides {};
        std::array<Grain, numGrains> grains {};   // shared by both sides, so the image holds
        int sliceLength = 0, grainLength = 0;
        bool frozen = false, caught = false;
        float gate = 0.0f, gateIn = 0.0f, gateOut = 0.0f;
        double tremblePhase = 0.0, sink = 1.0, sinkPerSample = 1.0;
        int crushCount = 0;
        int settledSamples = 0;
        uint32_t random = 0x51ed2701u;

        std::atomic<float> amountShown { 0.0f }, gateShown { 0.0f }, positionShown { 0.0f };
        std::atomic<bool> caughtShown { false };
        std::atomic<int> freezesShown { 0 };
    };
}
