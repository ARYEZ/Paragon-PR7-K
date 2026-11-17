#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

namespace effects
{
    // WOBBLE (0.97.0, the owner's choice for the dubstep bank's MACRO 3; our own design, no product modelled). The
    // dubstep wobble as a performance, locked to the song. One knob, AMOUNT, brings it in:
    //  - a resonant 24 dB low-pass (a resonant state-variable stage into a plain one) whose cutoff opens and closes from
    //    LOW (40-800 Hz) up RANGE (2-7 octaves), on a log scale;
    //  - PATTERN (never reorder `patternNames`): the wobble's speed beat by beat over two bars (`patterns`, in wobbles a
    //    beat: 1/2, 1/4, 1/8, 1/8 T, 1/16, 1/16 T, 1/32), so it changes rate as a dubstep bass does. Every speed is a
    //    whole number of wobbles a beat (1/2 only in pairs from an even beat), so each beat starts with the filter shut
    //    and a change of speed never jumps;
    //  - SHAPE (never reorder `shapeNames`): SMOOTH (a cosine, the classic wub), SNAP (open at once, falling fast: the
    //    punchy one), RISE (opening across each wobble, the "yoi");
    //  - TALK: a vowel filter (the VOICE filter's five bass formants, `synth::vocal`, each 2.5 times as wide and at least
    //    100 Hz so a low bass's harmonics fall in them) moving from OO (shut) through OH to AH (open) with the wobble,
    //    blended in before the low-pass: the "talking" growl, the "wow" (OO to EH was tried first: both ends put their
    //    first formant near 350-420 Hz and the vowel hardly showed);
    //  - DRIVE: tanh going in (scaled so a 0.3 level stays 0.3), the grit under the wobble;
    //  - RESONANCE: how much the low-pass sings.
    // AMOUNT moves the cutoff in from fully open (20 kHz) to the wobble's, the resonance, TALK and DRIVE with it, and
    // blends the filtered sound in over its first third. The level is kept (power in and out followed over 0.3 s, gain
    // 0.25-4, glided: at most +12 dB, since at 0.35-2.8 Growl Bass came out 6 dB quieter at full), since a wobble's shut half takes far more away than its open half gives back. In time with `ppq`
    // while the song plays (4/4); its own count while stopped, from the pattern's start when it wakes. Each side its own
    // filters, the same settings. AMOUNT smoothed 30 ms; under 0.001 it counts as 0, and at 0, once the gain is back to
    // 1 for 0.3 s, the sound passes untouched bit for bit (`isResting`).
    class Wobble
    {
    public:
        static inline const juce::StringArray patternNames { "STEADY", "HALF-TIME", "TRIPLETS", "DROP", "ROLLS", "TALKER", "STUTTER", "BOUNCE" };   // never reorder
        static inline const juce::StringArray shapeNames { "SMOOTH", "SNAP", "RISE" };   // never reorder
        static constexpr int patternBeats = 8;
        // Wobbles a beat, beat by beat over two bars.
        static constexpr std::array<std::array<float, patternBeats>, 8> patterns {{
            {{ 2, 2, 2, 2, 2, 2, 2, 2 }},              // STEADY: eighths
            {{ 1, 1, 1, 1, 1, 1, 1, 1 }},              // HALF-TIME: quarters
            {{ 3, 3, 3, 3, 3, 3, 3, 3 }},              // TRIPLETS
            {{ 2, 2, 2, 2, 3, 3, 4, 4 }},              // DROP: eighths, then a triplet and sixteenth fill
            {{ 1, 1, 2, 2, 4, 4, 8, 8 }},              // ROLLS: speeding up to 32nds
            {{ 1, 2, 1, 3, 1, 2, 4, 6 }},              // TALKER: a phrase that talks
            {{ 4, 3, 8, 2, 6, 4, 8, 8 }},              // STUTTER
            {{ 0.5f, 0.5f, 2, 2, 0.5f, 0.5f, 3, 4 }},  // BOUNCE: a long wub, then quick ones
        }};

        struct Settings
        {
            bool on = false;
            float amount = 0.0f;      // 0 to 1 (the macro's)
            int pattern = 0;          // patternNames: STEADY
            int shape = 0;            // shapeNames: SMOOTH
            float low = 0.4f;         // 0 to 1: 40 Hz to 800 Hz (log)
            float range = 0.6f;       // 0 to 1: 2 to 7 octaves
            float resonance = 0.55f;  // 0 to 1
            float talk = 0.0f;        // 0 to 1
            float drive = 0.3f;       // 0 to 1
        };

        static float lowHz (float l) noexcept { return 40.0f * std::pow (20.0f, juce::jlimit (0.0f, 1.0f, l)); }
        static float rangeOctaves (float r) noexcept { return 2.0f + 5.0f * juce::jlimit (0.0f, 1.0f, r); }
        // The wobbles a beat at `beats` into the song.
        static float rateAt (double beats, int pattern) noexcept
        {
            const auto& p = patterns[(size_t) juce::jlimit (0, (int) patterns.size() - 1, pattern)];
            auto beat = (int) std::floor (beats - patternBeats * std::floor (beats / patternBeats));
            return p[(size_t) juce::jlimit (0, patternBeats - 1, beat)];
        }
        static juce::String rateName (float rate)
        {
            if (rate < 0.75f) return "1/2";
            if (rate < 1.5f) return "1/4";
            if (rate < 2.5f) return "1/8";
            if (rate < 3.5f) return "1/8 T";
            if (rate < 5.0f) return "1/16";
            if (rate < 7.0f) return "1/16 T";
            return "1/32";
        }
        // How open the wobble is (0 shut to 1 open) at `beats` into the song.
        static double openAt (double beats, int pattern, int shape) noexcept
        {
            const auto cycles = beats * rateAt (beats, pattern);
            const auto phase = cycles - std::floor (cycles);
            switch (juce::jlimit (0, 2, shape))
            {
                case 1: return std::exp (-4.0 * phase) * juce::jmin (1.0, phase * 40.0);   // SNAP (opening over 1/40 of a wobble)
                case 2: return phase * phase * (3.0 - 2.0 * phase) * juce::jmin (1.0, (1.0 - phase) * 12.0);   // RISE
                default: return 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * phase);   // SMOOTH
            }
        }
        // The wobble's cutoff (Hz) at openness `w`, before AMOUNT.
        static float wobbleHz (double w, const Settings& st) noexcept
        {
            return juce::jlimit (30.0f, 20000.0f, lowHz (st.low) * std::exp2 ((float) w * rangeOctaves (st.range)));
        }
        // TALK's vowel (0 OO to 1 EE, as the VOICE filter's) at openness `w`: OO shut, AH open.
        static float vowelAt (double w) noexcept { return 0.5f * (float) w; }

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        // `ppq`: the song position at the block's start (-1 while stopped: then its own count).
        void process (juce::dsp::AudioBlock<float> block, const Settings&, double bpm, double ppq) noexcept;
        bool isResting() const noexcept { return resting; }

        // For the page and the tests.
        float getAmount() const noexcept    { return amountShown.load (std::memory_order_relaxed); }
        float getOpen() const noexcept      { return openShown.load (std::memory_order_relaxed); }      // 0-1
        float getBeat() const noexcept      { return beatShown.load (std::memory_order_relaxed); }      // 0 to 8, in the pattern
        float getCutoffHz() const noexcept  { return cutoffShown.load (std::memory_order_relaxed); }
        float getGainDb() const noexcept    { return gainShown.load (std::memory_order_relaxed); }

    private:
        static constexpr int numFormants = 5;
        struct Side
        {
            float s1 = 0.0f, s2 = 0.0f;   // the resonant stage
            float t1 = 0.0f, t2 = 0.0f;   // the plain stage
            std::array<float, numFormants> f1 {}, f2 {};   // TALK's band-passes
        };

        double sampleRate = 48000.0;
        bool resting = true;
        float amountNow = 0.0f, amountCoef = 0.0f, cutoffCoef = 0.0f, levelCoef = 0.0f, gainCoef = 0.0f;
        double ownBeats = 0.0;
        float logCutoff = 0.0f;   // glided (SNAP's opening eases over ~4 ms)
        float vowelNow = 0.0f;
        std::array<Side, 2> sides {};
        float powerIn = 0.0f, powerOut = 0.0f, gain = 1.0f;
        int settledSamples = 0;
        // Coefficients worked out every 16 samples.
        int countdown = 0;
        float g = 0.0f, k = 2.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k2 = 1.4142f, b1 = 1.0f, b2 = 0.0f, b3 = 0.0f;
        std::array<float, numFormants> fa1 {}, fa2 {}, fa3 {}, fk {}, fGain {};
        float talkMix = 0.0f, drive = 1.0f, driveScale = 1.0f, wet = 0.0f;

        std::atomic<float> amountShown { 0.0f }, openShown { 0.0f }, beatShown { 0.0f }, cutoffShown { 20000.0f }, gainShown { 0.0f };
    };
}
