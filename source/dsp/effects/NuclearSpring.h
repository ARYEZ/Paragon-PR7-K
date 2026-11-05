#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // NUCLEARSPRING (0.69.0, the owner's name, option 1 of two offered: a vintage spring reverb tank over a 1950s
    // radio); our own design, no particular tank modelled. The drippy, twangy echo of a mid-century amp or organ's
    // spring reverb: three springs a side of different lengths. In a spring, a sound spreads out as it travels (its
    // low part and its high part arrive at different times), which is the "drip" and the chirping "boing": here a
    // chain of 40 all-pass filters in each spring's loop, each spread over a few samples ((a + z^-M) / (1 + a z^-M),
    // M about the sample rate / 9600) so the spread falls below about 4.8 kHz, where a spring's chirp is (a plain
    // first-order all-pass only delays the very top: 400 Hz and 3 kHz came out 0.06 ms apart), with the spring's
    // travel time as a delay, a TONE low-pass, and the loop's loss setting the DECAY.
    // - DRIP: how far the sound spreads (the all-passes' coefficient 0 to 0.6): from a plain echo to a wet chirp.
    //   Each trip along a spring spreads 400 Hz and 3 kHz about 2 ms apart (measured), and the spread builds with
    //   every trip round, so the drip grows through the tail; a bigger chirp in one trip would take well over twice
    //   the filters (50 filters and a stronger spread gave no more than 40: the design's own limit).
    // - DECAY: 0.8 to 5 s to fall 60 dB. TONE: the tank's top 2 to 8 kHz (springs are dark). SIZE: the springs'
    //   lengths x 0.7 to x 1.4 (a small amp's short tank to a long organ tank).
    // - BOING: a hard hit (the level a few milliseconds long well over the level of the last 60 ms) is pushed up to
    //   4x harder into the tank, through a soft clip: the clang of a kicked spring tank.
    // - WIDTH: each side its own springs (0: the sides' tails mixed to the middle). Below about 110 Hz little goes in
    //   (two one-pole low cuts before the tank, a third in each loop at 80 Hz: a spring can't carry the lows, and
    //   with one cut a 40 Hz tone came through only 14 dB under 1 kHz). MIX: the tank added under the sound (which passes unchanged).
    // MIX under 0.001 counts as 0; at 0 untouched bit for bit, and the tank emptied so it wakes clean (`isResting`).
    class NuclearSpring
    {
    public:
        static constexpr int springsPerSide = 3, stages = 40, maxSpread = 16;

        struct Settings
        {
            bool on = false;
            float mix = 0.5f;     // 0 to 1
            float decay = 0.4f;   // 0 to 1: 0.8 to 5 s
            float drip = 0.5f;    // 0 to 1
            float tone = 0.5f;    // 0 to 1: 2 to 8 kHz
            float size = 0.5f;    // 0 to 1: x 0.7 to x 1.4
            float boing = 0.4f;   // 0 to 1
            float width = 0.7f;   // 0 to 1
        };

        static float decaySeconds (float decay) noexcept { return 0.8f * std::pow (6.25f, juce::jlimit (0.0f, 1.0f, decay)); }
        static float dripCoefficient (float drip) noexcept { return 0.6f * juce::jlimit (0.0f, 1.0f, drip); }
        static float toneHz (float tone) noexcept { return 2000.0f * std::pow (4.0f, juce::jlimit (0.0f, 1.0f, tone)); }
        static float sizeScale (float size) noexcept { return 0.7f * std::pow (2.0f, juce::jlimit (0.0f, 1.0f, size)); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return mixNow <= 0.0f; }

        // For the page and the unit: the tank's level coming out (dBFS, the last block), how hard the last hit was
        // pushed in (1 to 4), and each spring's swing now (for drawing them shake).
        float getLevelDb() const noexcept { return levelShown.load (std::memory_order_relaxed); }
        float getKick() const noexcept { return kickShown.load (std::memory_order_relaxed); }
        float getSwing (int side, int spring) const noexcept
        {
            return swingShown[(size_t) juce::jlimit (0, 1, side)][(size_t) juce::jlimit (0, springsPerSide - 1, spring)].load (std::memory_order_relaxed);
        }

    private:
        struct Spring
        {
            std::vector<float> line;
            int write = 0;
            float lengthMs = 40.0f;
            // Each all-pass's last M inputs and outputs (ring buffers sharing `spreadAt`).
            std::array<std::array<float, maxSpread>, stages> xHistory {}, yHistory {};
            float tone = 0.0f, feedback = 0.0f, swing = 0.0f, loopCut = 0.0f;
        };

        double sampleRate = 48000.0;
        std::array<std::array<Spring, springsPerSide>, 2> springs;
        std::array<float, 2> lowCut {}, lowCut2 {};
        int spread = 5, spreadAt = 0;
        float loopCutRate = 0.0f;
        float fastLevel = 0.0f, slowLevel = 0.0f, kick = 1.0f;
        float fastRate = 0.0f, slowRate = 0.0f, kickRelease = 0.0f, lowCutRate = 0.0f, smoothing = 0.0f;
        float mixNow = 0.0f;
        std::atomic<float> levelShown { -100.0f }, kickShown { 1.0f };
        std::array<std::array<std::atomic<float>, springsPerSide>, 2> swingShown {};
    };
}
