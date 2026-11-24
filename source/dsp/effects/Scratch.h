#pragma once

#include "EffectUtils.h"

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // SCRATCH (HARDWIRED's signature, the owner's choice after hearing the sketch: "especially that distorted
    // aggressive style of scratching"): a turntablist cutting up what you just played, in time with the song. Our own
    // design: no turntable, mixer or record is modelled from a product.
    //
    // It always records what it hears (`historySeconds` a side). When a scratch starts it parks the needle just before
    // the newest note's hit that's far enough back (the synth's key presses, `addNote`) and a hand moves that record:
    // the position follows each move's stroke (eased as a hand moves, quick through the middle), so the pitch and the
    // direction come from how fast the record moves, and the sound fades as the hand slows to a stop (a cartridge gives
    // nothing with the record still). The record is read from one of four copies, each low-passed for a faster stroke,
    // so a fast stroke doesn't fold its top back down. The crossfader cuts in half a millisecond.
    //
    // The moves (`moveNames`, never reorder): BABY (out and back), CHIRP (the fader closing as it goes out, opening as
    // it comes back), FLARE (one click mid-stroke each way), FLARE 2 (two clicks), CRAB (four fast taps), TEAR (the
    // stroke out torn in two), TRANSFORM (a slow stroke chopped every 32nd), SCRIBBLE (a tense buzz), JUGGLE (the hit
    // dropped straight from the needle) and REWIND (spun back hard, slowing into the bar line). Timing a few ms late at
    // random, each stroke's length varied by 15%: a hand, not a machine.
    //
    // AMOUNT (MACRO 3): how much of the song the DJ takes. Up to a third, a fill on the last beat of every other bar;
    // to two thirds, the last two beats of every bar; above, whole bars (every other bar first). Which moves fill a
    // stretch is a fixed hash of its bar, so the same place in the song is the same routine. STROKE: how far the hand
    // pulls the record. DIRT: the mixer pushed into the red (at 0, the sketch's drive: a low cut, a hard leaning drive
    // and a bite at 1.8 kHz; up to four times harder, a second fuzz stage and more bite). MIX: 1 replaces what you
    // played while it's scratched (the sketch), 0 puts the scratches over it.
    //
    // While nothing is scratched the sound passes untouched bit for bit (`isResting`); it records all the while it's
    // on, so the first scratch after waking has something to cut. Random by xorshift (`seedRandom`).
    class Scratch
    {
    public:
        enum class Move { baby, chirp, flare, flare2, crab, tear, transform, scribble, juggle, rewind };
        static inline const juce::StringArray moveNames { "BABY", "CHIRP", "FLARE", "FLARE 2", "CRAB", "TEAR",
                                                          "TRANSFORM", "SCRIBBLE", "JUGGLE", "REWIND" };   // never reorder

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;     // 0 to 1 (MACRO 3)
            float stroke = 0.15f;    // 0.06 to 0.3 s of record each stroke
            float dirt = 0.0f;       // 0 (the sketch's drive) to 1
            float mix = 1.0f;        // 0 over the sound to 1 replacing it
        };

        static constexpr double historySeconds = 6.0;
        static constexpr double maxAnchorAge = 3.0;    // the newest hit at most this far back, else the last beat
        static constexpr int maxPlanned = 16;

        // How much of bar `bar` (from its start, in sixteenths) is scratched at AMOUNT `amount`: 0 none.
        static int sixteenthsFor (float amount, std::int64_t bar) noexcept;
        // A bar's routine: its moves (kind, start and length in sixteenths), filling the bar's last `sixteenths`.
        struct Planned { Move kind = Move::baby; int start = 0, length = 2; };
        static int planBar (std::int64_t bar, float amount, std::array<Planned, maxPlanned>& out) noexcept;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { noise = seed == 0 ? 0x9e3779b9u : seed; }

        // A key pressed `offset` samples into the coming block (called before `process`).
        void addNote (int offset) noexcept;

        // The block at `bpm`, the song at `ppq` beats at its start (-1 while it's stopped: its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the unit.
        int getMoveNow() const noexcept { return moveNow.load (std::memory_order_relaxed); }      // -1 none
        float getRecordSeconds() const noexcept { return recordNow.load (std::memory_order_relaxed); }   // the needle, s from the hit
        float getFader() const noexcept { return faderNow.load (std::memory_order_relaxed); }
        int getMoveCount() const noexcept { return moveCount.load (std::memory_order_relaxed); }
        float getBarPosition() const noexcept { return barNow.load (std::memory_order_relaxed); }  // 0-16 sixteenths
        int getPlannedCount() const noexcept { return shownPlanned.load (std::memory_order_relaxed); }
        int getPlannedMove (int k) const noexcept { return shownKind[(size_t) juce::jlimit (0, maxPlanned - 1, k)].load (std::memory_order_relaxed); }
        int getPlannedStart (int k) const noexcept { return shownStart[(size_t) juce::jlimit (0, maxPlanned - 1, k)].load (std::memory_order_relaxed); }
        int getPlannedLength (int k) const noexcept { return shownLength[(size_t) juce::jlimit (0, maxPlanned - 1, k)].load (std::memory_order_relaxed); }

        // The hand: a move's needle (seconds from the hit) and crossfader at sample i of n (for tests and the page).
        struct Hand { double position = 0.0; float fader = 1.0f; };
        static Hand handAt (Move, int i, int n, double stroke, double sampleRate, double sixteenthSamples) noexcept;
        static double reachOf (Move, double stroke, double seconds) noexcept;   // how far past the hit it reads

    private:
        static constexpr int levels = 4;
        struct Biquad
        {
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;
            float run (float x) noexcept
            {
                const auto y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
            void clear() noexcept { z1 = z2 = 0.0f; }
        };
        static Biquad lowPass (double hz, double q, double sampleRate) noexcept;
        static Biquad highPass (double hz, double q, double sampleRate) noexcept;
        static Biquad peak (double hz, double q, double db, double sampleRate) noexcept;

        float random01() noexcept;
        float gaussian() noexcept;
        void startMove (const Planned&, double sixteenthSamples) noexcept;
        void startSpan (const Settings&, double sixteenthSamples) noexcept;
        float readLevel (int level, int channel, double position) const noexcept;
        void startChain() noexcept;

        double sampleRate = 48000.0;
        int size = 1;
        std::array<std::array<std::vector<float>, 2>, levels> history;   // the record, at four darknesses
        std::array<std::array<std::array<Biquad, 2>, 2>, levels> recordFilters;   // per level, side, stage
        int write = 0;
        std::int64_t clock = 0;   // samples recorded

        std::array<std::int64_t, 16> presses {};
        int pressWrite = 0, pressCount = 0;

        // The song.
        double ownBeat = 0.0, lastBeat = -1.0e9;
        std::int64_t planBarIndex = -1;
        std::array<Planned, maxPlanned> planned {};
        int numPlanned = 0, nextPlanned = 0;
        bool spanStarted = false;

        // The move playing.
        bool active = false;
        Move kind = Move::baby;
        int moveSample = 0, moveLength = 0, delay = 0;
        double moveStroke = 0.15, anchor = 0.0;   // the hit's place in the record (samples)
        double sixteenth = 6000.0;
        double lastPosition = 0.0;
        bool hasLast = false;

        // The mixer.
        float fader = 1.0f, gate = 0.0f;
        std::array<Biquad, 2> lowCut, bite, top, hissLow, hissHigh;
        std::array<float, 2> dcIn {}, dcOut {}, readLevelNow {};
        float inputPower = 0.0f, wetPower = 0.1f, wetGain = 1.0f;
        bool resting = true;
        std::uint32_t noise = 0x9e3779b9u;

        std::atomic<int> moveNow { -1 }, moveCount { 0 }, shownPlanned { 0 };
        std::atomic<float> recordNow { 0.0f }, faderNow { 1.0f }, barNow { 0.0f };
        std::array<std::atomic<int>, maxPlanned> shownKind {}, shownStart {}, shownLength {};
    };
}
