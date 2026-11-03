#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // SPECTRE (0.67.0, CYBERSHRINE's signature, the owner's name, option 1 of two offered: a ghost that follows the
    // sound over a metal shell ringing in sympathy); our own design. A whispered shadow trails each note: the shape
    // of its tone (its vowel, its brightness) with the pitch taken out, played on breath. A channel vocoder: each
    // side's sound through 24 band-passes a third of an octave apart (90 Hz up; two stages each, so a band hears
    // little of its neighbours: one stage let a low note into every high band), each followed by a level follower
    // (4 ms up, LINGER down: the time to fall 60 dB, 0.1 to 4 s); then breath noise through 24 more band-passes at
    // the same places, each set to its band's level (the noise's level through a band worked out from its width, so
    // a band comes out as loud as it went in), the lows thinned under 250 Hz (dark breath is rumble).
    // - AIR: the breath's bands from narrow (hollow, whistling) to wide (breathy); two stages each too, so a band's
    //   breath stays near its band (one stage spread a low note's ghost up into the top).
    // - SHIFT: the outline moved up or down to an octave (a third of an octave a band, read between bands), a
    //   smaller or larger ghost.
    // - DRIFT: each band wanders between the sides on its own slow swing, and the breath on each side its own
    //   (at 0 one breath for both, each side's ghost from that side alone).
    // - TRAIL: the ghost 0 to 800 ms behind. MIX: how loud it is, added under the sound (which passes unchanged).
    // MIX under 0.001 counts as 0; at 0 untouched bit for bit and nothing kept (`isResting`). In silence the
    // followers fall to nothing (cut at -120 dB), so silence stays silent.
    class Spectre
    {
    public:
        static constexpr int numBands = 24;
        static constexpr double lowestHz = 90.0;
        static constexpr double analysisQ = 2.8;    // each of two stages: together a third of an octave wide
        static constexpr double maxTrail = 0.8;

        struct Settings
        {
            bool on = false;
            float mix = 0.5f;      // 0 to 1
            float trail = 0.3f;    // 0 to 1: 0 to 800 ms behind
            float linger = 0.4f;   // 0 to 1: 0.1 to 4 s to fall 60 dB
            float air = 0.6f;      // 0 hollow to 1 breathy
            float drift = 0.5f;    // 0 to 1
            float shift = 0.0f;    // -1 to 1: an octave down to an octave up
        };

        static double bandHz (int band) noexcept { return lowestHz * std::pow (2.0, band / 3.0); }
        static float trailMs (float trail) noexcept { return 1000.0f * (float) maxTrail * juce::jlimit (0.0f, 1.0f, trail); }
        static float lingerSeconds (float linger) noexcept { return 0.1f * std::pow (40.0f, juce::jlimit (0.0f, 1.0f, linger)); }
        static float shiftSemitones (float shift) noexcept { return 12.0f * juce::jlimit (-1.0f, 1.0f, shift); }
        static float breathQ (float air) noexcept { return 1.4f * std::pow (8.0f, 1.0f - juce::jlimit (0.0f, 1.0f, air)); }   // each stage

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { state = seed == 0 ? 0x2545f491u : seed; }
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return mixNow <= 0.0f; }

        // For the page and the unit: each band's level going in (both sides, dB), the ghost's outline (dB, after
        // SHIFT), and the ghost's level coming out over the last block (dBFS).
        float getBandDb (int band) const noexcept { return bandShown[(size_t) juce::jlimit (0, numBands - 1, band)].load (std::memory_order_relaxed); }
        float getGhostBandDb (int band) const noexcept { return ghostShown[(size_t) juce::jlimit (0, numBands - 1, band)].load (std::memory_order_relaxed); }
        float getGhostDb() const noexcept { return ghostDb.load (std::memory_order_relaxed); }
        float getDriftOf (int band) const noexcept;   // -1 (left) to 1 (right): where the band has wandered

    private:
        struct Band
        {
            float g = 0.0f, k = 0.0f, a1 = 0.0f, a2 = 0.0f;
            void set (double hz, double q, double rate) noexcept
            {
                g = (float) std::tan (juce::MathConstants<double>::pi * hz / rate);
                k = (float) (1.0 / q);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
            }
        };
        struct State
        {
            float ic1 = 0.0f, ic2 = 0.0f;
            float run (const Band& b, float x) noexcept
            {
                const auto v1 = b.a1 * ic1 + b.a2 * (x - ic2);
                const auto v2 = ic2 + b.g * v1;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                return b.k * v1;   // unity at the peak
            }
        };

        void setBreath (float air) noexcept;
        float random() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) (state >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }
        float readLine (int side, double back) const noexcept;

        double sampleRate = 48000.0;
        std::array<Band, numBands> analysis {}, breath {};
        std::array<std::array<State, numBands>, 2> analysisState {}, analysisState2 {}, breathState {}, breathState2 {};
        std::array<std::array<float, numBands>, 2> power {};   // each band's followed power, per side
        std::array<float, numBands> breathGain {}, weight {}, swingPhase {}, swingRate {};
        float breathAir = -1.0f;
        float attack = 0.0f, smoothing = 0.0f, mixNow = 0.0f, trailNow = 0.0f;
        std::array<std::vector<float>, 2> lines;
        int lineSize = 0, writeIndex = 0;
        std::uint32_t state = 0x2545f491u;
        std::array<std::atomic<float>, numBands> bandShown {}, ghostShown {};
        std::atomic<float> ghostDb { -100.0f };
    };
}
