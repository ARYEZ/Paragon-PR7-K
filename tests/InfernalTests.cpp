// INFERNAL (0.59.0): parallel destruction. Checked by what it does to the sound: off and MIX 0 leave it exactly as it
// was; the lows under TIGHT pass clean; each chain alone adds harmonics, and the chains come out at about one level;
// the noise is pulsed at the note's pitch; FEEDBACK's bloom swells in an octave up while a note is held; silence in is
// silence out; CPU.
#include "dsp/effects/Infernal.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <vector>

namespace
{
    constexpr double rate = 48000.0;
    constexpr int block = 256;
    constexpr double pi2 = juce::MathConstants<double>::twoPi;

    // `seconds` of `make (t)` (the same on both sides) through INFERNAL with `notes` sounding; the left side out.
    template <typename Make>
    std::vector<float> run (effects::Infernal::Settings s, double seconds, Make make, std::vector<float> notes = { 45.0f })
    {
        effects::Infernal fx;
        fx.prepare (rate, block);
        fx.seedRandom (5);
        fx.setNotes (notes.data(), (int) notes.size());
        std::vector<float> l ((size_t) block), r ((size_t) block), out;
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) (seconds * rate / block); ++b)
        {
            for (size_t i = 0; i < (size_t) block; ++i)
                l[i] = r[i] = make ((double) (t + (long long) i) / rate);
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) block), s);
            out.insert (out.end(), l.begin(), l.end());
            t += block;
        }
        return out;
    }

    float sine (double hz, double t, float level = 0.5f) { return level * (float) std::sin (pi2 * hz * t); }

    // A 110 Hz saw (A2), band-limited by summing its harmonics up to 10 kHz.
    float saw (double t)
    {
        double v = 0.0;
        for (int h = 1; h * 110.0 < 10000.0; ++h)
            v += std::sin (pi2 * 110.0 * h * t) / h;
        return (float) (0.3 * v);
    }

    // The level of `hz` in x[from, from + length) (Goertzel over whole cycles).
    double levelAt (const std::vector<float>& x, double hz, size_t from, size_t length)
    {
        length = (size_t) (std::floor ((double) length * hz / rate) * rate / hz);
        const auto k = 2.0 * std::cos (pi2 * hz / rate);
        double s1 = 0.0, s2 = 0.0;
        for (auto i = from; i < from + length; ++i)
        {
            const auto s0 = x[i] + k * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return 2.0 * std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - k * s1 * s2)) / (double) length;
    }

    double db (double g) { return 20.0 * std::log10 (std::max (1.0e-12, g)); }
    double rmsDb (const std::vector<float>& x, size_t from)
    {
        double sum = 0.0;
        for (auto i = from; i < x.size(); ++i)
            sum += (double) x[i] * x[i];
        return 10.0 * std::log10 (sum / (double) (x.size() - from) + 1.0e-24);
    }

    effects::Infernal::Settings only (int chain)
    {
        effects::Infernal::Settings s;
        s.on = true;
        s.mix = 1.0f;
        s.tight = 0.0f;
        s.feedback = 0.0f;
        s.fuzz = chain == 0 ? 1.0f : 0.0f;
        s.crush = chain == 1 ? 1.0f : 0.0f;
        s.amp = chain == 2 ? 1.0f : 0.0f;
        s.noise = chain == 3 ? 1.0f : 0.0f;
        return s;
    }
}

TEST_CASE ("INFERNAL: off and MIX 0 leave the sound exactly as it was", "[infernal]")
{
    const auto dry = run (effects::Infernal::Settings {}, 1.0, [] (double) { return 0.0f; });
    const auto input = [] (double t) { return saw (t); };
    std::vector<float> expected;
    for (size_t i = 0; i < (size_t) (rate); ++i)
        expected.push_back (saw ((double) i / rate));
    expected.resize ((size_t) (1.0 * rate / block) * block);

    effects::Infernal::Settings off;
    off.mix = 1.0f;   // off: MIX doesn't matter
    CHECK (run (off, 1.0, input) == expected);
    effects::Infernal::Settings zero;
    zero.on = true;
    zero.mix = 0.0f;
    zero.drive = 1.0f;
    CHECK (run (zero, 1.0, input) == expected);
    (void) dry;
}

TEST_CASE ("INFERNAL: the lows under TIGHT pass clean under the destruction", "[infernal]")
{
    auto s = only (0);
    s.tight = 1.0f;   // 400 Hz
    s.drive = 0.8f;
    const auto both = [] (double t) { return sine (45.0, t, 0.4f) + sine (1000.0, t, 0.2f); };
    const auto out = run (s, 1.0, both);
    const auto from = (size_t) (0.3 * rate), length = (size_t) (0.6 * rate);
    const auto sub = db (levelAt (out, 45.0, from, length));
    INFO ("45 Hz in: " << db (0.4) << " dB, out: " << sub << " dB");
    CHECK (std::abs (sub - db (0.4)) < 1.0);   // the sub as it was (two one-poles at 400 Hz: under a dB at 45 Hz)
    // ...and the 1 kHz part destroyed: its 3rd harmonic (3 kHz) is there.
    CHECK (db (levelAt (out, 3000.0, from, length)) > -40.0);
}

TEST_CASE ("INFERNAL: each chain adds harmonics, and they come out at about one level", "[infernal]")
{
    const char* names[] = { "FUZZ", "CRUSH", "AMP", "NOISE" };
    double levels[4] {};
    for (int c = 0; c < 4; ++c)
    {
        const auto out = run (only (c), 1.5, [] (double t) { return saw (t); });
        levels[c] = rmsDb (out, (size_t) (0.3 * rate));
        INFO (names[c] << ": " << levels[c] << " dB (the saw in: about -13 dB)");
        if (c < 3)
        {
            // Distortion: a pure 220 Hz sine comes out with a strong 3rd harmonic it never had.
            const auto bent = run (only (c), 1.0, [] (double t) { return sine (220.0, t, 0.5f); });
            const auto first = levelAt (bent, 220.0, (size_t) (0.3 * rate), (size_t) (0.6 * rate));
            const auto third = levelAt (bent, 660.0, (size_t) (0.3 * rate), (size_t) (0.6 * rate));
            INFO ("3rd harmonic " << db (third) - db (first) << " dB under the fundamental");
            CHECK (db (third) - db (first) > -25.0);
        }
    }
    for (int c = 0; c < 4; ++c)
    {
        INFO (names[c] << " " << levels[c] << " dB vs FUZZ " << levels[0] << " dB");
        CHECK (std::abs (levels[c] - levels[0]) < 7.0);
    }
}

TEST_CASE ("INFERNAL: the noise is pulsed at the pitch of the notes sounding", "[infernal]")
{
    // A held A2 (110 Hz): the noise chain's output, squared (its loudness from moment to moment), has a strong 110 Hz
    // line: the noise bursts once a cycle, which the ear hears as a pitched growl.
    const auto out = run (only (3), 2.0, [] (double t) { return sine (110.0, t, 0.5f); }, { 45.0f });
    std::vector<float> power;
    for (auto v : out)
        power.push_back (v * v);
    const auto from = (size_t) (0.3 * rate), length = (size_t) (1.5 * rate);
    const auto atNote = levelAt (power, 110.0, from, length);
    const auto offNote = levelAt (power, 137.0, from, length);
    INFO ("the noise's loudness at 110 Hz " << db (atNote) << " dB, at 137 Hz " << db (offNote) << " dB");
    CHECK (db (atNote) > db (offNote) + 15.0);
}

TEST_CASE ("INFERNAL: FEEDBACK blooms in an octave up while a note is held", "[infernal]")
{
    const auto bloomGrowth = [] (float feedback)
    {
        auto s = only (2);
        s.feedback = feedback;
        s.drive = 0.3f;
        const auto out = run (s, 2.5, [] (double t) { return sine (110.0, t, 0.3f); }, { 45.0f });
        // The AMP turns 110 Hz into odd harmonics only; the bloom is a 220 Hz line rising.
        const auto early = levelAt (out, 220.0, (size_t) (0.05 * rate), (size_t) (0.2 * rate));
        const auto late = levelAt (out, 220.0, (size_t) (2.0 * rate), (size_t) (0.4 * rate));
        const auto note = levelAt (out, 110.0, (size_t) (2.0 * rate), (size_t) (0.4 * rate));
        return std::array<double, 3> { db (early), db (late), db (note) };
    };
    const auto with = bloomGrowth (1.0f), without = bloomGrowth (0.0f);
    INFO ("the octave with FEEDBACK: " << with[0] << " dB at first, " << with[1] << " dB after 2 s (the note " << with[2]
          << " dB); without: " << without[1] << " dB");
    CHECK (with[1] - with[0] > 10.0);        // it swells
    CHECK (with[1] > with[2] - 20.0);        // to something heard
    CHECK (without[1] < without[2] - 60.0);  // and without FEEDBACK there's no octave at all
}

TEST_CASE ("INFERNAL: silence in is silence out, the chains resting at exactly zero", "[infernal]")
{
    auto s = only (0);
    s.fuzz = s.crush = s.amp = s.noise = 1.0f;
    s.feedback = 1.0f;
    s.drive = 1.0f;
    const auto out = run (s, 4.0, [] (double t) { return t < 1.0 ? saw (t) : 0.0f; }, { 45.0f, 52.0f });
    float tail = 0.0f;
    for (auto i = (size_t) (3.5 * rate); i < out.size(); ++i)
        tail = std::max (tail, std::abs (out[i]));
    CHECK (tail == 0.0f);
}

TEST_CASE ("CPU: INFERNAL, every chain on", "[infernal][cpu]")
{
    auto s = only (0);
    s.fuzz = s.crush = s.amp = s.noise = 1.0f;
    s.feedback = 1.0f;
    const auto started = std::chrono::steady_clock::now();
    run (s, 20.0, [] (double t) { return sine (110.0, t, 0.4f); }, { 45.0f, 52.0f, 57.0f });
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    const auto load = seconds / 20.0 * 100.0;
    WARN ("INFERNAL, every chain on, three notes, stereo: " << load << "% of a core (the test's own input included)");
    CHECK (load < 5.0);
}

