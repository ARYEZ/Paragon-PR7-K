#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // BLACKHOLE (0.62.0, LAST LIGHT's signature, the owner's name): the last moments of the sound caught and stretched
    // into a haze that hangs under the next notes, as if time were slowing down; our own design.
    //
    // Each side keeps a few seconds of what it heard. Grains (short slices, 160-240 ms, raised-cosine faded) are lifted
    // from somewhere in the last SIZE of it, one after another and overlapping, so the slices blur into a steady haze
    // whatever was played. The haze is fed back in with the sound (HOLD: how long it hangs, from about half a second to
    // twenty), each pass read a little slower (PULL: the haze sinks in pitch, a little more each time round, falling in),
    // darker (TONE) and spread (WIDTH: each side its own slices, or both alike). The haze is added to the sound, which
    // passes unchanged; at MIX 0 (or off, once settled) nothing at all is added.
    class BlackHole
    {
    public:
        struct Settings
        {
            bool on = false;
            float hold = 0.5f;    // 0 to 1: the haze's fall time, 0.5 s to 20 s (to -60 dB)
            float size = 0.5f;    // 0 to 1: how far back slices are taken, 0.3 s to 3 s
            float pull = 0.0f;    // 0 to 1: how far each pass sinks, 0 to -2 semitones
            float tone = 0.5f;    // 0 dark (1.2 kHz) to 1 bright (14 kHz)
            float width = 0.8f;   // 0 to 1
            float mix = 0.0f;     // 0 to 1
        };

        static constexpr int grainsPerSide = 8;
        static float holdSeconds (float hold) noexcept { return 0.5f * std::pow (40.0f, juce::jlimit (0.0f, 1.0f, hold)); }
        static float sizeSeconds (float size) noexcept { return 0.3f + 2.7f * juce::jlimit (0.0f, 1.0f, size); }
        static float pullSemitones (float pull) noexcept { return pull <= 0.0f ? 0.0f : -2.0f * juce::jmin (1.0f, pull); }   // (0, not -0)
        static float toneHz (float tone) noexcept { return 1200.0f * std::pow (14000.0f / 1200.0f, juce::jlimit (0.0f, 1.0f, tone)); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { state = seed == 0 ? 0x2545f491u : seed; }
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return mixNow <= 0.0f && hazePower < 1.0e-14f; }

        // For the page: the haze's level now (dB, -100 when silent), and each grain's place (0 now, 1 SIZE ago) and how
        // far through it is (0-1, or -1 when idle).
        float getHazeDb() const noexcept { return hazeShown.load (std::memory_order_relaxed); }
        float getGrainPlace (int g) const noexcept { return placeShown[(size_t) juce::jlimit (0, grainsPerSide - 1, g)].load (std::memory_order_relaxed); }
        float getGrainProgress (int g) const noexcept { return progressShown[(size_t) juce::jlimit (0, grainsPerSide - 1, g)].load (std::memory_order_relaxed); }

    private:
        struct Grain
        {
            double readPos = 0.0;   // in samples, into the line
            int length = 0, age = 0;
            float place = 0.0f;
            bool active = false;
        };

        float random01() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) (state >> 8) * (1.0f / 16777216.0f);
        }
        float readLine (int side, double pos) const noexcept;
        void startGrain (int side, Grain&, float sizeSamples) noexcept;

        double sampleRate = 48000.0;
        std::array<std::vector<float>, 2> lines;
        int writeIndex = 0;
        std::array<std::array<Grain, grainsPerSide>, 2> grains {};
        std::array<float, 2> toneState {}, haze {};
        int spawnCountdown = 0;
        std::uint32_t state = 0x2545f491u;
        float mixNow = 0.0f, smoothing = 0.0f, hazePower = 0.0f;
        std::atomic<float> hazeShown { -100.0f };
        std::array<std::atomic<float>, grainsPerSide> placeShown {}, progressShown {};
    };
}
