#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // HAZE (0.66.0, NIGHTDRIVE's signature, the owner's name): streetlights passing; our own design. In time with the
    // song, the sound sweeps past you, left to right, then right to left on the next pass, like headlights sweeping
    // across a windscreen: louder, brighter and nearer as it passes, quieter, darker and further at the edges, with
    // a touch of Doppler (the pitch a little up as it comes, down as it goes, from how fast it's nearing or leaving).
    // It slows as it turns at each edge, so the pitch, pan and level all move smoothly: nothing jumps at a turn.
    //
    // The source is a point moving along a road (x from -1.6 to +1.6, -cos of the passes) passing 0.35 from you:
    // distance d = sqrt (x^2 + 0.35^2), nearness 0.35 / d. DEPTH sets how much of it is heard: the level falls by
    // (1 - nearness) (about -13.5 dB at the edges at full), the top darkens towards TONE's far cutoff, the balance
    // leans with x / d x WIDTH (each side kept its own), and the sound is delayed by its distance (sound's travel
    // time, scaled so the steepest pitch change is DOPPLER's cents): the Doppler comes from that delay changing, as
    // it does in the air. A make-up gain keeps a whole pass as loud on average as the sound was (most of a pass is
    // spent far off: without it full DEPTH lost 3-4 dB on NIGHTDRIVE's presets), so it swells above as it passes and
    // falls below at the edges; exactly 1 at DEPTH 0. WHOOSH: a breath of wind as it passes, following the music (silence stays silent).
    // At DEPTH 0 (or off, once settled) the sound passes untouched, bit for bit.
    class Haze
    {
    public:
        static inline const juce::StringArray divisionNames { "1/2 BAR", "1 BAR", "2 BARS", "4 BARS" };   // never reorder
        static constexpr std::array<double, 4> divisionBeats { 2.0, 4.0, 8.0, 16.0 };

        struct Settings
        {
            bool on = false;
            int division = 1;       // divisionNames: a pass a bar
            float depth = 0.5f;     // 0 to 1
            float doppler = 0.4f;   // 0 to 1: the steepest shift 0 to 40 cents
            float width = 0.8f;     // 0 to 1
            float tone = 0.4f;      // 0 to 1: the far cutoff 1.5 kHz to 12 kHz
            float whoosh = 0.3f;    // 0 to 1
        };

        static constexpr double roadLength = 1.6, closest = 0.35;
        static float dopplerCents (float doppler) noexcept { return 40.0f * juce::jlimit (0.0f, 1.0f, doppler); }
        static float farHz (float tone) noexcept { return 1500.0f * std::pow (8.0f, juce::jlimit (0.0f, 1.0f, tone)); }
        // The level at a point on the road for a DEPTH, and the make-up that keeps a whole pass at the sound's level.
        static float levelAt (double x, float depth) noexcept
        {
            return 1.0f - depth * (1.0f - (float) (closest / std::sqrt (x * x + closest * closest)));
        }
        static float makeUpFor (float depth) noexcept;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { state = seed == 0 ? 0x2545f491u : seed; }
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return depthNow <= 0.0f && whooshLevel <= 0.0f; }

        // For the page and the unit: where the source is on the road (-1.6 to 1.6), the pitch shift now (cents) and
        // the level change now (dB).
        float getPosition() const noexcept { return positionShown.load (std::memory_order_relaxed); }
        float getShiftCents() const noexcept { return shiftShown.load (std::memory_order_relaxed); }
        float getLevelDb() const noexcept { return levelShown.load (std::memory_order_relaxed); }

    private:
        float read (int side, double back) const noexcept;
        float random01() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) (state >> 8) * (1.0f / 16777216.0f);
        }

        double sampleRate = 48000.0;
        std::array<std::vector<float>, 2> lines;
        int writeIndex = 0, lineSize = 0;
        double ownPasses = 0.0, delayScale = 0.0;
        float depthNow = 0.0f, smoothing = 0.0f, lastDelay = 0.0f;
        std::array<float, 2> toneState {}, ic1 {}, ic2 {};
        float follow = 0.0f, followUp = 0.0f, followDown = 0.0f, whooshLevel = 0.0f;
        std::uint32_t state = 0x2545f491u;
        std::atomic<float> positionShown { -1.6f }, shiftShown { 0.0f }, levelShown { 0.0f };
    };
}
