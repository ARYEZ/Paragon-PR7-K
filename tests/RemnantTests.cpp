// REMNANT (0.64.0): dreams within dreams. Checked by what it does to the sound: off and MIX 0 leave it exactly as it
// was; on, the sound itself still passes untouched and each layer is there beneath it at its own pitch (an octave
// down, two, three; a fifth, two fifths), as many as LAYERS says; TAPER makes the deeper ones quieter; the sides stay
// apart; the layers die away; a MIX eased down to almost nothing rests; CPU.
#include "SynthTestUtils.h"

#include "dsp/effects/Remnant.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Remnant::Settings;

    template <typename Input>
    std::array<std::vector<float>, 2> run (double seconds, Input input, Settings s)
    {
        effects::Remnant fx;
        fx.prepare (sampleRate, blockSize);
        std::array<std::vector<float>, 2> out;
        std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
        {
            for (size_t i = 0; i < (size_t) blockSize; ++i)
            {
                const auto [a, c] = input (t + (long long) i);
                l[i] = a;
                r[i] = c;
            }
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) blockSize), s);
            out[0].insert (out[0].end(), l.begin(), l.end());
            out[1].insert (out[1].end(), r.begin(), r.end());
            t += blockSize;
        }
        return out;
    }

    Settings layered (int layers, int step = 0, float taper = 0.0f)
    {
        Settings s;
        s.on = true;
        s.layers = layers - 1;
        s.step = step;
        s.taper = taper;
        s.tone = 1.0f;
        s.width = 0.0f;
        s.mix = 1.0f;
        return s;
    }

    std::pair<float, float> sine880 (long long i)
    {
        const auto v = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 880.0 * (double) i / sampleRate);
        return { v, v };
    }

    size_t at (double seconds) { return (size_t) (seconds * sampleRate); }
    double levelDb (const std::vector<float>& x, double hz) { return db (levelAt (x, hz, at (2.0), at (6.0))); }
}

TEST_CASE ("REMNANT: off and MIX 0 leave the sound exactly as it was", "[remnant]")
{
    const auto input = [] (long long i) { const auto v = 0.3f * (float) std::sin (0.031 * (double) i); return std::pair<float, float> { v, -0.5f * v }; };
    std::vector<float> l, r;
    for (long long i = 0; i < (long long) (2.0 * sampleRate / blockSize) * blockSize; ++i)
    {
        const auto [a, b] = input (i);
        l.push_back (a);
        r.push_back (b);
    }
    auto zero = layered (3);
    zero.mix = 0.0f;
    const auto z = run (2.0, input, zero);
    CHECK (z[0] == l);
    CHECK (z[1] == r);
    auto off = layered (3);
    off.on = false;
    CHECK (run (2.0, input, off)[0] == l);
}

TEST_CASE ("REMNANT: your sound passes untouched; each layer is beneath it at its own pitch, as many as LAYERS says", "[remnant]")
{
    const auto input = db (0.4);
    for (const auto layers : { 1, 2, 3 })
    {
        const auto out = run (8.0, sine880, layered (layers));
        INFO (layers << " layers: 880 Hz " << levelDb (out[0], 880.0) << ", 440 " << levelDb (out[0], 440.0) << ", 220 "
                      << levelDb (out[0], 220.0) << ", 110 " << levelDb (out[0], 110.0) << " dB");
        CHECK (std::abs (levelDb (out[0], 880.0) - input) < 0.5);   // the sound itself, as it was
        for (int k = 1; k <= 3; ++k)
        {
            const auto hz = 880.0 / std::pow (2.0, k);
            if (k <= layers)
                CHECK (levelDb (out[0], hz) > input - 15.0);
            else
                CHECK (levelDb (out[0], hz) < input - 50.0);
        }
    }
    // A fifth: two thirds the speed each layer.
    const auto fifths = run (8.0, sine880, layered (2, 1));
    INFO ("fifths: 586.7 Hz " << levelDb (fifths[0], 880.0 * 2.0 / 3.0) << ", 391.1 " << levelDb (fifths[0], 880.0 * 4.0 / 9.0) << " dB");
    CHECK (levelDb (fifths[0], 880.0 * 2.0 / 3.0) > input - 15.0);
    CHECK (levelDb (fifths[0], 880.0 * 4.0 / 9.0) > input - 18.0);
}

TEST_CASE ("REMNANT: TAPER makes each deeper layer quieter", "[remnant]")
{
    const auto flat = run (8.0, sine880, layered (3, 0, 0.0f));
    const auto tapered = run (8.0, sine880, layered (3, 0, 1.0f));   // 12 dB a layer
    for (int k = 1; k <= 3; ++k)
    {
        const auto hz = 880.0 / std::pow (2.0, k);
        const auto drop = levelDb (flat[0], hz) - levelDb (tapered[0], hz);
        INFO ("layer " << k << ": " << drop << " dB quieter with TAPER at full");
        CHECK (std::abs (drop - 12.0 * (k - 1)) < 1.0);
    }
}

TEST_CASE ("REMNANT: the sides stay apart", "[remnant]")
{
    auto s = layered (3);
    s.width = 1.0f;
    const auto out = run (8.0, [] (long long i) { return std::pair<float, float> { sine880 (i).first, 0.0f }; }, s);
    CHECK (*std::max_element (out[1].begin(), out[1].end()) == 0.0f);
    CHECK (*std::min_element (out[1].begin(), out[1].end()) == 0.0f);
    CHECK (levelDb (out[0], 440.0) > db (0.4) - 15.0);
}

TEST_CASE ("REMNANT: the layers die away after the sound stops", "[remnant]")
{
    auto s = layered (3);
    s.length = 1.0f;   // 4 s: the longest
    const auto out = run (16.0, [] (long long i) { return i < (long long) (1.0 * sampleRate) ? sine880 (i) : std::pair<float, float> { 0.0f, 0.0f }; }, s);
    float tail = 0.0f;
    for (auto i = at (14.0); i < out[0].size(); ++i)
        tail = std::max (tail, std::abs (out[0][i]));
    INFO ("14 s on: " << tail);
    CHECK (tail < 1.0e-6f);
}

TEST_CASE ("REMNANT: a MIX eased down to almost nothing rests, and the sound passes untouched again", "[remnant]")
{
    effects::Remnant fx;
    fx.prepare (sampleRate, blockSize);
    std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
    float* ptrs[] = { l.data(), r.data() };
    auto s = layered (2);
    long long t = 0;
    auto differs = 0.0f;
    for (int b = 0; b < (int) (4.0 * sampleRate / blockSize); ++b)
    {
        s.mix = t < (long long) (2.0 * sampleRate) ? 1.0f : 0.0004f;   // as the matrix leaves it: never quite 0
        for (size_t i = 0; i < (size_t) blockSize; ++i)
            l[i] = r[i] = sine880 (t + (long long) i).first;
        fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) blockSize), s);
        if (t > (long long) (2.5 * sampleRate))
            for (size_t i = 0; i < (size_t) blockSize; ++i)
                differs = std::max (differs, std::abs (l[i] - sine880 (t + (long long) i).first));
        t += blockSize;
    }
    CHECK (fx.isResting());
    CHECK (differs == 0.0f);
}

TEST_CASE ("CPU: REMNANT, three layers", "[remnant][cpu]")
{
    const auto started = std::chrono::steady_clock::now();
    run (20.0, sine880, layered (3));
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("REMNANT, three layers, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input included)");
    CHECK (seconds / 20.0 * 100.0 < 6.0);
}
