// BLACKHOLE (0.62.0): the haze. Checked by what it does to the sound: off and MIX 0 leave it exactly as it was; after
// a note stops, the haze hangs at the note's pitch for as long as HOLD says (gone at the shortest); PULL sinks it;
// WIDTH 0 puts the same haze on both sides; it dies away to silence; CPU.
#include "SynthTestUtils.h"

#include "dsp/effects/BlackHole.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    template <typename Make>
    std::array<std::vector<float>, 2> run (effects::BlackHole::Settings s, double seconds, Make make)
    {
        effects::BlackHole fx;
        constexpr int block = 256;
        fx.prepare (sampleRate, block);
        fx.seedRandom (3);
        std::array<std::vector<float>, 2> out;
        std::vector<float> l ((size_t) block), r ((size_t) block);
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) (seconds * sampleRate / block); ++b)
        {
            for (size_t i = 0; i < (size_t) block; ++i)
            {
                const auto [a, c] = make (t + (long long) i);
                l[i] = a;
                r[i] = c;
            }
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) block), s);
            out[0].insert (out[0].end(), l.begin(), l.end());
            out[1].insert (out[1].end(), r.begin(), r.end());
            t += block;
        }
        return out;
    }

    effects::BlackHole::Settings haze (float hold, float pull = 0.0f)
    {
        effects::BlackHole::Settings s;
        s.on = true;
        s.mix = 1.0f;
        s.hold = hold;
        s.pull = pull;
        s.tone = 1.0f;
        s.size = 0.3f;
        return s;
    }

    // An A4 (440 Hz) for half a second, then silence.
    std::pair<float, float> note (long long i)
    {
        const auto v = i < 24000 ? 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (double) i / sampleRate) : 0.0f;
        return { v, v };
    }

    double rmsDb (const std::vector<float>& x, double from, double to)
    {
        return db (rms (x, (size_t) (from * sampleRate), std::min (x.size(), (size_t) (to * sampleRate))));
    }
}

TEST_CASE ("BLACKHOLE: off and MIX 0 leave the sound exactly as it was", "[blackhole]")
{
    const auto input = [] (long long i) { const auto v = 0.3f * (float) std::sin (0.03 * (double) i); return std::pair<float, float> { v, 0.5f * v }; };
    std::vector<float> expectedL, expectedR;
    for (long long i = 0; i < (long long) (sampleRate / 256) * 256; ++i)
    {
        const auto [a, b] = input (i);
        expectedL.push_back (a);
        expectedR.push_back (b);
    }
    auto off = haze (0.8f);
    off.on = false;
    CHECK (run (off, 1.0, input)[0] == expectedL);
    auto zero = haze (0.8f);
    zero.mix = 0.0f;
    const auto z = run (zero, 1.0, input);
    CHECK (z[0] == expectedL);
    CHECK (z[1] == expectedR);
}

TEST_CASE ("BLACKHOLE: after a note stops, the haze hangs at its pitch as long as HOLD says", "[blackhole]")
{
    const auto longHold = run (haze (1.0f), 4.0, note);
    const auto shortHold = run (haze (0.0f), 4.0, note);
    const auto playing = rmsDb (longHold[0], 0.1, 0.5);
    const auto longAfter = rmsDb (longHold[0], 2.5, 3.0), shortAfter = rmsDb (shortHold[0], 2.5, 3.0);
    INFO ("while playing " << playing << " dB; 2 s after the note: HOLD 20 s " << longAfter << " dB, HOLD 0.5 s " << shortAfter << " dB");
    CHECK (longAfter > playing - 30.0);   // still hanging
    CHECK (shortAfter < playing - 70.0);  // long gone
    // ...at the note's pitch: 440 Hz well over its neighbours.
    const auto from = (size_t) (2.5 * sampleRate), length = (size_t) (0.5 * sampleRate);
    const auto at = db (levelAt (longHold[0], 440.0, from, length)), off = db (levelAt (longHold[0], 400.0, from, length));
    INFO ("in the haze: 440 Hz " << at << " dB, 400 Hz " << off << " dB");
    CHECK (at > off + 15.0);
}

TEST_CASE ("BLACKHOLE: PULL sinks the haze in pitch, a little more each time round", "[blackhole]")
{
    // With PULL at full each pass reads 2 semitones low: two seconds on, the haze is well under 440 Hz.
    const auto sunk = run (haze (1.0f, 1.0f), 4.0, note);
    const auto from = (size_t) (2.5 * sampleRate), length = (size_t) (1.0 * sampleRate);
    double below = 0.0, at = 0.0;
    for (int k = 1; k <= 12; ++k)
        below += std::pow (levelAt (sunk[0], 440.0 * std::exp2 (-k / 12.0), from, length), 2.0);
    at = std::pow (levelAt (sunk[0], 440.0, from, length), 2.0);
    INFO ("2.5 s on, with PULL: the semitones under 440 Hz together " << 10.0 * std::log10 (below + 1e-30) << " dB, 440 Hz itself "
                                                                       << 10.0 * std::log10 (at + 1e-30) << " dB");
    CHECK (below > at * 4.0);
}

TEST_CASE ("BLACKHOLE: WIDTH 0 puts the same haze on both sides; at 1 the sides differ", "[blackhole]")
{
    auto narrow = haze (0.8f);
    narrow.width = 0.0f;
    const auto n = run (narrow, 2.0, note);
    double diff = 0.0;
    for (auto i = (size_t) (0.6 * sampleRate); i < n[0].size(); ++i)
        diff = std::max (diff, (double) std::abs (n[0][i] - n[1][i]));
    CHECK (diff < 1.0e-6);
    auto wide = haze (0.8f);
    wide.width = 1.0f;
    const auto w = run (wide, 2.0, note);
    double side = 0.0;
    for (auto i = (size_t) (0.6 * sampleRate); i < w[0].size(); ++i)
        side = std::max (side, (double) std::abs (w[0][i] - w[1][i]));
    CHECK (side > 0.01);
}

TEST_CASE ("BLACKHOLE: the haze dies away to silence", "[blackhole]")
{
    const auto out = run (haze (0.5f, 0.5f), 14.0, note);   // HOLD about 3 s
    float tail = 0.0f;
    for (auto i = (size_t) (13 * sampleRate); i < out[0].size(); ++i)
        tail = std::max (tail, std::max (std::abs (out[0][i]), std::abs (out[1][i])));
    INFO ("13 s on: " << tail);
    CHECK (tail < 1.0e-4f);
}

TEST_CASE ("CPU: BLACKHOLE, a long haze", "[blackhole][cpu]")
{
    const auto started = std::chrono::steady_clock::now();
    run (haze (0.9f, 0.3f), 20.0, [] (long long i) {
        const auto v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / sampleRate);
        return std::pair<float, float> { v, v };
    });
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("BLACKHOLE, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input included)");
    CHECK (seconds / 20.0 * 100.0 < 5.0);
}
