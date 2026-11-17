// SUBMERGE (0.80.0): the sound sinking under water. Off and DEPTH 0 leave the sound untouched bit for bit; under, the
// top goes (as far as MURK says), the pitch bends unevenly with the pressure (CURRENT's +-20 cents or so), bubbles rise
// only while there's music, the sea rings on after the sound, and the whole stays about as loud; back to DEPTH 0 it
// rests untouched again.
#include "SynthTestUtils.h"

#include "dsp/effects/Submerge.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Submerge::Settings;

    std::array<std::vector<float>, 2> runSubmerge (std::vector<float> left, std::vector<float> right, Settings s,
                                                   int changeAt = -1, Settings changed = {}, int* bubbles = nullptr)
    {
        effects::Submerge fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (1);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            juce::dsp::AudioBlock<float> block (channels, 2, n);
            fx.process (block, changeAt >= 0 && (int) start >= changeAt ? changed : s);
        }
        if (bubbles != nullptr)
            *bubbles = fx.getBubblesStarted();
        return { left, right };
    }

    std::vector<float> noise (double seconds, int seed, float amplitude = 0.2f)
    {
        juce::Random random (seed);
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (auto& v : x)
            v = amplitude * (2.0f * random.nextFloat() - 1.0f);
        return x;
    }

    std::vector<float> sine (double hz, double seconds, float amplitude = 0.3f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate);
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

    double highPower (const std::vector<float>& x, double hz, size_t from)
    {
        const auto a = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * hz / sampleRate);
        double low = 0.0, sum = 0.0;
        size_t count = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            low += (x[i] - low) * a;
            if (i >= from)
            {
                const auto h = x[i] - low;
                sum += h * h;
                ++count;
            }
        }
        return sum / (double) juce::jmax ((size_t) 1, count);
    }

    // The pitch in cents against `hz`, period by period (upward zero crossings, interpolated), from `from` on.
    std::vector<double> centsByPeriod (const std::vector<float>& x, double hz, size_t from)
    {
        std::vector<double> crossings, cents;
        for (auto i = from + 1; i < x.size(); ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
                crossings.push_back ((double) (i - 1) + x[i - 1] / (double) (x[i - 1] - x[i]));
        for (size_t k = 1; k < crossings.size(); ++k)
            cents.push_back (1200.0 * std::log2 (sampleRate / (crossings[k] - crossings[k - 1]) / hz));
        return cents;
    }

    Settings under (float depth = 1.0f)
    {
        Settings s;
        s.on = true;
        s.depth = depth;
        return s;
    }
}

TEST_CASE ("SUBMERGE: off and DEPTH 0 leave the sound untouched bit for bit", "[submerge]")
{
    const auto left = noise (1.0, 1), right = noise (1.0, 2);
    Settings off = under();
    off.on = false;
    const auto a = runSubmerge (left, right, off);
    CHECK (a[0] == left);
    CHECK (a[1] == right);
    const auto b = runSubmerge (left, right, under (0.0f));
    CHECK (b[0] == left);
    CHECK (b[1] == right);
    const auto c = runSubmerge (left, right, under (0.0005f));   // under 0.001 counts as 0 (the matrix eases down)
    CHECK (c[0] == left);
}

TEST_CASE ("SUBMERGE: under, the water takes the top, as far as MURK says", "[submerge]")
{
    const auto left = noise (2.0, 3), right = noise (2.0, 4);
    const auto from = (size_t) (0.5 * sampleRate);
    auto clean = under();
    clean.current = 0.0f;
    clean.bubbles = 0.0f;
    clean.sea = 0.0f;
    const auto dryHigh = highPower (left, 4000.0, from);
    const auto half = runSubmerge (left, right, clean);
    auto murky = clean;
    murky.murk = 1.0f;
    const auto deep = runSubmerge (left, right, murky);
    const auto halfDb = powerDb (highPower (half[0], 4000.0, from)) - powerDb (dryHigh);
    const auto deepDb = powerDb (highPower (deep[0], 4000.0, from)) - powerDb (dryHigh);
    INFO ("above 4 kHz: MURK 0.5 " << halfDb << " dB, MURK 1 " << deepDb << " dB");
    CHECK (halfDb < -12.0);
    CHECK (deepDb < halfDb - 10.0);
    // Half way down, less of the top goes than at the bottom.
    const auto mid = runSubmerge (left, right, [&] { auto s = clean; s.depth = 0.5f; return s; }());
    CHECK (powerDb (highPower (mid[0], 4000.0, from)) - powerDb (dryHigh) > halfDb + 3.0);
}

TEST_CASE ("SUBMERGE: the pressure bends the pitch unevenly, by CURRENT", "[submerge]")
{
    const auto tone = sine (220.0, 12.0);
    auto s = under();
    s.bubbles = 0.0f;
    s.sea = 0.0f;
    s.murk = 0.0f;
    const auto from = (size_t) (2.0 * sampleRate);
    for (const auto current : { 0.0f, 0.5f, 1.0f })
    {
        s.current = current;
        const auto out = runSubmerge (tone, tone, s);
        const auto cents = centsByPeriod (out[0], 220.0, from);
        REQUIRE (cents.size() > 1000);
        // Smoothed over 10 periods (zero crossings wobble a little on the darkened sine).
        double low = 1.0e9, high = -1.0e9;
        for (size_t k = 10; k < cents.size(); ++k)
        {
            double mean = 0.0;
            for (size_t j = k - 10; j < k; ++j)
                mean += cents[j] / 10.0;
            low = juce::jmin (low, mean);
            high = juce::jmax (high, mean);
        }
        INFO ("CURRENT " << current << ": " << low << " to " << high << " cents");
        if (current == 0.0f)
            CHECK (high - low < 1.0);
        else
        {
            CHECK (high > 2.0 * current);
            CHECK (low < -2.0 * current);
            CHECK (high < 26.0);
            CHECK (low > -26.0);
        }
        if (current == 1.0f)
            CHECK (high - low > 15.0);
    }
}

TEST_CASE ("SUBMERGE: turning DEPTH up at once only sags the pitch a little", "[submerge]")
{
    const auto tone = sine (220.0, 3.0);
    auto s = under();
    s.bubbles = 0.0f;
    s.sea = 0.0f;
    s.murk = 0.0f;
    s.current = 1.0f;
    auto off = s;
    off.depth = 0.0f;
    const auto out = runSubmerge (tone, tone, off, (int) sampleRate, s);
    const auto cents = centsByPeriod (out[0], 220.0, (size_t) (0.9 * sampleRate));
    double low = 0.0;
    for (size_t k = 10; k < cents.size(); ++k)
    {
        double mean = 0.0;
        for (size_t j = k - 10; j < k; ++j)
            mean += cents[j] / 10.0;
        low = juce::jmin (low, mean);
    }
    INFO ("lowest: " << low << " cents");
    CHECK (low > -26.0);
}

TEST_CASE ("SUBMERGE: bubbles rise only while there's music, as many as BUBBLES says", "[submerge]")
{
    auto s = under();
    s.bubbles = 1.0f;
    const auto music = sawChord (10.0);
    int count = 0;
    runSubmerge (music, music, s, -1, {}, &count);
    INFO ("bubbles in 10 s at full: " << count);
    CHECK (count > 90);   // 14 a second at full
    CHECK (count < 200);
    s.bubbles = 0.3f;
    int fewer = 0;
    runSubmerge (music, music, s, -1, {}, &fewer);
    CHECK (fewer < count / 2);
    CHECK (fewer > 15);

    // Silence: no bubbles, nothing at all.
    s.bubbles = 1.0f;
    const std::vector<float> silence ((size_t) (5.0 * sampleRate), 0.0f);
    int none = -1;
    const auto quiet = runSubmerge (silence, silence, s, -1, {}, &none);
    CHECK (none == 0);
    CHECK (std::all_of (quiet[0].begin(), quiet[0].end(), [] (float v) { return v == 0.0f; }));
    CHECK (std::all_of (quiet[1].begin(), quiet[1].end(), [] (float v) { return v == 0.0f; }));

    // A bubble rises: its pitch climbs. A sine at 100 Hz with bubbles alone (no sea, no murk) leaves short tones above.
    s.sea = 0.0f;
    s.current = 0.0f;
    s.murk = 0.0f;
    const auto low = sine (60.0, 4.0, 0.3f);
    const auto out = runSubmerge (low, low, s);
    CHECK (highPower (out[0], 250.0, (size_t) sampleRate) > 1.0e-6);
}

TEST_CASE ("SUBMERGE: the sea rings on after the sound, by SEA", "[submerge]")
{
    auto burst = noise (0.5, 5);
    burst.resize ((size_t) (3.0 * sampleRate), 0.0f);
    auto s = under();
    s.bubbles = 0.0f;
    s.current = 0.0f;
    s.sea = 1.0f;
    const auto wet = runSubmerge (burst, burst, s);
    s.sea = 0.0f;
    const auto dry = runSubmerge (burst, burst, s);
    const auto later = (size_t) (1.5 * sampleRate);
    const auto tail = powerDb (power (wet[0], later, later + (size_t) (0.5 * sampleRate)));
    const auto none = powerDb (power (dry[0], later, later + (size_t) (0.5 * sampleRate)));
    const auto during = powerDb (power (wet[0], (size_t) (0.2 * sampleRate), (size_t) (0.5 * sampleRate)));
    INFO ("during " << during << " dB, a second after: sea " << tail << " dB, without " << none << " dB");
    CHECK (tail > during - 40.0);
    CHECK (tail < during - 6.0);
    CHECK (none < -100.0);
    // Wide: the sides differ.
    CHECK (wet[0] != wet[1]);
}

TEST_CASE ("SUBMERGE: the whole stays about as loud", "[submerge]")
{
    const auto music = sawChord (6.0);
    const auto from = (size_t) (2.0 * sampleRate);
    const auto dryDb = powerDb (power (music, from, music.size()));
    for (const auto depth : { 0.5f, 1.0f })
    {
        auto s = under (depth);
        s.bubbles = 0.0f;
        s.sea = 1.0f;
        const auto out = runSubmerge (music, music, s);
        const auto outDb = powerDb (power (out[0], from, out[0].size()));
        INFO ("DEPTH " << depth << ": " << outDb - dryDb << " dB");
        CHECK (std::abs (outDb - dryDb) < 2.0);
    }
}

TEST_CASE ("SUBMERGE: back to DEPTH 0, it rests untouched again", "[submerge]")
{
    const auto left = noise (12.0, 6), right = noise (12.0, 7);
    auto s = under();
    s.bubbles = 1.0f;
    s.sea = 1.0f;
    auto off = s;
    off.depth = 0.0f;
    const auto out = runSubmerge (left, right, s, (int) (2.0 * sampleRate), off);
    // The sea dies away over a few seconds, then everything is cleared and the sound passes untouched.
    const auto late = (size_t) (11.0 * sampleRate);
    CHECK (std::equal (out[0].begin() + (long) late, out[0].end(), left.begin() + (long) late));
    CHECK (std::equal (out[1].begin() + (long) late, out[1].end(), right.begin() + (long) late));
}

TEST_CASE ("SUBMERGE: CPU", "[submerge][cpu]")
{
    const auto music = sawChord (10.0);
    auto s = under();
    s.bubbles = 1.0f;
    s.sea = 1.0f;
    const auto t0 = std::chrono::steady_clock::now();
    runSubmerge (music, music, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const auto share = 100.0 * seconds / 10.0;
    WARN ("SUBMERGE at full: " << share << "% of a core");
    CHECK (share < 10.0);
}
