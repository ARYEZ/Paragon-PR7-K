#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <vector>

namespace effects
{
    // SORT (0.93.0, the owner's choice and name; the Severance rebuild's signature, option B of two sketched (SPLIT, a
    // second self peeling off the sound), chosen after hearing both on one office piano lead; our own design). The
    // office's tidy, pointless work done to what you play. In time with the song it cuts the sound into SLICEs and, a
    // window (LENGTH) at a time, plays back the window before re-filed in ORDER:
    //  - UP, lowest note first; DOWN, highest first; PEAK, rising to the highest in the middle and falling back;
    //    LOUDEST, loudest first; SHUFFLE, a new random order each window (never reorder `orderNames`);
    //  - each slice is filed by the note the synth was playing a third of the way into it (`setNotes`, the newest held
    //    note or chord, from `Engine::getNewestNotes`; reading pitch from the audio slipped octaves on a piano), its
    //    loudness measured; a silent slice (under -80 dB) goes to the end, or stays where it was with GAPS IN PLACE;
    //  - EDGE: the crossfade between pieces, 1 ms (crisp) to 30 ms (smeared); a piece that follows on from the one before
    //    in the recording isn't crossfaded at all, so a phrase already in order plays back whole;
    //  - STRIKE: each piece given a hit at its start and a fall after it, like a key struck (the pieces are cut from
    //    ringing notes and most start without an attack);
    //  - AMOUNT: the blend from the live sound (0) to the re-filed one (1), smoothed 30 ms.
    // It can only re-file what it has heard, so what comes back is always a window behind. In time with `ppq` while the
    // song plays, its own count while stopped. Under 0.001 AMOUNT counts as 0, and at 0, once its crossfade has finished,
    // the sound passes untouched bit for bit (`isResting`); it keeps recording and filing, so it wakes with the last
    // window to hand. Both sides are re-filed the same way from their own recordings. Random by xorshift (`seedRandom`).
    class Sort
    {
    public:
        static inline const juce::StringArray sliceNames { "1/4", "1/8", "1/16", "1/8 T" };          // never reorder
        static constexpr std::array<double, 4> sliceBeats { 1.0, 0.5, 0.25, 1.0 / 3.0 };
        static inline const juce::StringArray lengthNames { "1/2 BAR", "1 BAR", "2 BARS" };          // never reorder
        static constexpr std::array<double, 3> lengthBeats { 2.0, 4.0, 8.0 };
        static inline const juce::StringArray orderNames { "UP", "DOWN", "PEAK", "LOUDEST", "SHUFFLE" };   // never reorder
        static inline const juce::StringArray gapNames { "AT END", "IN PLACE" };                      // never reorder
        enum Order { up = 0, down, peak, loudest, shuffle };

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;   // 0 to 1: live to re-filed (the macro's)
            int slice = 1;         // sliceNames: 1/8
            int length = 1;        // lengthNames: 1 bar
            int order = up;        // orderNames
            int gaps = 0;          // gapNames: at the end
            float edge = 0.3f;     // 0 to 1: 1 ms to 30 ms
            float strike = 0.3f;   // 0 to 1
        };

        static constexpr int maxSlices = 32;          // a window's slices at most (2 bars of 1/16)
        static constexpr double historySeconds = 20.0;
        static constexpr int maxNotes = 16;
        static constexpr float silentRms = 1.0e-4f;   // -80 dB
        static double edgeMs (float edge) noexcept { return std::pow (30.0, (double) juce::jlimit (0.0f, 1.0f, edge)); }

        // The order a window's slices are played back in, from their notes and loudness (`note` < 0: silent).
        // `result[j]` is the slice played in place j. Shared with the tests and the page.
        static void orderFor (const float* note, const float* loudness, int count, int order, bool gapsInPlace,
                              std::uint32_t& random, int* result) noexcept;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { random = seed != 0 ? seed : 0x7a3c19e5u; }
        // The newest notes held now (none: keeps the last ones, so a ringing note's tail is still filed as that note).
        void setNotes (const float* notes, int count) noexcept;
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests: the window being played back (its slices' notes, -1 silent, in recorded order;
        // the order they're played in; the place playing now, -1 none).
        float getAmount() const noexcept { return amountShown.load (std::memory_order_relaxed); }
        int getSliceCount() const noexcept { return countShown.load (std::memory_order_relaxed); }
        float getNote (int k) const noexcept { return notesShown[(size_t) juce::jlimit (0, maxSlices - 1, k)].load (std::memory_order_relaxed); }
        int getOrder (int k) const noexcept { return orderShown[(size_t) juce::jlimit (0, maxSlices - 1, k)].load (std::memory_order_relaxed); }
        int getPlace() const noexcept { return placeShown.load (std::memory_order_relaxed); }
        int getWindowsFiled() const noexcept { return windowsShown.load (std::memory_order_relaxed); }

    private:
        struct Slice
        {
            std::int64_t start = 0;   // absolute sample index into the recording
            float note = -1.0f;       // the note it's filed by
            double energy = 0.0;      // sum of squares
            int samples = 0;
            bool noteTaken = false;
        };
        struct Window
        {
            std::array<Slice, maxSlices> slices {};
            int count = 0;
            std::int64_t index = -10;   // the song's window number
            bool complete = false;      // heard from its first slice, every slice in turn
            int next = 0;               // the slice expected next
        };
        struct Player
        {
            std::int64_t start = 0;   // the source, an absolute sample index
            double t = 0.0;
            float gain = 0.0f;
            float attack = 1.0f, fall = 1.0f;   // STRIKE's hit and fall, each decaying from 1
            bool dry = true;          // plays the live sound
            bool active = false;
        };

        float readAt (int channel, std::int64_t position) const noexcept;
        void playerValue (Player&, std::int64_t now, float inL, float inR, float strike, float& l, float& r) noexcept;
        void fileWindow (const Window&, int order, bool gapsInPlace) noexcept;
        void beginSlice (std::int64_t now, std::int64_t window, int place, int perWindow, const Settings&) noexcept;

        double sampleRate = 48000.0;
        std::uint32_t random = 0x7a3c19e5u;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f;
        double ownBeats = 0.0;
        std::int64_t lastSlice = std::numeric_limits<std::int64_t>::min();
        double sliceLengthNow = 1.0;
        int lastPerWindow = 0;
        double fadeStep = 1.0;
        float attackCoef = 0.0f, fallCoef = 0.0f;

        std::array<std::vector<float>, 2> history;
        std::int64_t written = 0;

        std::array<Window, 2> windows {};   // [0] being recorded, [1] the one before
        std::array<int, maxSlices> playOrder {};
        std::int64_t playWindow = -10;      // the window `playOrder` re-files (played back in the next)
        int playCount = 0;

        float label = 60.0f;
        bool haveLabel = false;

        Player current, previous;

        std::atomic<float> amountShown { 0.0f };
        std::atomic<int> countShown { 0 }, placeShown { -1 }, windowsShown { 0 };
        std::array<std::atomic<float>, maxSlices> notesShown {};
        std::array<std::atomic<int>, maxSlices> orderShown {};
    };
}
