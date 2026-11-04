#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // LIGHTSPEED (0.73.0, HYPERDRIVE's signature, option 1 of two offered: a jump to lightspeed over a flickering
    // hologram); our own design. JUMP takes the sound from rest to lightspeed:
    // - STREAKS: three copies of the sound pitched up (each a third further: at full RISE, +8, +16 and +24 semitones)
    //   by delay-line pitch shifters (two read heads 60 ms apart, raised-cosine faded so they add to one), each a little
    //   later than the last (12, 27, 45 ms) and panned out to alternate sides by WIDTH; their pitch follows JUMP through
    //   a slow glide (0.25 s), so a jump spools up and falls back like engines rather than stepping. Darkened by TONE
    //   (a one-pole, 2 to 16 kHz), brighter as JUMP rises. Made from the middle (left + right) / 2.
    // - WHOOSH: noise through a band-pass sweeping 200 Hz to 6 kHz with JUMP (each side its own noise), following the
    //   music's level (10 ms up, 0.3 s down; silence stays silent).
    // - The sound itself ducks by up to a third at full JUMP, so the jump rushes past instead of piling on top.
    // JUMP under 0.001 counts as 0; at 0, once its glides have settled, untouched bit for bit and everything emptied so
    // it wakes clean (`isResting`).
    class LightSpeed
    {
    public:
        static constexpr int streaks = 3;

        struct Settings
        {
            bool on = false;
            float jump = 0.0f;     // 0 to 1: the macro's target
            float rise = 0.6f;     // 0 to 1: the top streak's pitch at full JUMP, 0 to +24 semitones
            float streak = 0.6f;   // 0 to 1: the streaks' level
            float whoosh = 0.4f;   // 0 to 1
            float width = 0.8f;    // 0 to 1
            float tone = 0.6f;     // 0 to 1: 2 to 16 kHz
        };

        static float riseSemitones (float rise) noexcept { return 24.0f * juce::jlimit (0.0f, 1.0f, rise); }
        static float toneHz (float tone) noexcept { return 2000.0f * std::pow (8.0f, juce::jlimit (0.0f, 1.0f, tone)); }
        static float whooshHz (float jump) noexcept { return 200.0f * std::pow (30.0f, juce::jlimit (0.0f, 1.0f, jump)); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return jumpNow <= 0.0f && spool <= 0.0f; }
        void seedRandom (std::uint32_t seed) noexcept { noise = seed != 0 ? seed : 1u; }

        // For the page and the unit: how far into the jump it is now (0 to 1, the glided pitch's share of full), the
        // top streak's pitch now (semitones) and the whoosh's level (dBFS, the last block).
        float getSpool() const noexcept { return spoolShown.load (std::memory_order_relaxed); }
        float getPitchSemitones() const noexcept { return pitchShown.load (std::memory_order_relaxed); }
        float getWhooshDb() const noexcept { return whooshShown.load (std::memory_order_relaxed); }

    private:
        struct Shifter
        {
            double phase = 0.0;   // 0 to 1 across the window: the first head's place (the second half a window on)
        };

        float readLine (double delaySamples) const noexcept;
        float nextNoise() noexcept
        {
            noise ^= noise << 13;
            noise ^= noise >> 17;
            noise ^= noise << 5;
            return (float) (int32_t) noise * (1.0f / 2147483648.0f);
        }

        double sampleRate = 48000.0;
        std::vector<float> line;   // the middle, as heard (1 s)
        int write = 0;
        std::array<Shifter, streaks> shifters {};
        std::array<float, 2 * streaks> toneState {};
        std::array<float, 2> bandLow {}, bandBand {};   // the whoosh's TPT SVFs, a side each
        float jumpNow = 0.0f, spool = 0.0f, follower = 0.0f;
        float jumpRate = 0.0f, spoolRate = 0.0f, followUp = 0.0f, followDown = 0.0f;
        double window = 2880.0;    // 60 ms
        std::uint32_t noise = 0x9e3779b9u;
        std::atomic<float> spoolShown { 0.0f }, pitchShown { 0.0f }, whooshShown { -100.0f };
    };
}
