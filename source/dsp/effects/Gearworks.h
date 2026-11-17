#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <numeric>

namespace effects
{
    // GEARWORKS (0.91.0, the owner's choice and name; the Silo bank's signature, chosen over DEEP SHAFT after hearing both
    // sketched on one industrial lead; our own design, no machine or product modelled). The sound caught in the
    // machinery. One knob, AMOUNT, brings it in:
    //  - THE GEARS: two gears turning in time with the song, one tooth every `a` steps, the other every `b` (RATIO,
    //    `ratioNames` 3 : 4, 2 : 3, 3 : 5, 5 : 7: never reorder), the steps DIVISION long (`divisionNames` 1/16, 1/8,
    //    1/16 T: never reorder). Wherever a tooth comes round the sound is cut (BITE: how deep; 1.5 ms in, 4 ms out), so
    //    the two together grind out an interlocking pattern that repeats every a x b steps;
    //  - the cut's loss made up from the pattern itself (its mean power over a whole turn, at most +6 dB), not a follower;
    //  - METAL: the sound ring-modulated by a sine 2.76 x the lowest note sounding (`setNotes`: a struck metal bar's
    //    first overtone), so it rings as metal whatever's played; its loss made up too;
    //  - GRIND: the gears' teeth on the signal: held samples and fewer bits;
    //  - STEAM: a hiss of band-passed noise on every cut, following your level (silence stays silent);
    //  - CLANK: each tooth strikes metal: three decaying partials (1, 2.76, 5.40) at 1.2 kHz for the first gear, 760 Hz
    //    for the second (both when they meet), panned a little apart, as loud as your playing allows.
    // In time with `ppq` while the song plays (4/4); its own count while stopped. AMOUNT smoothed 30 ms; under 0.001 it
    // counts as 0, and at 0, the clanks and steam died, the sound passes untouched bit for bit (`isResting`).
    // Random by xorshift (`seedRandom`).
    class Gearworks
    {
    public:
        static inline const juce::StringArray ratioNames { "3 : 4", "2 : 3", "3 : 5", "5 : 7" };   // never reorder
        static constexpr std::array<std::array<int, 2>, 4> ratios { { { 3, 4 }, { 2, 3 }, { 3, 5 }, { 5, 7 } } };
        static inline const juce::StringArray divisionNames { "1/16", "1/8", "1/16 T" };           // never reorder
        static constexpr std::array<double, 3> divisionBeats { 0.25, 0.5, 1.0 / 6.0 };
        static constexpr int maxNotes = 16;
        static constexpr float metalRatio = 2.76f;

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;   // 0 to 1 (the macro's)
            int ratio = 0;         // ratioNames: 3 : 4
            int division = 0;      // divisionNames: 1/16
            float bite = 0.8f;     // 0 to 1: how deep a tooth cuts
            float metal = 0.4f;    // 0 to 1
            float grind = 0.3f;    // 0 to 1
            float steam = 0.3f;    // 0 to 1
            float clank = 0.4f;    // 0 to 1
        };

        // Which gears have a tooth on `step`: bit 0 the first, bit 1 the second.
        static int teethAt (long long step, int ratio) noexcept
        {
            const auto& r = ratios[(size_t) juce::jlimit (0, (int) ratios.size() - 1, ratio)];
            const auto s = step < 0 ? 0 : step;
            return ((s % r[0]) == r[0] - 1 ? 1 : 0) | ((s % r[1]) == r[1] - 1 ? 2 : 0);
        }
        // Steps in a whole turn (until the pattern repeats).
        static int turnSteps (int ratio) noexcept
        {
            const auto& r = ratios[(size_t) juce::jlimit (0, (int) ratios.size() - 1, ratio)];
            return std::lcm (r[0], r[1]);
        }
        // The share of a turn's steps that are cut.
        static float cutShare (int ratio) noexcept
        {
            const auto n = turnSteps (ratio);
            int cut = 0;
            for (int k = 0; k < n; ++k)
                cut += teethAt (k, ratio) != 0 ? 1 : 0;
            return (float) cut / (float) n;
        }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (uint32_t seed) noexcept { random = seed != 0 ? seed : 1u; }
        void setNotes (const float* notes, int count) noexcept;
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page, the unit and the tests.
        float getAmount() const noexcept { return amountShown.load (std::memory_order_relaxed); }
        int getStep() const noexcept     { return stepShown.load (std::memory_order_relaxed); }      // in the turn
        int getTeeth() const noexcept    { return teethShown.load (std::memory_order_relaxed); }     // bits, the step now
        float getTurn() const noexcept   { return turnShown.load (std::memory_order_relaxed); }      // 0-1 through the turn
        float getClank() const noexcept  { return clankShown.load (std::memory_order_relaxed); }
        int getClanks() const noexcept   { return clanksShown.load (std::memory_order_relaxed); }

    private:
        struct Partial
        {
            double phase = 0.0, step = 0.0;
            float level = 0.0f, fall = 1.0f;
        };
        struct Side
        {
            float held = 0.0f;     // GRIND's held sample
            float steamLow = 0.0f, steamBand = 0.0f;
            float follow = 0.0f;   // this side's level (10 ms up, 0.4 s down)
        };

        uint32_t nextRandom() noexcept;
        float noise() noexcept { return (float) (int32_t) nextRandom() * (1.0f / 2147483648.0f); }
        void strike (int teeth, float strength) noexcept;

        double sampleRate = 48000.0;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f;
        double ownBeats = 0.0;
        long long lastStep = -1;
        float cut = 0.0f, cutIn = 0.0f, cutOut = 0.0f;
        int teethNow = 0;
        float lowestNote = 48.0f, metalNote = 48.0f, noteCoef = 0.0f;
        double metalPhase = 0.0;
        int grindCount = 0;
        float steam = 0.0f, steamFall = 1.0f;
        float follower = 0.0f, followUp = 0.0f, followDown = 0.0f;
        std::array<Side, 2> sides {};
        std::array<Partial, 6> partials {};   // the first gear's three, then the second's
        int settledSamples = 0;
        uint32_t random = 0x6ea2c0d3u;

        std::atomic<float> amountShown { 0.0f }, turnShown { 0.0f }, clankShown { 0.0f };
        std::atomic<int> stepShown { 0 }, teethShown { 0 }, clanksShown { 0 };
    };
}
