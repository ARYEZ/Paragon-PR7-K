#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // LEGION (0.76.0, MAESTRO's signature, the owner's name and choice (an ensemble size over a growing hall); our own
    // design, nothing measured from a section or copied from a product). SIZE takes a sound from one player to a full
    // section: up to eight more "players" join one at a time, each the sound read back from a short line a little
    // late (its timing: 2 ms to LOOSE's 6-30 ms, each its own share), its delay swaying slowly (0.09-0.23 Hz, each
    // its own) so it drifts up to DETUNE's 0-14 cents in and out of tune, as players never sit exactly together; each
    // placed at its own spot across the stage (SPREAD), darkened a little (TONE, 3-16 kHz: a section's blur). The
    // players hear the middle of the sound (left and right together) and are added round it, the sound itself kept as
    // the leader; the whole is kept as loud as the sound going in: at once by an estimate from the players' weights
    // (right for a centred sound), then by measurement (in and out followed over about 0.3 s, the correction held to
    // -6 to +4 dB and glided), so a full section is bigger, not louder (the estimate alone was 4.6 dB out on wide
    // noise, where the players hear less than either side; the measurement alone swelled 4 dB as they joined).
    // SIZE is smoothed (40 ms); under 0.001 it counts as 0, and at 0, once
    // settled, the sound passes untouched bit for bit (`isResting`); the line keeps recording while it's on so a
    // player joining hears the present, and is cleared when it's switched on.
    class Legion
    {
    public:
        static constexpr int numPlayers = 8;

        struct Settings
        {
            bool on = false;
            float size = 0.0f;     // 0 to 1: how many players (the macro's)
            float spread = 0.8f;   // 0 to 1: how wide the stage
            float loose = 0.4f;    // 0 to 1: how far apart in time, 6 to 30 ms at the latest
            float detune = 0.4f;   // 0 to 1: how far out of tune they drift, 0 to 14 cents
            float tone = 0.6f;     // 0 to 1: the players' top, 3 to 16 kHz
        };

        static float looseMs (float l) noexcept   { return 6.0f + 24.0f * juce::jlimit (0.0f, 1.0f, l); }
        static float detuneCents (float d) noexcept { return 14.0f * juce::jlimit (0.0f, 1.0f, d); }
        static float toneHz (float t) noexcept    { return 3000.0f * std::pow (16.0f / 3.0f, juce::jlimit (0.0f, 1.0f, t)); }
        // Each player's own place (-1 to 1, before SPREAD) and share of LOOSE's lateness (for the page, at rest too).
        static float placeOf (int k) noexcept;
        static float latenessOf (int k) noexcept;
        // Players at SIZE (the sound itself counts as the first): 1 to 9.
        static float playersAt (float size) noexcept { return 1.0f + (float) numPlayers * juce::jlimit (0.0f, 1.0f, size); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return sizeNow <= 0.0f; }

        // For the page: each player's share (0-1), place (-1 left to 1 right), lateness (ms) and drift now (-1 to 1).
        float getPlayerWeight (int k) const noexcept { return weightShown[(size_t) k].load (std::memory_order_relaxed); }
        float getPlayerPan (int k) const noexcept    { return panShown[(size_t) k].load (std::memory_order_relaxed); }
        float getPlayerDelayMs (int k) const noexcept { return delayShown[(size_t) k].load (std::memory_order_relaxed); }
        float getPlayerDrift (int k) const noexcept  { return driftShown[(size_t) k].load (std::memory_order_relaxed); }
        float getSize() const noexcept { return sizeShown.load (std::memory_order_relaxed); }

    private:
        float readAt (double delaySamples) const noexcept;

        double sampleRate = 48000.0;
        std::vector<float> line;          // the middle of the sound, a power of two long
        int mask = 0, write = 0;
        bool wasOn = false;
        float sizeNow = 0.0f, sizeCoef = 0.0f;
        std::array<double, numPlayers> driftPhase {};
        std::array<float, numPlayers> delayNow {};   // samples, as of the last chunk
        std::array<float, 2> toneState {};
        float powerIn = 0.0f, powerOut = 0.0f, levelCoef = 0.0f, gainNow = 1.0f;   // the level kept (see Legion.cpp)
        std::array<std::atomic<float>, numPlayers> weightShown {}, panShown {}, delayShown {}, driftShown {};
        std::atomic<float> sizeShown { 0.0f };
    };
}
