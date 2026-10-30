#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>

// The VOICE filter (0.39.0, the owner's ask for a choir that sounds human): a formant filter, our own design.
// A voice gets its vowels from resonances of the throat and mouth ("formants") that stay at the same frequencies
// whatever note is sung: that's what the Vowels wavetable can't do (its vowels move with the note, going thin
// up high and muddy down low). Here five band-pass resonators sit at a vowel's five formants, fixed in Hz,
// summed in parallel. VOWEL morphs OO - OH - AH - EH - EE; SIZE morphs a bass singer's formants into a soprano's;
// SHARP narrows or widens them; MIX blends with the sound going in.
//
// The formant data (frequency, level, bandwidth for each vowel) is the widely published table for bass and
// soprano singers used in sound-synthesis textbooks and the Csound manual's appendix. The blending between
// vowels and sizes is ours (frequencies and bandwidths on a log scale, levels in dB).
namespace synth::vocal
{
    inline constexpr int numFormants = 5;
    inline constexpr int numVowels = 5;
    inline const juce::StringArray vowelNames { "OO", "OH", "AH", "EH", "EE" };

    struct Formant
    {
        float hz, db, bandwidthHz;
    };
    using Vowel = std::array<Formant, numFormants>;

    // In VOWEL's order: OO (u), OH (o), AH (a), EH (e), EE (i).
    inline constexpr std::array<Vowel, numVowels> bass {{
        {{ { 350.0f, 0.0f, 40.0f }, { 600.0f, -20.0f, 80.0f }, { 2400.0f, -32.0f, 100.0f }, { 2675.0f, -28.0f, 120.0f }, { 2950.0f, -36.0f, 120.0f } }},
        {{ { 400.0f, 0.0f, 40.0f }, { 750.0f, -11.0f, 80.0f }, { 2400.0f, -21.0f, 100.0f }, { 2600.0f, -20.0f, 120.0f }, { 2900.0f, -40.0f, 120.0f } }},
        {{ { 600.0f, 0.0f, 60.0f }, { 1040.0f, -7.0f, 70.0f }, { 2250.0f, -9.0f, 110.0f }, { 2450.0f, -9.0f, 120.0f }, { 2750.0f, -20.0f, 130.0f } }},
        {{ { 400.0f, 0.0f, 40.0f }, { 1620.0f, -12.0f, 80.0f }, { 2400.0f, -9.0f, 100.0f }, { 2800.0f, -12.0f, 120.0f }, { 3100.0f, -18.0f, 120.0f } }},
        {{ { 250.0f, 0.0f, 60.0f }, { 1750.0f, -30.0f, 90.0f }, { 2600.0f, -16.0f, 100.0f }, { 3050.0f, -22.0f, 120.0f }, { 3340.0f, -28.0f, 120.0f } }},
    }};
    inline constexpr std::array<Vowel, numVowels> soprano {{
        {{ { 325.0f, 0.0f, 50.0f }, { 700.0f, -16.0f, 60.0f }, { 2700.0f, -35.0f, 170.0f }, { 3800.0f, -40.0f, 180.0f }, { 4950.0f, -60.0f, 200.0f } }},
        {{ { 450.0f, 0.0f, 70.0f }, { 800.0f, -11.0f, 80.0f }, { 2830.0f, -22.0f, 100.0f }, { 3800.0f, -22.0f, 130.0f }, { 4950.0f, -50.0f, 135.0f } }},
        {{ { 800.0f, 0.0f, 80.0f }, { 1150.0f, -6.0f, 90.0f }, { 2900.0f, -32.0f, 120.0f }, { 3900.0f, -20.0f, 130.0f }, { 4950.0f, -50.0f, 140.0f } }},
        {{ { 350.0f, 0.0f, 60.0f }, { 2000.0f, -20.0f, 100.0f }, { 2800.0f, -15.0f, 120.0f }, { 3600.0f, -40.0f, 150.0f }, { 4950.0f, -56.0f, 200.0f } }},
        {{ { 270.0f, 0.0f, 60.0f }, { 2140.0f, -12.0f, 90.0f }, { 2950.0f, -26.0f, 100.0f }, { 3900.0f, -26.0f, 120.0f }, { 4950.0f, -44.0f, 120.0f } }},
    }};

    // The five formants for a VOWEL (0 OO to 1 EE) and SIZE (0 bass to 1 soprano).
    inline Vowel formantsFor (float vowel, float size) noexcept
    {
        const auto position = juce::jlimit (0.0f, 1.0f, vowel) * (float) (numVowels - 1);
        const auto first = juce::jlimit (0, numVowels - 2, (int) position);
        const auto t = position - (float) first;
        const auto s = juce::jlimit (0.0f, 1.0f, size);
        const auto logBlend = [] (float a, float b, float x) { return a * std::pow (b / a, x); };
        const auto at = [&] (const std::array<Vowel, numVowels>& table, int f)
        {
            const auto& a = table[(size_t) first][(size_t) f];
            const auto& b = table[(size_t) first + 1][(size_t) f];
            return Formant { logBlend (a.hz, b.hz, t), a.db + (b.db - a.db) * t, logBlend (a.bandwidthHz, b.bandwidthHz, t) };
        };
        Vowel v;
        for (int f = 0; f < numFormants; ++f)
        {
            const auto low = at (bass, f), high = at (soprano, f);
            v[(size_t) f] = { logBlend (low.hz, high.hz, s), low.db + (high.db - low.db) * s, logBlend (low.bandwidthHz, high.bandwidthHz, s) };
        }
        return v;
    }

    // The resonators' coefficients (a band-pass state-variable filter each, unity at its peak) and levels.
    struct Coefficients
    {
        std::array<float, numFormants> a1 {}, a2 {}, a3 {}, k {}, gain {};
        std::array<float, numFormants> hz {}, q {};   // for working out a note's level (levelling, below)

        // `noteHz`: a note above the first formant lifts it to just above the note, as a soprano opens a vowel on
        // high notes (singers call it vowel modification): otherwise the formant would cut the note itself.
        static Coefficients make (float vowel, float size, float sharp, double sampleRate, double noteHz = 0.0) noexcept
        {
            // SHARP 0.5 is the table's bandwidths; 0 twice as wide, 1 half as wide.
            const auto widen = std::exp2 (1.0 - 2.0 * (double) juce::jlimit (0.0f, 1.0f, sharp));
            const auto formants = formantsFor (vowel, size);
            Coefficients c;
            for (size_t f = 0; f < (size_t) numFormants; ++f)
            {
                auto hz = (double) formants[f].hz;
                if (f == 0 && noteHz * 1.1 > hz)
                    hz = noteHz * 1.1;
                hz = juce::jlimit (20.0, 0.45 * sampleRate, hz);
                const auto q = hz / juce::jmax (10.0, (double) formants[f].bandwidthHz * widen);
                const auto g = std::tan (juce::MathConstants<double>::pi * hz / sampleRate);
                const auto k = 1.0 / q;
                const auto a1 = 1.0 / (1.0 + g * (g + k));
                c.a1[f] = (float) a1;
                c.a2[f] = (float) (g * a1);
                c.a3[f] = (float) (g * g * a1);
                c.k[f] = (float) k;
                c.gain[f] = juce::Decibels::decibelsToGain (formants[f].db);
                c.hz[f] = (float) hz;
                c.q[f] = (float) q;
            }
            return c;
        }

        static Coefficients lerp (const Coefficients& from, const Coefficients& to, float t) noexcept
        {
            Coefficients c;
            for (size_t f = 0; f < (size_t) numFormants; ++f)
            {
                c.a1[f] = from.a1[f] + (to.a1[f] - from.a1[f]) * t;
                c.a2[f] = from.a2[f] + (to.a2[f] - from.a2[f]) * t;
                c.a3[f] = from.a3[f] + (to.a3[f] - from.a3[f]) * t;
                c.k[f] = from.k[f] + (to.k[f] - from.k[f]) * t;
                c.gain[f] = from.gain[f] + (to.gain[f] - from.gain[f]) * t;
                c.hz[f] = to.hz[f];
                c.q[f] = to.q[f];
            }
            return c;
        }
    };

    // Lifts the resonators' sum back to about the level going in (a bright saw through AH): measured in the tests.
    inline constexpr float makeUp = 4.0f;

    // LEVELLING: narrow formants make some notes jump out (a harmonic right on a formant, up to +10 dB) and others
    // drop away (harmonics either side of it), as a sampled choir's notes are levelled, so is each note here: the
    // gain that brings a saw at `noteHz` back to the level it had going in. Worked out from the resonators' shapes
    // (each harmonic's level through them, their phases ignored: an estimate), only when a note starts, moves
    // or its vowel changes. Kept within -12 to +18 dB.
    inline float levelling (const Coefficients& c, double noteHz, double sampleRate) noexcept
    {
        if (noteHz <= 0.0)
            return 1.0f;
        auto through = 0.0, going = 0.0;
        const auto top = juce::jmin (6000.0, 0.45 * sampleRate);
        for (auto h = 1; h <= 64 && h * noteHz < top; ++h)
        {
            const auto f = h * noteHz;
            const auto weight = 1.0 / ((double) h * h);   // a saw's harmonics, by power
            auto response = 0.0;
            for (size_t i = 0; i < (size_t) numFormants; ++i)
            {
                const auto ratio = f / c.hz[i] - c.hz[i] / f;
                const auto band = 1.0 / std::sqrt (1.0 + (double) c.q[i] * c.q[i] * ratio * ratio);
                response += (double) c.gain[i] * band;
            }
            response *= makeUp;
            through += weight * response * response;
            going += weight;
        }
        if (through <= 0.0)
            return 1.0f;
        return (float) juce::jlimit (0.25, 8.0, std::sqrt (going / through));
    }

    // One side of one note's VOICE filter: the five resonators' states.
    class Channel
    {
    public:
        void reset() noexcept
        {
            s1.fill (0.0f);
            s2.fill (0.0f);
        }

        float process (float x, const Coefficients& c) noexcept
        {
            auto sum = 0.0f;
            for (size_t f = 0; f < (size_t) numFormants; ++f)
            {
                const auto v3 = x - s2[f];
                const auto v1 = c.a1[f] * s1[f] + c.a2[f] * v3;
                const auto v2 = s2[f] + c.a2[f] * s1[f] + c.a3[f] * v3;
                s1[f] = 2.0f * v1 - s1[f];
                s2[f] = 2.0f * v2 - s2[f];
                sum += c.gain[f] * c.k[f] * v1;   // band-pass, unity at its peak
            }
            return sum * makeUp;
        }

    private:
        std::array<float, numFormants> s1 {}, s2 {};
    };
}
