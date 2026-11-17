#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // CANYON (0.84.0, the western bank's signature, the owner's choice; our own design, no unit modelled). One knob,
    // AMOUNT, sends the sound into a canyon:
    //  - NEAR WALLS: three slapbacks off rock walls close by (SIZE: the first 45-200 ms after the sound, the others
    //    1.53x and 2.21x that), each from its own side (WIDTH), each further one darker (TONE: 1.5-9 kHz, x 0.8 a
    //    wall) and smeared by the rock (two short allpasses a side);
    //  - THE FAR WALL: the distant reply, in time with the song (FAR: a quarter, a half, a bar or two bars, the host's
    //    tempo; never over 6 s), bouncing from one side to the other (a ping-pong pair), each pass darker still and
    //    thinned under 120 Hz, REPEATS how many come back (feedback 0 to 0.7).
    // The walls hear the middle of the sound (left and right together): a wall answers whatever reaches it. The
    // echoes are added; the sound passes unchanged. AMOUNT is the send (smoothed 30 ms; under 0.001 it counts as 0),
    // so turning it down lets the echoes already in the canyon ring out; at 0, once they've died (under -100 dB for
    // 0.5 s), the sound passes untouched bit for bit (`isResting`) and everything is cleared.
    class Canyon
    {
    public:
        static inline const juce::StringArray farNames { "1/4", "1/2", "1 BAR", "2 BARS" };   // never reorder
        static constexpr std::array<double, 4> farBeats { 1.0, 2.0, 4.0, 8.0 };
        static constexpr int numNear = 3;

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;    // 0 to 1: how much of the sound goes into the canyon (the macro's)
            float size = 0.5f;      // 0 to 1: the near walls, 45 to 200 ms
            int far = 1;            // farNames: 1/2
            float repeats = 0.4f;   // 0 to 1: the far wall's feedback, 0 to 0.7
            float tone = 0.5f;      // 0 to 1: dark to bright (1.5 to 9 kHz)
            float width = 0.8f;     // 0 to 1: how far to the sides the walls are
        };

        static float nearSeconds (float size) noexcept { return 0.045f + 0.155f * juce::jlimit (0.0f, 1.0f, size); }
        static float toneHz (float tone) noexcept { return 1500.0f * std::pow (6.0f, juce::jlimit (0.0f, 1.0f, tone)); }
        static float feedbackFor (float repeats) noexcept { return 0.7f * juce::jlimit (0.0f, 1.0f, repeats); }
        static double farSeconds (int far, double bpm) noexcept
        {
            const auto beats = farBeats[(size_t) juce::jlimit (0, (int) farBeats.size() - 1, far)];
            return juce::jmin (maxFarSeconds, beats * 60.0 / juce::jmax (20.0, bpm));
        }
        static constexpr double maxFarSeconds = 6.0;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getAmount() const noexcept { return amountShown.load (std::memory_order_relaxed); }
        // Each wall's echo level just now, 0-1 (0-2 the near walls, 3 the far one).
        float getWallLevel (int wall) const noexcept
        {
            return wall >= 0 && wall <= numNear ? wallShown[(size_t) wall].load (std::memory_order_relaxed) : 0.0f;
        }
        float getFarSecondsShown() const noexcept { return farShown.load (std::memory_order_relaxed); }

    private:
        struct OnePole
        {
            float z = 0.0f;
            float low (float x, float a) noexcept { z += a * (x - z); return z; }
        };
        struct Allpass
        {
            std::vector<float> line;
            int pos = 0;
            float process (float x, float g) noexcept
            {
                const auto delayed = line[(size_t) pos];
                const auto v = x + g * delayed;
                line[(size_t) pos] = v;
                pos = pos + 1 < (int) line.size() ? pos + 1 : 0;
                return delayed - g * v;
            }
        };

        static float read (const std::vector<float>& line, int write, double delaySamples) noexcept;

        double sampleRate = 48000.0;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f;
        int quietSamples = 0;

        std::vector<float> nearLine;            // the middle of the sound
        int nearWrite = 0;
        std::array<OnePole, numNear> nearLow {};
        std::array<std::array<Allpass, 2>, 2> rock {};   // per side, two allpasses

        std::array<std::vector<float>, 2> farLines;      // the ping-pong pair
        int farWrite = 0;
        double farDelay = 0.0;                            // samples, glided towards the tempo's
        std::array<OnePole, 2> farLow {}, farHighLow {};

        std::array<float, numNear + 1> wallLevel {};
        std::atomic<float> amountShown { 0.0f }, farShown { 0.0f };
        std::array<std::atomic<float>, numNear + 1> wallShown {};
    };
}
