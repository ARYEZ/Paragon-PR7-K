#pragma once

#include "EffectUtils.h"

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // FLIP (HARDWIRED's second signature, the owner's choice after hearing the sketch: "both scratch and flip are
    // amazing, can we keep both?"): a 90s sampler catching a beat of what you just played and flipping it, in time with
    // the song. Our own design: no sampler is modelled from a product.
    //
    // It always records what it hears (`historySeconds` a side). At each bar it flips, two beats of the bar just played
    // are caught (its first beat, A, and its third, B) and played back in cells along the bar, the way a producer flips
    // a loop on the pads. Each bar flips one way (`flipNames`, never reorder): LOOP (A looped a beat at a time), UP (A
    // retriggered every eighth, up PITCH and PITCH + 2 semitones in turn: the pitched-up sampler), DOWN (a beat at a
    // time, down PITCH, every other one B backwards), REVERSE (B backwards a beat at a time), CHOP (A cut into its four
    // sixteenths and played back in a new order) and STUTTER (A's start repeated every 32nd, the last few an octave up).
    // Pitch is a sampler's: faster is higher and shorter, as the sampler played it. Which way a bar flips is a fixed
    // hash of the bar, so the same place in the song flips the same way.
    //
    // AMOUNT (MACRO 3): how much of the song is flipped, as SCRATCH's: up to a third, the last beat of every other bar
    // (a stutter into the bar line); to two thirds, the last two beats of every bar; above, whole bars (every other bar
    // first). CRUNCH: the sampler's colour (at 0.5, 12 bits at 26.04 kHz as in the sketch; 0 clean, 1 eight bits at
    // 13 kHz). PUMP: a compressor ducking each beat and letting it back over an eighth. MIX: 1 replaces what you played
    // where it's flipped, 0 puts the flip over it.
    //
    // While nothing is flipped the sound passes untouched bit for bit (`isResting`); it records all the while it's on.
    class Flip
    {
    public:
        enum class Kind { loop, up, down, reverse, chop, stutter };
        static inline const juce::StringArray flipNames { "LOOP", "UP", "DOWN", "REVERSE", "CHOP", "STUTTER" };   // never reorder

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;   // 0 to 1 (MACRO 3)
            float pitch = 5.0f;    // 1 to 12 semitones: UP's and DOWN's step
            float crunch = 0.5f;   // 0 clean, 0.5 the sketch's 12 bits at 26.04 kHz, 1 eight bits at 13 kHz
            float pump = 0.45f;    // 0 to 1
            float mix = 1.0f;      // 0 over the sound to 1 replacing it
        };

        static constexpr double historySeconds = 12.0;
        static constexpr int maxCells = 40;

        static int sixteenthsFor (float amount, std::int64_t bar) noexcept;   // as SCRATCH's
        struct Cell
        {
            float start = 0.0f, length = 4.0f;   // sixteenths in the bar
            bool fromB = false, reversed = false;
            float semitones = 0.0f;
            float offset = 0.0f;                 // sixteenths into the grab it starts from
        };
        struct Plan { Kind kind = Kind::loop; std::array<Cell, maxCells> cells {}; int count = 0; bool stutterEnd = false; };
        static Plan planBar (std::int64_t bar, float amount, float pitch) noexcept;
        static Kind kindOf (std::int64_t bar) noexcept;   // how a bar flips (its whole stretch, the stutter fill aside)
        static double rateFor (float crunch) noexcept { return 26040.0 * std::exp2 ((0.5 - (double) juce::jlimit (0.0f, 1.0f, crunch)) * 2.0); }
        static float bitsFor (float crunch) noexcept { return 16.0f - 8.0f * juce::jlimit (0.0f, 1.0f, crunch); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the unit.
        int getKindNow() const noexcept { return kindNow.load (std::memory_order_relaxed); }      // -1 none
        int getCellNow() const noexcept { return cellNow.load (std::memory_order_relaxed); }      // -1 none
        int getFlips() const noexcept { return flips.load (std::memory_order_relaxed); }          // cells played
        float getBarPosition() const noexcept { return barNow.load (std::memory_order_relaxed); } // 0-16 sixteenths
        int getCellCount() const noexcept { return shownCount.load (std::memory_order_relaxed); }
        float getCellStart (int k) const noexcept { return shownStart[(size_t) juce::jlimit (0, maxCells - 1, k)].load (std::memory_order_relaxed); }
        float getCellLength (int k) const noexcept { return shownLength[(size_t) juce::jlimit (0, maxCells - 1, k)].load (std::memory_order_relaxed); }
        float getCellSemitones (int k) const noexcept { return shownSemis[(size_t) juce::jlimit (0, maxCells - 1, k)].load (std::memory_order_relaxed); }
        bool isCellReversed (int k) const noexcept { return shownReversed[(size_t) juce::jlimit (0, maxCells - 1, k)].load (std::memory_order_relaxed); }

    private:
        float readAt (int channel, double position) const noexcept;

        double sampleRate = 48000.0;
        int size = 1;
        std::array<std::vector<float>, 2> history;
        int write = 0;
        std::int64_t clock = 0;

        double ownBeat = 0.0, lastBeat = -1.0e9;
        std::int64_t planBarIndex = -1;
        Plan plan;
        int nextCell = 0;
        double grabA = 0.0, grabB = 0.0;   // where the bar's grabs start in the record (samples)
        bool grabbed = false;

        // The cell playing.
        bool active = false;
        int current = -1;
        double cellAge = 0.0, cellLength = 0.0, speed = 1.0, from = 0.0, grabLength = 0.0;
        bool reversed = false;

        float gate = 0.0f;
        double holdPhase = 0.0;
        std::array<float, 2> held {}, lowIn {}, lowOut {};
        float inputPower = 0.0f, wetPower = 0.05f, wetGain = 1.0f;
        bool resting = true;

        std::atomic<int> kindNow { -1 }, cellNow { -1 }, flips { 0 }, shownCount { 0 };
        std::atomic<float> barNow { 0.0f };
        std::array<std::atomic<float>, maxCells> shownStart {}, shownLength {}, shownSemis {};
        std::array<std::atomic<bool>, maxCells> shownReversed {};
    };
}
