// LOCKSTEP (0.81.0): the sound forced into lockstep. Off and AMOUNT 0 leave the sound untouched bit for bit; drilled,
// the sound is cut into steps where the song's beat says (open for GATE of each, the off beats lower by ACCENT), squeezed
// flat (harmonics up, level kept), a boot stamped on each step while there's music; back to 0 it rests untouched again.
#include "SynthTestUtils.h"

#include "dsp/effects/Lockstep.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>

using Catch::Matchers::WithinAbs;

namespace
{
    using Settings = effects::Lockstep::Settings;
    constexpr double bpm = 120.0;   // an eighth note: 0.25 s

    // `startPpq` < 0: the song stopped (its own count).
    std::array<std::vector<float>, 2> runLockstep (std::vector<float> left, std::vector<float> right, Settings s, double startPpq = 0.0,
                                                   int changeAt = -1, Settings changed = {}, int* stamps = nullptr)
    {
        effects::Lockstep fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (1);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            juce::dsp::AudioBlock<float> block (channels, 2, n);
            const auto ppq = startPpq < 0.0 ? -1.0 : startPpq + (double) start * bpm / 60.0 / sampleRate;
            fx.process (block, changeAt >= 0 && (int) start >= changeAt ? changed : s, bpm, ppq);
        }
        if (stamps != nullptr)
            *stamps = fx.getStampsStarted();
        return { left, right };
    }

    std::vector<float> sine (double hz, double seconds, float amplitude = 0.3f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate);
        return x;
    }

    std::vector<float> noise (double seconds, int seed, float amplitude = 0.2f)
    {
        juce::Random random (seed);
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (auto& v : x)
            v = amplitude * (2.0f * random.nextFloat() - 1.0f);
        return x;
    }

    std::vector<float> sawChord (double seconds)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (auto hz : { 220.0, 277.18, 329.63 })
        {
            double phase = 0.0;
            for (auto& v : x)
            {
                v += 0.15f * (float) (2.0 * phase - 1.0);
                phase += hz / sampleRate;
                if (phase >= 1.0)
                    phase -= 1.0;
            }
        }
        return x;
    }

    double power (const std::vector<float>& x, size_t from, size_t to)
    {
        double sum = 0.0;
        for (auto i = from; i < juce::jmin (to, x.size()); ++i)
            sum += (double) x[i] * x[i];
        return sum / (double) juce::jmax ((size_t) 1, juce::jmin (to, x.size()) - from);
    }

    double powerDb (double p) { return 10.0 * std::log10 (juce::jmax (1.0e-20, p)); }

    // The strength of `hz` over [from, to) (one DFT bin).
    double strength (const std::vector<float>& x, double hz, size_t from, size_t to)
    {
        double re = 0.0, im = 0.0;
        for (auto i = from; i < to; ++i)
        {
            const auto w = juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate;
            re += x[i] * std::cos (w);
            im += x[i] * std::sin (w);
        }
        return 2.0 * std::sqrt (re * re + im * im) / (double) (to - from);
    }

    Settings drilled (float amount = 1.0f)
    {
        Settings s;
        s.on = true;
        s.amount = amount;
        s.division = 1;   // eighths
        s.gate = (0.5f - 0.15f) / 0.75f;   // open for half of each step
        s.accent = 0.0f;
        s.squeeze = 0.0f;
        s.stamp = 0.0f;
        return s;
    }

    size_t at (double seconds) { return (size_t) (seconds * sampleRate); }
}

TEST_CASE ("LOCKSTEP: off and AMOUNT 0 leave the sound untouched bit for bit", "[lockstep]")
{
    const auto left = noise (1.0, 1), right = noise (1.0, 2);
    auto off = drilled();
    off.on = false;
    const auto a = runLockstep (left, right, off);
    CHECK (a[0] == left);
    CHECK (a[1] == right);
    const auto b = runLockstep (left, right, drilled (0.0f));
    CHECK (b[0] == left);
    CHECK (b[1] == right);
    const auto c = runLockstep (left, right, drilled (0.0005f));   // under 0.001 counts as 0 (the matrix eases down)
    CHECK (c[0] == left);
}

TEST_CASE ("LOCKSTEP: drilled, the sound is cut into the song's steps, open for GATE of each", "[lockstep]")
{
    // 120 BPM eighths: a step every 0.25 s, open its first half. From the song's start the gate opens at 0, 0.25, 0.5...
    const auto tone = sine (440.0, 3.0);
    const auto out = runLockstep (tone, tone, drilled());
    const auto openDb = powerDb (power (out[0], at (2.005), at (2.12)));
    const auto closedDb = powerDb (power (out[0], at (2.14), at (2.245)));
    const auto inDb = powerDb (power (tone, at (2.0), at (2.25)));
    INFO ("open " << openDb << " dB, closed " << closedDb << " dB, the tone " << inDb << " dB");
    CHECK (closedDb < openDb - 50.0);
    // Its loss made up from the pattern: half the time open, so +3 dB while open (the whole about as loud).
    CHECK_THAT (openDb - inDb, WithinAbs (3.0, 0.5));

    // Started half a step into the song (ppq 0.25): the gate is shut at once, opening at 0.125 s.
    const auto late = runLockstep (tone, tone, drilled(), 0.25);
    CHECK (powerDb (power (late[0], at (2.01), at (2.12))) < powerDb (power (late[0], at (2.13), at (2.245))) - 50.0);

    // GATE wider, the open part longer.
    auto wide = drilled();
    wide.gate = 1.0f;   // 90%
    const auto w = runLockstep (tone, tone, wide);
    CHECK (powerDb (power (w[0], at (2.15), at (2.2))) > powerDb (power (w[0], at (2.235), at (2.245))) + 40.0);
    CHECK (powerDb (power (w[0], at (2.15), at (2.2))) > inDb - 1.0);
}

TEST_CASE ("LOCKSTEP: the steps off the beat sit lower by ACCENT", "[lockstep]")
{
    const auto tone = sine (440.0, 3.0);
    auto s = drilled();
    s.accent = 1.0f;   // off beats at 0.4: about -8 dB
    const auto out = runLockstep (tone, tone, s);
    const auto onBeat = powerDb (power (out[0], at (2.01), at (2.11)));    // beat 4 (a whole beat)
    const auto offBeat = powerDb (power (out[0], at (2.26), at (2.36)));   // the eighth after it
    INFO ("on " << onBeat << " dB, off " << offBeat << " dB");
    CHECK_THAT (offBeat - onBeat, WithinAbs (20.0 * std::log10 (0.4), 0.3));
}

TEST_CASE ("LOCKSTEP: SQUEEZE flattens the sound (harmonics up) and keeps its level", "[lockstep]")
{
    const auto tone = sine (220.0, 3.0, 0.4f);
    auto s = drilled();
    s.gate = 1.0f;
    s.squeeze = 1.0f;
    auto plain = s;
    plain.squeeze = 0.0f;
    const auto flat = runLockstep (tone, tone, s), clean = runLockstep (tone, tone, plain);
    const auto from = at (2.0), to = at (2.1);
    const auto third = strength (flat[0], 660.0, from, to) / strength (flat[0], 220.0, from, to);
    const auto cleanThird = strength (clean[0], 660.0, from, to) / strength (clean[0], 220.0, from, to);
    INFO ("3rd harmonic against the fundamental: squeezed " << third << ", clean " << cleanThird);
    CHECK (third > 0.1);
    CHECK (cleanThird < 0.01);
    CHECK_THAT (powerDb (power (flat[0], from, to)) - powerDb (power (clean[0], from, to)), WithinAbs (0.0, 1.0));
}

TEST_CASE ("LOCKSTEP: the whole stays about as loud", "[lockstep]")
{
    const auto music = sawChord (6.0);
    const auto from = at (2.0), to = at (6.0);
    const auto dryDb = powerDb (power (music, from, to));
    for (const auto amount : { 0.5f, 1.0f })
    {
        auto s = drilled (amount);
        s.squeeze = 1.0f;
        s.accent = 0.5f;
        const auto out = runLockstep (music, music, s);
        const auto outDb = powerDb (power (out[0], from, to));
        INFO ("AMOUNT " << amount << ": " << outDb - dryDb << " dB");
        CHECK (std::abs (outDb - dryDb) < 2.0);
    }
}

TEST_CASE ("LOCKSTEP: a boot on every step while there's music, none in silence", "[lockstep]")
{
    auto s = drilled();
    s.stamp = 1.0f;
    const auto music = sawChord (4.0);
    int count = 0;
    const auto out = runLockstep (music, music, s, 0.0, -1, {}, &count);
    INFO ("stamps in 4 s of eighths at 120 BPM: " << count);
    CHECK (count >= 14);   // one a step after the first (15)
    CHECK (count <= 16);
    // The boot lands on the step: more low thud (70 Hz) just after a step starts than just before the next.
    CHECK (strength (out[0], 70.0, at (2.0), at (2.04)) > 3.0 * strength (out[0], 70.0, at (2.2), at (2.24)));

    const std::vector<float> silence ((size_t) (3.0 * sampleRate), 0.0f);
    int none = -1;
    const auto quiet = runLockstep (silence, silence, s, 0.0, -1, {}, &none);
    CHECK (none == 0);
    CHECK (std::all_of (quiet[0].begin(), quiet[0].end(), [] (float v) { return v == 0.0f; }));
}

TEST_CASE ("LOCKSTEP: with the song stopped it keeps its own count", "[lockstep]")
{
    const auto tone = sine (440.0, 3.0);
    const auto out = runLockstep (tone, tone, drilled(), -1.0);
    CHECK (powerDb (power (out[0], at (2.14), at (2.245))) < powerDb (power (out[0], at (2.005), at (2.12))) - 50.0);
}

TEST_CASE ("LOCKSTEP: back to AMOUNT 0, it rests untouched again", "[lockstep]")
{
    const auto left = noise (5.0, 6), right = noise (5.0, 7);
    auto s = drilled();
    s.squeeze = 1.0f;
    s.stamp = 1.0f;
    const auto out = runLockstep (left, right, s, 0.0, (int) (2.0 * sampleRate), drilled (0.0f));
    const auto late = at (4.0);
    CHECK (std::equal (out[0].begin() + (long) late, out[0].end(), left.begin() + (long) late));
    CHECK (std::equal (out[1].begin() + (long) late, out[1].end(), right.begin() + (long) late));
}

TEST_CASE ("LOCKSTEP: CPU", "[lockstep][cpu]")
{
    const auto music = sawChord (10.0);
    auto s = drilled();
    s.squeeze = 1.0f;
    s.stamp = 1.0f;
    s.division = 2;
    const auto t0 = std::chrono::steady_clock::now();
    runLockstep (music, music, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const auto share = 100.0 * seconds / 10.0;
    WARN ("LOCKSTEP at full: " << share << "% of a core");
    CHECK (share < 10.0);
}
