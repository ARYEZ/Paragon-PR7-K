// HAZE (0.66.0): streetlights passing. Checked by what it does to the sound: off and at DEPTH 0 untouched; on a pass
// (a bar at 120 BPM: 2 s) the level is full as it passes and about -13.5 dB at the edges at full DEPTH; it leans to
// the side it's on, left to right on the first pass and back on the next; the pitch rises as it comes and falls as
// it goes, by about DOPPLER's cents, and not at all at DOPPLER 0; the far edges are darker; it follows the song's
// position; the sides stay apart; the whoosh stops in silence; CPU.
#include "SynthTestUtils.h"

#include "dsp/effects/Haze.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Haze::Settings;

    template <typename Input>
    std::array<std::vector<float>, 2> run (double seconds, Input input, Settings s, double startPpq = -1.0)
    {
        effects::Haze fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (7);
        std::array<std::vector<float>, 2> out;
        std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) std::ceil (seconds * sampleRate / blockSize); ++b)   // at least `seconds`: a measurement up to the end stays inside
        {
            for (size_t i = 0; i < (size_t) blockSize; ++i)
            {
                const auto [a, c] = input (t + (long long) i);
                l[i] = a;
                r[i] = c;
            }
            const auto ppq = startPpq < 0.0 ? -1.0 : startPpq + (double) t / sampleRate * 2.0;   // 120 BPM
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) blockSize), s, 120.0, ppq);
            out[0].insert (out[0].end(), l.begin(), l.end());
            out[1].insert (out[1].end(), r.begin(), r.end());
            t += blockSize;
        }
        return out;
    }

    Settings passing (float depth, float doppler = 0.0f)
    {
        Settings s;
        s.on = true;
        s.division = 1;   // a pass a bar: 2 s at 120 BPM
        s.depth = depth;
        s.doppler = doppler;
        s.width = 1.0f;
        s.tone = 1.0f;
        s.whoosh = 0.0f;
        return s;
    }

    std::pair<float, float> sine440 (long long i)
    {
        const auto v = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (double) i / sampleRate);
        return { v, v };
    }

    size_t at (double seconds) { return (size_t) (seconds * sampleRate); }
    double both (const std::array<std::vector<float>, 2>& out, double from, double to)
    {
        return db (std::sqrt (0.5 * (std::pow (rms (out[0], at (from), at (to)), 2.0) + std::pow (rms (out[1], at (from), at (to)), 2.0))));
    }
}

TEST_CASE ("HAZE: off and at DEPTH 0 the sound passes exactly as it was", "[haze]")
{
    const auto input = [] (long long i) { const auto v = 0.3f * (float) std::sin (0.017 * (double) i); return std::pair<float, float> { v, -0.6f * v }; };
    std::vector<float> l, r;
    for (long long i = 0; i < (long long) (2.0 * sampleRate / blockSize) * blockSize; ++i)
    {
        const auto [a, b] = input (i);
        l.push_back (a);
        r.push_back (b);
    }
    auto zero = passing (0.0f, 1.0f);
    zero.whoosh = 1.0f;
    const auto z = run (2.0, input, zero);
    CHECK (z[0] == l);
    CHECK (z[1] == r);
    auto off = passing (1.0f, 1.0f);
    off.on = false;
    CHECK (run (2.0, input, off)[0] == l);
}

TEST_CASE ("HAZE: loudest as it passes, about 13.5 dB down at the edges, a whole pass as loud as the sound; leaning to its side", "[haze]")
{
    const auto out = run (4.0, sine440, passing (1.0f));
    const auto passingDb = both (out, 0.9, 1.1), edgeDb = both (out, 1.97, 2.03), input = db (0.4 / std::sqrt (2.0));
    const auto wholeDb = both (out, 0.5, 2.5);   // a whole pass, from the middle of one to the middle of the next
    INFO ("passing " << passingDb << " dB, at the edge " << edgeDb << " dB, a whole pass " << wholeDb << " dB, the sound " << input << " dB");
    CHECK (passingDb > input);   // the make-up: above the sound as it passes
    CHECK (edgeDb - passingDb < -11.0);
    CHECK (edgeDb - passingDb > -16.0);
    CHECK (std::abs (wholeDb - input) < 1.0);
    // First pass left to right: early it's on the left, late on the right; the second pass the other way.
    CHECK (rms (out[0], at (0.3), at (0.5)) > rms (out[1], at (0.3), at (0.5)) * 1.5);
    CHECK (rms (out[1], at (1.5), at (1.7)) > rms (out[0], at (1.5), at (1.7)) * 1.5);
    CHECK (rms (out[1], at (2.3), at (2.5)) > rms (out[0], at (2.3), at (2.5)) * 1.5);
}

TEST_CASE ("HAZE: the pitch rises as it comes and falls as it goes, by about DOPPLER's cents", "[haze]")
{
    const auto cents = [] (double hz) { return 1200.0 * std::log2 (hz / 440.0); };
    const auto out = run (2.0, sine440, passing (1.0f, 1.0f));   // up to 40 cents
    const auto coming = cents (frequencyOf (out[0], at (0.35), at (0.6))), going = cents (frequencyOf (out[1], at (1.4), at (1.65)));
    INFO ("coming " << coming << " cents, going " << going << " cents");
    CHECK (coming > 8.0);
    CHECK (going < -8.0);
    CHECK (coming < 45.0);
    CHECK (going > -45.0);
    const auto flat = run (2.0, sine440, passing (1.0f, 0.0f));
    CHECK (std::abs (cents (frequencyOf (flat[0], at (0.35), at (0.6)))) < 0.5);
}

TEST_CASE ("HAZE: further away is darker as well as quieter", "[haze]")
{
    const auto bright = [] (long long i) { const auto v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 8000.0 * (double) i / sampleRate); return std::pair<float, float> { v, v }; };
    auto s = passing (1.0f);
    s.tone = 0.0f;   // far cutoff 1.5 kHz
    const auto dark = run (4.0, bright, s);
    s.tone = 1.0f;   // 12 kHz
    const auto open = run (4.0, bright, s);
    const auto darkEdge = both (dark, 1.97, 2.03), openEdge = both (open, 1.97, 2.03);
    INFO ("8 kHz at the edge: TONE 0 " << darkEdge << " dB, TONE 1 " << openEdge << " dB");
    CHECK (darkEdge < openEdge - 6.0);
}

TEST_CASE ("HAZE: it follows the song's position", "[haze]")
{
    // Started at beat 1.8 of a bar-long pass, it's passing (halfway, beat 2) a tenth of a second in: loudest there.
    const auto out = run (0.3, sine440, passing (1.0f), 1.8);
    CHECK (std::abs (both (out, 0.09, 0.11) - (db (0.4 / std::sqrt (2.0)) + db (effects::Haze::makeUpFor (1.0f)))) < 0.5);
}

TEST_CASE ("HAZE: the sides stay apart; the whoosh stops in silence", "[haze]")
{
    const auto leftOnly = run (3.0, [] (long long i) { return std::pair<float, float> { sine440 (i).first, 0.0f }; }, passing (1.0f, 1.0f));
    CHECK (*std::max_element (leftOnly[1].begin(), leftOnly[1].end()) == 0.0f);
    CHECK (*std::min_element (leftOnly[1].begin(), leftOnly[1].end()) == 0.0f);
    auto s = passing (1.0f);
    s.whoosh = 1.0f;
    const auto out = run (8.0, [] (long long i) { return i < (long long) (2.0 * sampleRate) ? sine440 (i) : std::pair<float, float> { 0.0f, 0.0f }; }, s);
    float tail = 0.0f;
    for (auto i = at (7.0); i < out[0].size(); ++i)
        tail = std::max (tail, std::max (std::abs (out[0][i]), std::abs (out[1][i])));
    CHECK (tail == 0.0f);
}

TEST_CASE ("CPU: HAZE, passing with the wind", "[haze][cpu]")
{
    auto s = passing (0.8f, 0.6f);
    s.whoosh = 0.5f;
    s.tone = 0.3f;
    const auto started = std::chrono::steady_clock::now();
    run (20.0, sine440, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("HAZE, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input included)");
    CHECK (seconds / 20.0 * 100.0 < 5.0);
}
