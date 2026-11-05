#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // BLOOM (0.74.0, GRAND CINEMA's signature, the owner's choice of two offered (a sympathetic bloom over reverse
    // swells); our own design, no piano or hall modelled from measurements). AMOUNT takes a piano from close and
    // intimate to vast:
    // - RESONANCE: a piano's undamped strings ringing along with what's played. A pool of 24 "strings" a side, each a
    //   tuned comb (a delay one period long fed back through a gentle low-pass: a string's whole harmonic series), tuned
    //   to the notes sounding and their neighbours on the soundboard (an octave below, the note, an octave above, an
    //   octave and a fifth above), excited by the sound itself, so only what the music has in common with a string
    //   sets it ringing; strings keep ringing after the keys let go (as with the pedal down), RING long (1.5-12 s). A
    //   new pitch takes the quietest string; the left side's strings sit 1.5 cents flat, the right's 1.5 sharp (two
    //   strings a note, beating slowly).
    // - BLOOM: a hall (eight delay lines through a Householder matrix, damped by TONE in the loop, SIZE its fall time
    //   2-12 s) fed the sound and the strings through a slow swell: the longer you hold, the more goes in (SWELL
    //   0.15-4 s to open, 2 s to close once the playing stops), so held chords open out into the room.
    // The sound passes unchanged; both are added, x AMOUNT (smoothed 30 ms). AMOUNT under 0.001 counts as 0; at 0, once
    // settled, untouched bit for bit and emptied so it wakes clean (`isResting`).
    class Bloom
    {
    public:
        static constexpr int maxNotes = 16, numStrings = 24, lines = 8;

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;      // 0 to 1: the macro's target
            float resonance = 0.5f;   // 0 to 1: the strings' share
            float ring = 0.5f;        // 0 to 1: the strings' fall, 1.5 to 12 s
            float size = 0.5f;        // 0 to 1: the hall's fall, 2 to 12 s
            float swell = 0.5f;       // 0 to 1: how slowly the hall opens, 0.15 to 4 s
            float tone = 0.5f;        // 0 to 1: the hall's top, 1.5 to 15 kHz
            float width = 0.8f;       // 0 to 1
        };

        static float ringSeconds (float r) noexcept  { return 1.5f * std::pow (8.0f, juce::jlimit (0.0f, 1.0f, r)); }
        static float sizeSeconds (float s) noexcept  { return 2.0f * std::pow (6.0f, juce::jlimit (0.0f, 1.0f, s)); }
        static float swellSeconds (float w) noexcept { return 0.15f * std::pow (26.0f, juce::jlimit (0.0f, 1.0f, w)); }
        static float toneHz (float t) noexcept       { return 1500.0f * std::pow (10.0f, juce::jlimit (0.0f, 1.0f, t)); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void setNotes (const float* pitches, int count) noexcept;   // the notes sounding (MIDI pitches)
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return amountNow <= 0.0f; }

        // For the page: each string's pitch (MIDI, < 0 unused) and level (0-1), the hall's level (dBFS) and how open
        // the swell is (0-1).
        float getStringPitch (int k) const noexcept { return stringPitchShown[(size_t) k].load (std::memory_order_relaxed); }
        float getStringLevel (int k) const noexcept { return stringLevelShown[(size_t) k].load (std::memory_order_relaxed); }
        float getHallDb() const noexcept { return hallShown.load (std::memory_order_relaxed); }
        float getSwell() const noexcept { return swellShown.load (std::memory_order_relaxed); }
        float getAmount() const noexcept { return amountShown.load (std::memory_order_relaxed); }

    private:
        struct String
        {
            int pitch = -1;                       // MIDI note, -1 unused
            std::array<std::vector<float>, 2> line;
            std::array<int, 2> write {};
            std::array<double, 2> delay {};
            std::array<float, 2> low {};
            float gain = 0.0f;                    // the loop's gain for RING at this pitch
            float energy = 0.0f;                  // its level, followed
            bool wanted = false;
        };

        void tune (String&, int pitch) noexcept;
        float ringGain (int pitch, float ringS) const noexcept;

        double sampleRate = 48000.0;
        std::array<String, numStrings> strings;
        float ringNow = -1.0f;
        std::array<std::vector<float>, lines> hall;
        std::array<int, lines> hallLength {}, hallWrite {};
        std::array<float, lines> hallLow {};
        float amountNow = 0.0f, amountRate = 0.0f, swellNow = 0.0f, follower = 0.0f;
        float followUp = 0.0f, followDown = 0.0f, closeRate = 0.0f, energyRate = 0.0f;
        std::array<std::atomic<float>, numStrings> stringPitchShown {}, stringLevelShown {};
        std::atomic<float> hallShown { -100.0f }, swellShown { 0.0f }, amountShown { 0.0f };
    };
}
