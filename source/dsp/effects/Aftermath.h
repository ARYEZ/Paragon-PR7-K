#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // AFTERMATH (0.79.0, the owner's name; STOLEN SKY's signature, option 1 of two offered (from under an open sky into
    // a sealed bunker, over an endless Shepard rise), chosen after hearing both sketched on one lead; our own design,
    // no room or building measured or modelled). One knob, SEAL, takes the sound from outdoors into a sealed concrete
    // room:
    //  - the room closes in: a small, hard room (four short lines through a Householder mix, ROOM sizing them 0.6 to
    //    1.6 times) is blended in, the walls ringing longer with RING (each pass 0.5 to 0.92 of the last, a one-pole in
    //    the loop at 3.5-9 kHz so concrete stays bright but not glassy);
    //  - the top darkens: the sound itself through a 12 dB low-pass whose cutoff falls from 20 kHz to DARK's 1.2-6 kHz,
    //    blended in by SEAL (at 0 the plain sound);
    //  - the machinery: a mains hum (HUM HZ 50 or 60: never reorder `humNames`, with a 2nd and 3rd harmonic) and the
    //    air of a ventilation shaft (noise through a 120-600 Hz band, each side its own), at HUM's -66 to -30 dB, only
    //    while there's music (a follower: 10 ms up, 0.6 s down, gone under about -64 dB), so silence stays silent.
    // The level is kept: power in and out (before the hum) followed over 0.3 s, the gain held to -6 to +4 dB and glided.
    // SEAL is smoothed (30 ms); under 0.001 it counts as 0, and at 0, once the room has died away (under 1e-6 for
    // 0.3 s) and the gain is back to 1, the sound passes untouched bit for bit (`isResting`) and everything is cleared.
    class Aftermath
    {
    public:
        static constexpr int numLines = 4;
        static inline const juce::StringArray humNames { "50 HZ", "60 HZ" };

        struct Settings
        {
            bool on = false;
            float seal = 0.0f;    // 0 to 1: open sky to sealed bunker (the macro's)
            float room = 0.4f;    // 0 to 1: the bunker's size
            float ring = 0.5f;    // 0 to 1: how long the walls ring
            float dark = 0.5f;    // 0 to 1: how dark when sealed, 6 kHz down to 1.2 kHz
            float hum = 0.4f;     // 0 to 1: the machinery, -66 to -30 dB (0 none)
            int humHz = 1;        // humNames
            float width = 0.7f;   // 0 to 1: the room's width
        };

        static float darkHz (float d) noexcept { return 6000.0f * std::pow (0.2f, juce::jlimit (0.0f, 1.0f, d)); }
        static float roomScale (float r) noexcept { return 0.6f + juce::jlimit (0.0f, 1.0f, r); }
        static float ringGain (float r) noexcept { return 0.5f + 0.42f * juce::jlimit (0.0f, 1.0f, r); }
        static float humDb (float h) noexcept { return -66.0f + 36.0f * juce::jlimit (0.0f, 1.0f, h); }
        // The room's ring: seconds to fall 60 dB at ROOM and RING (for the page and the tests).
        static float ringSeconds (float room, float ring) noexcept;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { random = seed != 0 ? seed : 0x2545f491u; }
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getSeal() const noexcept     { return sealShown.load (std::memory_order_relaxed); }
        float getRoomLevel() const noexcept { return roomShown.load (std::memory_order_relaxed); }   // the room's level, 0-1
        float getHumLevel() const noexcept { return humShown.load (std::memory_order_relaxed); }     // the machinery now, 0-1
        float getGainDb() const noexcept   { return gainShown.load (std::memory_order_relaxed); }

    private:
        float noise() noexcept
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return (float) (random >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }

        double sampleRate = 48000.0;
        std::uint32_t random = 0x2545f491u;
        bool resting = true;
        float sealNow = 0.0f, sealCoef = 0.0f;

        // The room: four lines, their read lengths set by ROOM.
        std::array<std::vector<float>, numLines> lines;
        std::array<int, numLines> writes {}, lengths {};
        int lineSize = 0;
        float lengthsFor = -1.0f;
        std::array<float, numLines> loopTone {};
        float roomEnergy = 0.0f;
        int quietSamples = 0;

        // The darkened sound: two one-poles a side.
        std::array<float, 2> darkA {}, darkB {};

        // The machinery.
        double humPhase = 0.0;
        std::array<float, 2> airLow {}, airHigh {};
        float follower = 0.0f, followUp = 0.0f, followDown = 0.0f;

        float powerIn = 0.0f, powerOut = 0.0f, levelCoef = 0.0f, gainNow = 1.0f;

        std::atomic<float> sealShown { 0.0f }, roomShown { 0.0f }, humShown { 0.0f }, gainShown { 0.0f };
    };
}
