// CRACKLE (0.56.0): random clicks as a radiation counter's. Checked as a counter would be: the count over a minute,
// how randomly they fall (a Poisson process: counts per slice vary as much as their average; gaps exponential),
// CLUSTER's bursts, FOLLOW, TONE, WIDTH, LEVEL, off, CPU.
#include "dsp/effects/Crackle.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <numeric>
#include <vector>

namespace
{
    constexpr double rate = 48000.0;
    constexpr int block = 16;   // small blocks: each click's time known to a third of a millisecond

    struct Run
    {
        std::vector<float> left, right;
        std::vector<double> clickTimes;   // seconds
    };

    // `seconds` of `input` (a level, 0 silence; or a sine at 220 Hz of that peak) through CRACKLE.
    Run run (effects::Crackle::Settings s, double seconds, float input = 0.0f, std::uint32_t seed = 7)
    {
        effects::Crackle c;
        c.prepare (rate, block);
        c.seedRandom (seed);
        Run out;
        juce::AudioBuffer<float> buffer (2, block);
        long long t = 0, lastTotal = 0;
        const auto n = (int) (seconds * rate / block);
        for (int b = 0; b < n; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const auto x = input * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) (t + i) / rate);
                buffer.setSample (0, i, x);
                buffer.setSample (1, i, x);
            }
            c.process (juce::dsp::AudioBlock<float> (buffer), s);
            const auto total = c.getTotalClicks();
            for (auto k = lastTotal; k < total; ++k)
                out.clickTimes.push_back ((double) t / rate);
            lastTotal = total;
            out.left.insert (out.left.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
            out.right.insert (out.right.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + block);
            t += block;
        }
        return out;
    }

    // Counts per slice of `sliceSeconds`, and their variance over their mean (1 for a Poisson process).
    double dispersion (const std::vector<double>& times, double seconds, double sliceSeconds)
    {
        std::vector<int> counts ((size_t) (seconds / sliceSeconds), 0);
        for (const auto t : times)
            if (const auto k = (size_t) (t / sliceSeconds); k < counts.size())
                ++counts[k];
        const auto mean = std::accumulate (counts.begin(), counts.end(), 0.0) / (double) counts.size();
        double var = 0.0;
        for (const auto c : counts)
            var += (c - mean) * (c - mean);
        var /= (double) counts.size();
        return var / juce::jmax (1.0e-9, mean);
    }

    effects::Crackle::Settings steady (float clicksPerSecond)
    {
        effects::Crackle::Settings s;
        s.on = true;
        s.rate = clicksPerSecond;
        s.cluster = 0.0f;
        s.follow = 0.0f;   // clicks without a sound coming in
        return s;
    }
}

TEST_CASE ("CRACKLE: RATE clicks a second, falling at random as a counter's", "[crackle]")
{
    const auto r = run (steady (50.0f), 60.0);
    const auto count = (double) r.clickTimes.size();
    INFO ("clicks in a minute: " << count << " (3000 expected)");
    CHECK (std::abs (count - 3000.0) < 3000.0 * 0.06);

    // Counts per 200 ms vary as much as their average (Poisson), not evenly (a metronome gives ~0).
    const auto d = dispersion (r.clickTimes, 60.0, 0.2);
    INFO ("dispersion " << d);
    CHECK (d > 0.8);
    CHECK (d < 1.25);

    // The gaps between clicks: exponential (their spread as large as their mean).
    std::vector<double> gaps;
    for (size_t k = 1; k < r.clickTimes.size(); ++k)
        gaps.push_back (r.clickTimes[k] - r.clickTimes[k - 1]);
    const auto mean = std::accumulate (gaps.begin(), gaps.end(), 0.0) / (double) gaps.size();
    double var = 0.0;
    for (const auto g : gaps)
        var += (g - mean) * (g - mean);
    const auto cv = std::sqrt (var / (double) gaps.size()) / mean;
    INFO ("gaps: mean " << mean * 1000.0 << " ms, spread / mean " << cv);
    CHECK (cv > 0.85);
    CHECK (cv < 1.15);
}

TEST_CASE ("CRACKLE: CLUSTER bunches the clicks into bursts and lulls", "[crackle]")
{
    auto s = steady (50.0f);
    const auto even = dispersion (run (s, 60.0).clickTimes, 60.0, 0.1);
    s.cluster = 1.0f;
    const auto bunched = dispersion (run (s, 60.0).clickTimes, 60.0, 0.1);
    INFO ("dispersion: cluster 0 " << even << ", cluster 1 " << bunched);
    CHECK (even < 1.3);
    CHECK (bunched > 3.0);
}

TEST_CASE ("CRACKLE: FOLLOW clicks only while the sound plays, so silence stays silent", "[crackle]")
{
    auto s = steady (100.0f);
    s.follow = 1.0f;
    const auto quiet = run (s, 5.0, 0.0f);
    CHECK (quiet.clickTimes.empty());
    CHECK (*std::max_element (quiet.left.begin(), quiet.left.end()) == 0.0f);

    const auto loud = run (s, 5.0, 0.5f);   // -6 dBFS: past FOLLOW's full-rate level
    INFO ("clicks with a loud sound in: " << loud.clickTimes.size() << " (500 expected)");
    CHECK (std::abs ((double) loud.clickTimes.size() - 500.0) < 80.0);

    // The sound stops: the clicks die away with it (FOLLOW's 150 ms release), then nothing.
    effects::Crackle c;
    c.prepare (rate, block);
    c.seedRandom (3);
    juce::AudioBuffer<float> buffer (2, block);
    for (int b = 0; b < (int) (1.0 * rate / block); ++b)
    {
        for (int i = 0; i < block; ++i)
            buffer.setSample (0, i, 0.5f), buffer.setSample (1, i, 0.5f);
        c.process (juce::dsp::AudioBlock<float> (buffer), s);
    }
    float tail = 0.0f;
    for (int b = 0; b < (int) (3.0 * rate / block); ++b)
    {
        buffer.clear();
        c.process (juce::dsp::AudioBlock<float> (buffer), s);
        if (b * block > 2.0 * rate)
            tail = juce::jmax (tail, buffer.getMagnitude (0, block));
    }
    CHECK (tail < 1.0e-5f);
}

TEST_CASE ("CRACKLE: TONE sets the clicks' pitch, WIDTH their spread, LEVEL their peak", "[crackle]")
{
    // TONE: the spectrum's centre moves with it.
    const auto centre = [] (float toneHz)
    {
        auto s = steady (200.0f);
        s.toneHz = toneHz;
        const auto r = run (s, 4.0);
        juce::dsp::FFT fft (14);
        std::vector<float> data ((size_t) 2 << 14, 0.0f);
        std::vector<double> sum ((size_t) 1 << 13, 0.0);
        for (size_t start = 0; start + (1u << 14) <= r.left.size(); start += 1u << 14)
        {
            std::fill (data.begin(), data.end(), 0.0f);
            std::copy_n (r.left.begin() + (long) start, 1 << 14, data.begin());
            fft.performFrequencyOnlyForwardTransform (data.data());
            for (size_t k = 0; k < sum.size(); ++k)
                sum[k] += data[k] * data[k];
        }
        double num = 0.0, den = 0.0;
        for (size_t k = 1; k < sum.size(); ++k)
        {
            num += sum[k] * (double) k * rate / (1 << 14);
            den += sum[k];
        }
        return num / den;
    };
    const auto dull = centre (800.0f), sharp = centre (6000.0f);
    INFO ("spectral centre: TONE 800 Hz " << dull << " Hz, TONE 6 kHz " << sharp << " Hz");
    CHECK (dull < 2000.0);
    CHECK (sharp > 4000.0);

    // WIDTH 0: every click in the middle (both sides the same); 1: spread (the sides apart).
    auto s = steady (100.0f);
    s.width = 0.0f;
    const auto centred = run (s, 2.0);
    CHECK (centred.left == centred.right);
    s.width = 1.0f;
    const auto wide = run (s, 2.0);
    double ll = 0.0, rr = 0.0, lr = 0.0;
    for (size_t i = 0; i < wide.left.size(); ++i)
    {
        ll += wide.left[i] * wide.left[i];
        rr += wide.right[i] * wide.right[i];
        lr += wide.left[i] * wide.right[i];
    }
    const auto correlation = lr / std::sqrt (ll * rr);
    INFO ("left / right correlation at WIDTH 1: " << correlation);
    CHECK (correlation < 0.85);

    // LEVEL: the loudest clicks peak near it (each click's strength varies, the hardest at full).
    s = steady (40.0f);
    s.width = 0.0f;
    for (const auto level : { -24.0f, -12.0f, -6.0f })
    {
        s.levelDb = level;
        const auto r = run (s, 10.0);
        const auto top = *std::max_element (r.left.begin(), r.left.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
        const auto topDb = juce::Decibels::gainToDecibels (std::abs (top));
        INFO ("LEVEL " << level << " dB: the loudest click " << topDb << " dB");
        CHECK (std::abs (topDb - level) < 4.0f);
    }
}

TEST_CASE ("CRACKLE: off changes nothing; the sound passes unchanged under the clicks", "[crackle]")
{
    effects::Crackle::Settings off;
    off.rate = 500.0f;
    const auto r = run (off, 1.0, 0.3f);
    effects::Crackle::Settings none;
    none.on = true;
    none.rate = 0.0f;   // on, no clicks: the dry sound exactly
    const auto dry = run (none, 1.0, 0.3f);
    CHECK (r.left == dry.left);
    CHECK (r.clickTimes.empty());
}

TEST_CASE ("CPU: CRACKLE at a storm", "[crackle][cpu]")
{
    auto s = steady (2000.0f);
    s.cluster = 1.0f;
    const auto started = std::chrono::steady_clock::now();
    run (s, 20.0, 0.0f);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    const auto load = seconds / 20.0 * 100.0;
    WARN ("CRACKLE, 2000 clicks a second clustered, stereo, in blocks of 16: " << load << "% of a core");
    CHECK (load < 5.0);
}
