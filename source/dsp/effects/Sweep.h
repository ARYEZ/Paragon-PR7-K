#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

namespace effects
{
    // SWEEP (0.87.0, LAST DAY's signature, the owner's choice of two offered (a song-locked resonant sweep over a
    // sample-and-hold "computer"), chosen after hearing both sketched on one lead; our own design). The 1970s analogue
    // sweep as a performance, locked to the song. One knob, AMOUNT, brings it in:
    //  - a resonant filter (FILTER: LOW-PASS, 24 dB: a resonant state-variable stage into a plain one; or BAND-PASS:
    //    the resonant stage's band; never reorder `filterNames`) whose cutoff moves on a log scale round CENTER
    //    (150 Hz-4 kHz) across RANGE (2 to 9 octaves), in time with the song;
    //  - SHAPE (never reorder `shapeNames`): UP rises over LENGTH and starts again, DOWN falls, UP-DOWN rises over LENGTH
    //    and falls over the next; LENGTH (`lengthNames` 1/2 BAR to 8 BARS, never reorder);
    //  - RESONANCE rises with the sweep, from 30% of itself at the bottom to all of it at the top (the near-scream at the
    //    peak); a light saturation going into the resonant stage, as the old ladder filters had;
    //  - PHASER: four allpasses a side riding the sweep (their notches at 0.7 x the cutoff, the right side's 15% higher,
    //    a little feedback), mixed in by PHASER.
    // AMOUNT moves the cutoff in from fully open (20 kHz) to the sweep's, the resonance and phaser with it, and blends
    // the filtered sound in over its first third. The level is kept (power in and out followed over 0.3 s, gain 0.5-2,
    // glided), since a sweep's dark half takes more away than its bright half gives back (4.5 dB on the sketch's lead).
    // In time with `ppq` while the song plays (4/4); its own count while stopped, starting at the bottom when it wakes.
    // AMOUNT smoothed 30 ms; under 0.001 it counts as 0, and at 0, once the gain is back to 1 for 0.3 s, the sound passes
    // untouched bit for bit (`isResting`).
    class Sweep
    {
    public:
        static inline const juce::StringArray lengthNames { "1/2 BAR", "1 BAR", "2 BARS", "4 BARS", "8 BARS" };   // never reorder
        static constexpr std::array<double, 5> lengthBeats { 2.0, 4.0, 8.0, 16.0, 32.0 };
        static inline const juce::StringArray shapeNames { "UP", "DOWN", "UP-DOWN" };                         // never reorder
        static inline const juce::StringArray filterNames { "LOW-PASS", "BAND-PASS" };                        // never reorder

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;      // 0 to 1 (the macro's)
            int length = 2;           // lengthNames: 2 BARS
            int shape = 2;            // shapeNames: UP-DOWN
            int filter = 0;           // filterNames: LOW-PASS
            float center = 0.5f;      // 0 to 1: 150 Hz to 4 kHz (log)
            float range = 0.5f;       // 0 to 1: 2 to 9 octaves
            float resonance = 0.6f;   // 0 to 1
            float phaser = 0.4f;      // 0 to 1
        };

        static float centerHz (float c) noexcept { return 150.0f * std::pow (4000.0f / 150.0f, juce::jlimit (0.0f, 1.0f, c)); }
        static float rangeOctaves (float r) noexcept { return 2.0f + 7.0f * juce::jlimit (0.0f, 1.0f, r); }
        // Where the sweep is (0 bottom to 1 top) at `beats` into the song.
        static double positionAt (double beats, int shape, int length) noexcept
        {
            const auto cycle = lengthBeats[(size_t) juce::jlimit (0, (int) lengthBeats.size() - 1, length)];
            const auto phase = beats / cycle - std::floor (beats / cycle);
            switch (juce::jlimit (0, 2, shape))
            {
                case 0: return phase;
                case 1: return 1.0 - phase;
                default:
                {
                    const auto two = beats / (2.0 * cycle) - std::floor (beats / (2.0 * cycle));
                    return two < 0.5 ? 2.0 * two : 2.0 - 2.0 * two;
                }
            }
        }
        // The sweep's cutoff (Hz) at position `s` with these settings, before AMOUNT.
        static float sweepHz (double s, const Settings& st) noexcept
        {
            return juce::jlimit (30.0f, 20000.0f, centerHz (st.center) * std::exp2 (((float) s - 0.5f) * rangeOctaves (st.range)));
        }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getAmount() const noexcept    { return amountShown.load (std::memory_order_relaxed); }
        float getPosition() const noexcept  { return positionShown.load (std::memory_order_relaxed); }   // 0-1
        float getCutoffHz() const noexcept  { return cutoffShown.load (std::memory_order_relaxed); }
        float getGainDb() const noexcept    { return gainShown.load (std::memory_order_relaxed); }

    private:
        struct Side
        {
            float s1 = 0.0f, s2 = 0.0f;   // the resonant stage
            float t1 = 0.0f, t2 = 0.0f;   // the plain stage
            std::array<float, 4> ap {};   // the phaser's allpasses
            float last = 0.0f;            // the phaser's feedback
        };

        double sampleRate = 48000.0;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f, cutoffCoef = 0.0f, levelCoef = 0.0f, gainCoef = 0.0f;
        double ownBeats = 0.0;
        float logCutoff = 0.0f;   // glided (a sweep that starts again from the bottom eases down over ~15 ms)
        std::array<Side, 2> sides {};
        float powerIn = 0.0f, powerOut = 0.0f, gain = 1.0f;
        int settledSamples = 0;
        // Coefficients worked out every 16 samples.
        int countdown = 0;
        float g = 0.0f, k = 2.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k2 = 1.4142f, b1 = 1.0f, b2 = 0.0f, b3 = 0.0f;
        std::array<float, 2> apCoef {};
        float res = 0.0f, phaserMix = 0.0f, wet = 0.0f;

        std::atomic<float> amountShown { 0.0f }, positionShown { 0.0f }, cutoffShown { 20000.0f }, gainShown { 0.0f };
    };
}
