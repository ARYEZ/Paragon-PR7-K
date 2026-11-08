#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // JACK-IN (0.85.0, the owner's name; MACHINE SUPREMACY's signature, option 1 of two offered (a break-in that takes
    // the phrase over, over the sound turned to metal), chosen after hearing both sketched from existing effects on one
    // lead; our own design). One knob, AMOUNT, lets something break into the sound:
    //  - it keeps hearing (12 s a side) and, in time with the song, cuts the sound into slices (SLICE: 1/8, 1/16, 1/32,
    //    1/16 T; never reorder `sliceNames`);
    //  - at each slice it may take the slice over (the chance rises with AMOUNT: at full nearly every one): the slice is
    //    replaced by one from the last REACH (1 beat, 2 beats, 1 bar, 2 bars; never reorder `reachNames`), played as it
    //    was (REPLAY), backwards (REVERSE), its start repeated two or four times (STUTTER), an octave up (UP) or down
    //    (DOWN). CHAOS chooses: low, mostly replays and stutters from whole beats back, so the rhythm holds; high, more
    //    reverses and octaves from anywhere in REACH;
    //  - CRUSH: how often a taken slice is also crushed (held samples and fewer levels) and how hard;
    //  - DROPS: now and then a beat ends in a tape stop (the sound slowing to nothing over the rest of the beat).
    // Each change crossfades over 2.5 ms, and a stutter's repeats are faded at their edges, so nothing clicks. Both sides
    // are taken the same way from their own recordings (the image keeps). In time with `ppq` while the song plays, its own
    // count while stopped. Under 0.001 AMOUNT counts as 0, and at 0, once the last taken slice has faded out, the sound
    // passes untouched bit for bit (`isResting`); it keeps recording, so it wakes with the past to hand. Random by xorshift
    // (`seedRandom`).
    class JackIn
    {
    public:
        static inline const juce::StringArray sliceNames { "1/8", "1/16", "1/32", "1/16 T" };   // never reorder
        static constexpr std::array<double, 4> sliceBeats { 0.5, 0.25, 0.125, 1.0 / 6.0 };
        static inline const juce::StringArray reachNames { "1 BEAT", "2 BEATS", "1 BAR", "2 BARS" };   // never reorder
        static constexpr std::array<double, 4> reachBeats { 1.0, 2.0, 4.0, 8.0 };

        // What a slice became (for the page and the tests). `dry`: untouched.
        enum class Move { dry, replay, reverse, stutter, up, down, stop };
        static inline const juce::StringArray moveNames { "", "REPLAY", "REVERSE", "STUTTER", "UP", "DOWN", "STOP" };

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;    // 0 to 1: how much is taken (the macro's)
            int slice = 1;          // sliceNames: 1/16
            int reach = 2;          // reachNames: 1 bar
            float chaos = 0.4f;     // 0 to 1: replays and stutters to reverses and octaves
            float crush = 0.3f;     // 0 to 1: how often and how hard taken slices are crushed
            float drops = 0.3f;     // 0 to 1: how often a beat ends in a tape stop
        };

        static constexpr double historySeconds = 12.0;
        static constexpr int maxHistoryKept = 64;   // slices remembered for the page

        // The chance a slice is taken at AMOUNT `a`.
        static float takeChance (float a) noexcept { return a <= 0.0f ? 0.0f : 0.95f * std::pow (juce::jlimit (0.0f, 1.0f, a), 0.7f); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { random = seed != 0 ? seed : 0x2545f491u; }
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getAmount() const noexcept { return amountShown.load (std::memory_order_relaxed); }
        // The slices so far (counted since it woke) and what slice n became (the last `maxHistoryKept`).
        int getSliceCount() const noexcept { return slicesShown.load (std::memory_order_relaxed); }
        Move getMove (int slice) const noexcept
        {
            return slice < 0 ? Move::dry : (Move) moves[(size_t) (slice % maxHistoryKept)].load (std::memory_order_relaxed);
        }
        int getSlicesPerBar() const noexcept { return perBarShown.load (std::memory_order_relaxed); }
        // How many of each move so far (for the tests).
        int getMoveCount (Move m) const noexcept { return counts[(size_t) m].load (std::memory_order_relaxed); }

    private:
        struct Player
        {
            Move move = Move::dry;
            double start = 0.0;       // the source: an absolute sample index into the recording
            double t = 0.0;           // samples since the player started
            double length = 1.0;      // the slice, in samples
            double loop = 1.0;        // STUTTER: the repeated piece
            double stopLength = 1.0;  // STOP: samples to slow to nothing
            int hold = 1;             // CRUSH: samples held (1: none)
            float levels = 0.0f;      // CRUSH: steps each side (0: none)
            int held = 0;
            std::array<float, 2> heldValue {};
            float gain = 0.0f;        // the crossfade
            bool active = false;
        };

        std::uint32_t next() noexcept
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return random;
        }
        float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
        float readAt (int channel, double position) const noexcept;
        // Starts a new slice at absolute sample `now`, `sliceIndex` (the song in slices).
        void startSlice (std::int64_t now, std::int64_t sliceIndex, double sliceLength, double slicesPerBeat, const Settings&);
        // The player's value for each side, advancing it a sample.
        void playerValue (Player&, std::int64_t now, float inL, float inR, float& outL, float& outR) noexcept;

        double sampleRate = 48000.0;
        std::uint32_t random = 0x2545f491u;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f;
        double ownBeats = 0.0;
        std::int64_t lastSlice = -1;
        int holdSlices = 0;          // STOP: slices it keeps
        int awakeSlices = 0;

        std::array<std::vector<float>, 2> history;
        std::int64_t written = 0;    // samples recorded so far
        double fadeStep = 0.0;

        Player current, previous;

        std::atomic<float> amountShown { 0.0f };
        std::atomic<int> slicesShown { 0 }, perBarShown { 16 };
        std::array<std::atomic<int>, maxHistoryKept> moves {};
        std::array<std::atomic<int>, 7> counts {};
    };
}
