#pragma once

#include <juce_dsp/juce_dsp.h>

#include "dsp/synth/VoiceFilter.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace effects
{
    // LEVIATHAN (0.68.0, UNSPOKEN's signature, the owner's name, option 2 of two offered: a vast wordless voice
    // over a palindrome loop); our own design. A huge, slow voice singing far beneath what you play, like a creature
    // answering from the fog: not a copy of your sound but a new one.
    // - Its note: the lowest note sounding in the synth (`setNotes`, as DOWNPOUR's), DEPTH below (1 or 2 octaves:
    //   never reorder `depthNames`), gliding to a new note over about 120 ms, a slow wavering in pitch.
    // - Its throat: a band-limited buzz (polyBLEP saw, softened by a one-pole) through five formant resonators (the
    //   VOICE filter's bass vowels, every formant scaled down by SIZE: 1 to 0.55 times, a bigger throat), the vowel
    //   wandering on its own between OO and EE (DRIFT: a new place every 6 s down to 0.8 s, glided).
    // - GROWL: rougher each cycle (pitch and level jitter), every other cycle weaker (creak: a lower shadow), and
    //   clicks in the throat (short noise bursts, 2 to 12 a second, through the same vowel).
    // - Each side its own throat, a few cents apart, its vowel a step away: a wide voice.
    // - Its level follows yours (power 40 ms up, 0.5 s down) and is kept at it (the voice's own level measured and
    //   matched, whatever the vowel), x MIX: so it breathes with your playing and silence stays silent.
    // - DELAY: it answers 0 to 1.5 s behind (its note and level both).
    // MIX under 0.001 counts as 0; at 0 untouched bit for bit, and everything let go so it wakes clean (`isResting`).
    class Leviathan
    {
    public:
        static inline const juce::StringArray depthNames { "1 OCTAVE", "2 OCTAVES" };   // never reorder
        static constexpr int maxNotes = 64;
        static constexpr double maxDelay = 1.5;

        struct Settings
        {
            bool on = false;
            float mix = 0.5f;     // 0 to 1
            int depth = 0;        // depthNames
            float drift = 0.4f;   // 0 to 1: the vowel moves every 6 s down to 0.8 s
            float growl = 0.3f;   // 0 to 1
            float delay = 0.2f;   // 0 to 1: 0 to 1.5 s behind
            float size = 0.6f;    // 0 to 1: formants x 1 down to x 0.55
        };

        static float delaySeconds (float delay) noexcept { return (float) maxDelay * juce::jlimit (0.0f, 1.0f, delay); }
        static float driftSeconds (float drift) noexcept { return 6.0f * std::pow (0.8f / 6.0f, juce::jlimit (0.0f, 1.0f, drift)); }
        static float sizeScale (float size) noexcept { return 1.0f - 0.45f * juce::jlimit (0.0f, 1.0f, size); }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        void seedRandom (std::uint32_t seed) noexcept { state = seed == 0 ? 0x2545f491u : seed; }
        // The notes sounding now (MIDI, fractional for glides); none keeps the last one.
        void setNotes (const float* notes, int count) noexcept;
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;
        bool isResting() const noexcept { return mixNow <= 0.0f; }

        // For the page and the unit: the note it sings (MIDI, -1 none), its vowel (0 OO to 1 EE), its level (dBFS)
        // and its roughness now.
        float getSungNote() const noexcept { return sungShown.load (std::memory_order_relaxed); }
        float getVowel() const noexcept { return vowelShown.load (std::memory_order_relaxed); }
        float getLevelDb() const noexcept { return levelShown.load (std::memory_order_relaxed); }

    private:
        struct Throat
        {
            double phase = 0.0;
            float soft = 0.0f, cycleGain = 1.0f, cycleStretch = 1.0f;
            bool odd = false;
            synth::vocal::Channel formants;
            float click = 0.0f, clickLeft = 0.0f;
        };

        float random01() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) (state >> 8) * (1.0f / 16777216.0f);
        }
        void setVowel (float vowel, float scale) noexcept;
        static float polyBlep (double t, double dt) noexcept;

        double sampleRate = 48000.0;
        std::array<Throat, 2> throats {};
        std::array<synth::vocal::Coefficients, 2> vowels {};
        float lowestNote = -1.0f, pitchNow = -1.0f;
        float vowelNow = 0.3f, vowelTarget = 0.3f, vowelTimer = 0.0f, vowelShownValue = 0.3f;
        float waverPhase = 0.0f, clickTimer = 0.5f;
        float inPower = 0.0f, voicePower = 0.0f, levelNow = 0.0f, mixNow = 0.0f;
        float powerUp = 0.0f, powerDown = 0.0f, voicePowerRate = 0.0f, glide = 0.0f, smoothing = 0.0f;
        // DELAY: the note and the level going in, every `controlStep` samples.
        static constexpr int controlStep = 32;
        std::vector<float> delayedNote, delayedPower;
        int delayWrite = 0, controlCount = 0;
        float heldNote = -1.0f, heldPower = 0.0f;
        int countdown = 0;
        std::uint32_t state = 0x2545f491u;
        std::atomic<float> sungShown { -1.0f }, vowelShown { 0.3f }, levelShown { -100.0f };
    };
}
