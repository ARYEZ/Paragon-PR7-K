// LEGION (0.76.0): one player to a section. SIZE 0 and off leave the sound untouched bit for bit; each player is the
// sound a little late, placed across the stage (SPREAD), drifting in and out of tune (DETUNE); a full section is
// bigger, not louder; turning it up doesn't click; silence stays silent.
#include "SynthTestUtils.h"

#include "dsp/effects/Legion.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>

using Catch::Matchers::WithinAbs;

namespace
{
    using Settings = effects::Legion::Settings;

    // Runs `left` / `right` through LEGION in blocks of 256 (changes `s` at `changeAt` if given).
    std::array<std::vector<float>, 2> runLegion (std::vector<float> left, std::vector<float> right, Settings s,
                                                 int changeAt = -1, Settings changed = {})
    {
        effects::Legion legion;
        legion.prepare (sampleRate, blockSize);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            juce::dsp::AudioBlock<float> block (channels, 2, n);
            legion.process (block, changeAt >= 0 && (int) start >= changeAt ? changed : s);
        }
        return { left, right };
    }

    std::vector<float> saw (double hz, double seconds, float amplitude = 0.3f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        double phase = 0.0;
        for (auto& v : x)
        {
            v = amplitude * (float) (2.0 * phase - 1.0);
            phase += hz / sampleRate;
            phase -= std::floor (phase);
        }
        return x;
    }

    std::vector<float> chord (double seconds)
    {
        auto x = saw (110.0, seconds);
        for (const auto hz : { 164.81, 220.0, 277.18 })
        {
            const auto y = saw (hz, seconds);
            for (size_t i = 0; i < x.size(); ++i)
                x[i] += y[i];
        }
        return x;
    }

    std::vector<float> noise (double seconds, int seed)
    {
        juce::Random random (seed);
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (auto& v : x)
            v = 0.3f * (2.0f * random.nextFloat() - 1.0f);
        return x;
    }

    Settings with (float size, float spread = 0.8f, float loose = 0.4f, float detune = 0.4f)
    {
        Settings s;
        s.on = true;
        s.size = size;
        s.spread = spread;
        s.loose = loose;
        s.detune = detune;
        return s;
    }

    double stereoRms (const std::array<std::vector<float>, 2>& x, size_t from, size_t to)
    {
        return std::sqrt (0.5 * (std::pow (rms (x[0], from, to), 2.0) + std::pow (rms (x[1], from, to), 2.0)));
    }
}

TEST_CASE ("LEGION: off, and on at SIZE 0, leave the sound untouched bit for bit", "[legion]")
{
    const auto left = chord (1.0), right = noise (1.0, 3);
    Settings off = with (1.0f);
    off.on = false;
    const auto a = runLegion (left, right, off);
    CHECK (a[0] == left);
    CHECK (a[1] == right);
    const auto b = runLegion (left, right, with (0.0f));
    CHECK (b[0] == left);
    CHECK (b[1] == right);
    const auto c = runLegion (left, right, with (0.0005f));   // under 0.001 counts as 0
    CHECK (c[0] == left);
}

TEST_CASE ("LEGION: each player is the sound a little late: an impulse comes back once per player, at its lateness", "[legion]")
{
    // DETUNE 0 (no sway): a click in the middle of both sides, SIZE 1, LOOSE 1 (the latest 30 ms behind).
    // (The click comes at 0.5 s, once SIZE has settled: every player in.) Quiet noise under it keeps the level keeper steady.
    auto x = noise (0.7, 9);
    for (auto& v : x)
        v *= 0.001f;
    const auto click = (int) (0.5 * sampleRate);
    x[(size_t) click] = 1.0f;
    const auto out = runLegion (x, x, with (1.0f, 0.0f, 1.0f, 0.0f));   // SPREAD 0: every player in the middle
    // The players' lateness: 2 ms + 28 ms x their shares (Legion.cpp's own table).
    constexpr float shares[] = { 0.20f, 0.85f, 0.45f, 0.65f, 0.30f, 1.00f, 0.55f, 0.75f };
    for (const auto share : shares)
    {
        const auto at = click + (int) std::lround ((2.0 + 28.0 * share) * 0.001 * sampleRate);
        auto peak = 0.0f;
        for (int i = at - 2; i <= at + 2; ++i)
            peak = std::max (peak, std::abs (out[0][(size_t) i]));
        INFO ("player at " << (2.0 + 28.0 * share) << " ms: " << peak);
        CHECK (peak > 0.1f);
    }
    // Nothing between the click and the first player, nothing after the last.
    for (int i = click + 10; i < click + (int) (0.007 * sampleRate); ++i)
        CHECK (std::abs (out[0][(size_t) i]) < 0.02f);
    for (auto i = (size_t) (click + 0.032 * sampleRate); i < out[0].size(); ++i)
        CHECK (std::abs (out[0][i]) < 0.02f);
}

TEST_CASE ("LEGION: a full section is bigger, not louder (within about 1 dB on a saw chord and on noise)", "[legion]")
{
    for (const auto which : { 0, 1 })
    {
        const auto left = which == 0 ? chord (3.0) : noise (3.0, 5);
        const auto right = which == 0 ? left : noise (3.0, 6);
        const auto dry = stereoRms ({ left, right }, (size_t) sampleRate, left.size());
        for (const auto size : { 0.5f, 1.0f })
        {
            const auto out = runLegion (left, right, with (size));
            const auto change = db (stereoRms (out, (size_t) sampleRate, left.size()) / dry);
            INFO ((which == 0 ? "saw chord" : "noise") << " at SIZE " << size << ": " << change << " dB");
            CHECK (std::abs (change) < 1.5);
        }
    }
}

TEST_CASE ("LEGION: SPREAD places the players across the stage; at 0 a centred sound stays centred", "[legion]")
{
    const auto mono = chord (2.0);
    const auto sideToMid = [] (const std::array<std::vector<float>, 2>& x)
    {
        double mid = 0.0, side = 0.0;
        for (auto i = (size_t) sampleRate; i < x[0].size(); ++i)
        {
            mid += std::pow (0.5 * (x[0][i] + x[1][i]), 2.0);
            side += std::pow (0.5 * (x[0][i] - x[1][i]), 2.0);
        }
        return side / std::max (1.0e-12, mid);
    };
    const auto narrow = runLegion (mono, mono, with (1.0f, 0.0f));
    const auto wide = runLegion (mono, mono, with (1.0f, 1.0f));
    INFO ("side / mid: SPREAD 0 " << sideToMid (narrow) << ", SPREAD 1 " << sideToMid (wide));
    CHECK (sideToMid (narrow) < 1.0e-8);
    CHECK (sideToMid (wide) > 0.1);
}

TEST_CASE ("LEGION: DETUNE makes the players drift in and out of tune (a held note beats)", "[legion]")
{
    // A 440 Hz sine, SIZE 1: with DETUNE the players drift, so the sum's level swells and fades (beating); without,
    // it holds steady.
    std::vector<float> sine ((size_t) (6.0 * sampleRate));
    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (double) i / sampleRate);
    const auto swing = [&] (float detune)
    {
        const auto out = runLegion (sine, sine, with (1.0f, 0.0f, 0.4f, detune));
        double lo = 1.0e9, hi = 0.0;
        for (auto start = (size_t) sampleRate; start + 2400 < out[0].size(); start += 2400)   // 50 ms windows
        {
            const auto r = rms (out[0], start, start + 2400);
            lo = std::min (lo, r);
            hi = std::max (hi, r);
        }
        return db (hi / lo);
    };
    const auto still = swing (0.0f), drifting = swing (1.0f);
    INFO ("level swing over 5 s: DETUNE 0 " << still << " dB, DETUNE 1 " << drifting << " dB");
    CHECK (still < 0.5);
    CHECK (drifting > 3.0);
}

TEST_CASE ("LEGION: turning SIZE up at once doesn't click; silence stays silent", "[legion]")
{
    std::vector<float> sine ((size_t) (1.0 * sampleRate));
    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / sampleRate);
    const auto out = runLegion (sine, sine, with (0.0f), (int) (0.5 * sampleRate), with (1.0f));
    // While the section joins (from 0.5 s), no 10 ms stretch is more than 1.5 dB over the settled section (its last
    // 0.2 s) or the sound before it: no swell, no click.
    const auto settled = rms (out[0], (size_t) (0.8 * sampleRate), out[0].size());
    const auto before = rms (out[0], (size_t) (0.3 * sampleRate), (size_t) (0.5 * sampleRate));
    auto worst = -100.0;
    for (auto start = (size_t) (0.5 * sampleRate); start < (size_t) (0.8 * sampleRate); start += 480)
        worst = std::max (worst, db (rms (out[0], start, start + 480) / std::max (settled, before)));
    INFO ("loudest 10 ms while joining: " << worst << " dB over the louder of before and settled");
    CHECK (worst < 1.5);

    std::vector<float> silence ((size_t) sampleRate, 0.0f);
    const auto quiet = runLegion (silence, silence, with (1.0f));
    CHECK (rms (quiet[0], 0, quiet[0].size()) == 0.0);
}

TEST_CASE ("LEGION: CPU", "[legion][cpu]")
{
    const auto left = chord (10.0);
    const auto start = std::chrono::steady_clock::now();
    const auto out = runLegion (left, left, with (1.0f));
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const auto share = 100.0 * seconds / 10.0;
    WARN ("LEGION, nine players: " << share << "% of one core");
    CHECK (share < 10.0);
    CHECK (out[0].size() == left.size());
}
