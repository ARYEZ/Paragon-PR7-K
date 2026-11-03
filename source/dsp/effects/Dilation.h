#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // DILATION (0.63.0, SIMULACRUM's signature, the owner's name): time slowing down; our own design. Not a haze or an
    // echo added to the sound (that's BLACKHOLE's and LIGHTTRAIL's job): the sound itself is played back slower, pitch
    // and speed dropping together as a tape does when it winds down, so a run you play comes out stretched and lower,
    // dragging behind real time.
    //
    // Each side keeps the last few seconds it heard. One read head plays them back at SPEED (1 down to a quarter: SLOW,
    // up to two octaves down), sliding there over RAMP. Reading slower than the sound arrives falls behind it: when it's
    // REACH behind, the head can't fall further and fades (60 ms) back to the present, where it starts slowing again.
    // When SLOW comes back to 0 (or the effect is switched off), RETURN says how time catches up: GLIDE speeds the
    // head up past real time (the pitch rises a little) until it's caught up; SNAP fades straight back to the present.
    // TONE: slowed tape loses its top; at 0 the slower it goes the darker it gets, at 1 no darkening at all. MIX blends
    // the slowed sound with the sound as played (1: only the slowed one).
    //
    // At SLOW 0 and caught up the sound passes untouched, bit for bit (the head reads the sample just written).
    class Dilation
    {
    public:
        static inline const juce::StringArray returnNames { "GLIDE", "SNAP" };   // never reorder

        struct Settings
        {
            bool on = false;
            float slow = 0.0f;          // 0 to 1: speed 1 down to 1/4 (0 to -24 semitones)
            float ramp = 0.5f;          // 0 to 1: how quickly it slides, 0.05 s to 4 s
            int returnMode = 0;         // returnNames
            float reach = 2.0f / 3.0f;  // 0 to 1: how far behind it can fall, 1 s to 8 s
            float tone = 0.5f;          // 0 to 1
            float mix = 1.0f;           // 0 to 1
        };

        static constexpr float maxReachSeconds = 8.0f;
        static float speedFor (float slow) noexcept { return std::exp2 (-2.0f * juce::jlimit (0.0f, 1.0f, slow)); }
        static float semitonesFor (float slow) noexcept { return slow <= 0.0f ? 0.0f : -24.0f * juce::jmin (1.0f, slow); }   // (0, not -0)
        static float rampSeconds (float ramp) noexcept { return 0.05f * std::pow (80.0f, juce::jlimit (0.0f, 1.0f, ramp)); }
        static float reachSeconds (float reach) noexcept { return std::pow (maxReachSeconds, juce::jlimit (0.0f, 1.0f, reach)); }
        // TONE: the slowed sound's top, from 20 kHz at full speed down with the speed (squared at TONE 0).
        static float toneHz (float tone, float speed) noexcept
        {
            return 20000.0f * std::pow (juce::jlimit (0.01f, 1.0f, speed), 2.0f * (1.0f - juce::jlimit (0.0f, 1.0f, tone)));
        }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        // Caught up and at full speed: the sound passes untouched.
        bool isResting() const noexcept { return lag <= 0.0 && speedLog == 0.0f && fadeGain <= 0.0f; }

        // For the page and the unit: the speed now (1 = real time), how far behind real time (seconds), the slowed
        // clock (seconds of the sound played back since it started: the page's hand), and whether it's catching up.
        float getSpeed() const noexcept { return speedShown.load (std::memory_order_relaxed); }
        float getBehindSeconds() const noexcept { return behindShown.load (std::memory_order_relaxed); }
        double getSlowClock() const noexcept { return clockShown.load (std::memory_order_relaxed); }
        bool isCatchingUp() const noexcept { return catchingShown.load (std::memory_order_relaxed); }

    private:
        float read (int side, double back) const noexcept;
        void startFade() noexcept;

        double sampleRate = 48000.0;
        std::array<std::vector<float>, 2> lines;
        int writeIndex = 0;
        double lag = 0.0, fadeLag = 0.0;   // samples behind the present: the head, and the one fading out
        float speedLog = 0.0f;             // log2 of the speed
        float fadeGain = 0.0f, fadeStep = 0.0f;
        float mixNow = 1.0f, mixSmoothing = 0.0f;
        std::array<float, 2> toneState {};
        double clock = 0.0;
        std::atomic<float> speedShown { 1.0f }, behindShown { 0.0f };
        std::atomic<double> clockShown { 0.0 };
        std::atomic<bool> catchingShown { false };
    };
}
