// LIGHTSPEED (0.73.0): HYPERDRIVE's jump to lightspeed. Checked by what it does to the sound: off and at JUMP 0
// untouched bit for bit, and back to untouched once a jump has settled down; the streaks at the pitches RISE says
// (thirds of the way to its top), out to the sides by WIDTH; the sound ducked by a third at full JUMP; the whoosh
// following the music (silence stays silent) and sweeping higher as JUMP rises; the pitch gliding up, not stepping.
#include "SynthTestUtils.h"

#include "dsp/effects/LightSpeed.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

namespace
{
    using Settings = effects::LightSpeed::Settings;

    // Runs `seconds` of input (a function of the sample index giving left, right) through one LightSpeed, with the
    // settings a function of time too.
    template <typename Input, typename At>
    std::array<std::vector<float>, 2> run (effects::LightSpeed& fx, double seconds, Input input, At settingsAt)
    {
        std::array<std::vector<float>, 2> out;
        std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) std::ceil (seconds * sampleRate / blockSize); ++b)
        {
            for (size_t i = 0; i < (size_t) blockSize; ++i)
            {
                const auto [a, c] = input (t + (long long) i);
                l[i] = a;
                r[i] = c;
            }
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) blockSize), settingsAt ((double) t / sampleRate));
            out[0].insert (out[0].end(), l.begin(), l.end());
            out[1].insert (out[1].end(), r.begin(), r.end());
            t += blockSize;
        }
        return out;
    }

    // A pitch-shifted streak warbles a little (its two read heads cross every few tens of ms), spreading its energy a
    // few Hz either side: its level is the energy within 40 Hz of the pitch (1 Hz steps over a 1 s window).
    double bandLevel (const std::vector<float>& x, double hz, size_t from, size_t length)
    {
        double power = 0.0;
        for (auto f = hz - 40.0; f <= hz + 40.0; f += 1.0)
            power += std::pow (levelAt (x, f, from, length), 2.0);
        return std::sqrt (power);
    }

    std::pair<float, float> sine (long long t, double hz, float amplitude = 0.5f)
    {
        const auto v = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) t / sampleRate);
        return { v, v };
    }
}

TEST_CASE ("LIGHTSPEED: off and at JUMP 0 untouched; back to untouched once a jump settles", "[lightspeed]")
{
    effects::LightSpeed fx;
    fx.prepare (sampleRate, blockSize);
    juce::Random random (7);
    std::vector<float> noiseL (48000 * 8), noiseR (noiseL.size());
    for (size_t i = 0; i < noiseL.size(); ++i)
    {
        noiseL[i] = random.nextFloat() - 0.5f;
        noiseR[i] = random.nextFloat() - 0.5f;
    }
    const auto input = [&] (long long t) { return std::make_pair (noiseL[(size_t) t % noiseL.size()], noiseR[(size_t) t % noiseR.size()]); };
    const auto same = [&] (const std::array<std::vector<float>, 2>& out, size_t from, size_t to)
    {
        for (auto i = from; i < to; ++i)
            if (out[0][i] != noiseL[i % noiseL.size()] || out[1][i] != noiseR[i % noiseR.size()])
                return false;
        return true;
    };
    Settings s;
    s.on = false;
    s.jump = 1.0f;
    auto out = run (fx, 0.5, input, [&] (double) { return s; });
    CHECK (same (out, 0, out[0].size()));
    s.on = true;
    s.jump = 0.0f;
    out = run (fx, 0.5, input, [&] (double) { return s; });
    CHECK (same (out, 0, out[0].size()));
    CHECK (fx.isResting());

    // A jump up for a second, then down: changed while it's up, untouched again once it has settled (a few seconds).
    effects::LightSpeed jumper;
    jumper.prepare (sampleRate, blockSize);
    out = run (jumper, 8.0, input, [&] (double time) { auto x = s; x.jump = time < 1.0 ? 1.0f : 0.0f; return x; });
    CHECK_FALSE (same (out, 24000, 48000));
    CHECK (jumper.isResting());
    CHECK (same (out, (size_t) (7.0 * sampleRate), (size_t) (8.0 * sampleRate)));
}

TEST_CASE ("LIGHTSPEED: the streaks at RISE's pitches, out to the sides; the sound ducked by a third", "[lightspeed]")
{
    effects::LightSpeed fx;
    fx.prepare (sampleRate, blockSize);
    Settings s;
    s.on = true;
    s.jump = 1.0f;
    s.rise = 0.5f;     // the top streak +12 semitones: +4, +8, +12
    s.streak = 1.0f;
    s.whoosh = 0.0f;
    s.width = 1.0f;
    s.tone = 1.0f;
    const auto out = run (fx, 4.0, [] (long long t) { return sine (t, 440.0); }, [&] (double) { return s; });
    const auto from = (size_t) (3.0 * sampleRate), length = (size_t) sampleRate;
    std::array<double, 2> dry {}, first {}, second {}, third {};
    for (int c = 0; c < 2; ++c)
    {
        dry[(size_t) c] = levelAt (out[(size_t) c], 440.0, from, length);
        first[(size_t) c] = bandLevel (out[(size_t) c], 440.0 * std::exp2 (4.0 / 12.0), from, length);
        second[(size_t) c] = bandLevel (out[(size_t) c], 440.0 * std::exp2 (8.0 / 12.0), from, length);
        third[(size_t) c] = bandLevel (out[(size_t) c], 880.0, from, length);
    }
    CHECK_THAT (dry[0], WithinAbs (0.5 * 0.67, 0.01));   // ducked by a third
    // Each streak clearly there (well above what a plain shifter leaks), on its own side: the first and third left,
    // the second right.
    CHECK (first[0] > 0.08);
    CHECK (third[0] > 0.05);
    CHECK (second[1] > 0.06);
    CHECK (db (first[0] / first[1]) > 12.0);
    CHECK (db (second[1] / second[0]) > 12.0);
    CHECK (db (third[0] / third[1]) > 12.0);
    // RISE 0: no pitch at all (the streaks at the note itself, so nothing at the old pitches).
    effects::LightSpeed flat;
    flat.prepare (sampleRate, blockSize);
    auto level = s;
    level.rise = 0.0f;
    const auto plain = run (flat, 3.0, [] (long long t) { return sine (t, 440.0); }, [&] (double) { return level; });
    CHECK (levelAt (plain[0], 880.0, (size_t) (2.0 * sampleRate), length) < 0.005);
}

TEST_CASE ("LIGHTSPEED: the pitch glides up over a quarter second rather than stepping", "[lightspeed]")
{
    // The top streak's pitch a quarter second into a jump is part way (about 63% by a 0.25 s glide), not all the way.
    effects::LightSpeed fx;
    fx.prepare (sampleRate, blockSize);
    Settings s;
    s.on = true;
    s.rise = 1.0f;
    s.whoosh = 0.0f;
    std::vector<float> pitches;
    run (fx, 1.5, [] (long long t) { return sine (t, 220.0); },
         [&] (double time) { auto x = s; x.jump = time < 0.5 ? 0.0f : 1.0f; pitches.push_back (fx.getPitchSemitones()); return x; });
    const auto at = [&] (double seconds) { return pitches[(size_t) (seconds * sampleRate / blockSize)]; };
    CHECK (at (0.45) == 0.0f);
    CHECK_THAT (at (0.5 + 0.25), WithinAbs (24.0 * 0.63, 2.0));
    CHECK (at (1.45) > 23.0f);
}

TEST_CASE ("LIGHTSPEED: the whoosh follows the music and sweeps up with JUMP", "[lightspeed]")
{
    Settings s;
    s.on = true;
    s.streak = 0.0f;
    s.whoosh = 1.0f;
    // Silence in: silence out, at full jump.
    {
        effects::LightSpeed fx;
        fx.prepare (sampleRate, blockSize);
        s.jump = 1.0f;
        const auto out = run (fx, 2.0, [] (long long) { return std::make_pair (0.0f, 0.0f); }, [&] (double) { return s; });
        CHECK (rms (out[0], 0, out[0].size()) < 1.0e-6);
    }
    // With music: a whoosh, its energy higher at full JUMP than at a third of it (the band sweeps up). A low note
    // (55 Hz) so the music itself barely touches the bands measured.
    const auto bandPower = [&] (float jump, double lowHz, double highHz)
    {
        effects::LightSpeed fx;
        fx.prepare (sampleRate, blockSize);
        auto x = s;
        x.jump = jump;
        const auto out = run (fx, 3.0, [] (long long t) { return sine (t, 55.0); }, [&] (double) { return x; });
        double power = 0.0;
        for (auto hz = lowHz; hz < highHz; hz *= 1.06)
            power += std::pow (levelAt (out[0], hz, (size_t) (2.0 * sampleRate), (size_t) (sampleRate / 2)), 2.0);
        return power;
    };
    const auto lowAtThird = bandPower (0.33f, 250.0, 700.0), highAtThird = bandPower (0.33f, 3000.0, 9000.0);
    const auto lowAtFull = bandPower (1.0f, 250.0, 700.0), highAtFull = bandPower (1.0f, 3000.0, 9000.0);
    CHECK (highAtFull > 4.0 * lowAtFull);      // at full, the whoosh is high
    CHECK (lowAtThird > 4.0 * highAtThird);    // at a third, low
}
