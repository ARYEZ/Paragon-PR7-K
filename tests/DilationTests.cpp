// DILATION (0.63.0): time slowing down. Checked by what it does to the sound: at SLOW 0 (and off) the sound passes
// untouched; slowed, the pitch falls with the speed (an octave at half speed); it never falls further behind than
// REACH and goes on slowed past it; GLIDE catches up faster than real time and then passes the sound untouched
// again; SNAP comes back at once; TONE darkens; the sides stay apart; CPU.
#include "SynthTestUtils.h"

#include "dsp/effects/Dilation.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Dilation::Settings;

    struct Run
    {
        std::array<std::vector<float>, 2> out;
        std::vector<float> behind, speed;   // per block
    };

    // `input (sample)` gives both sides; `at (seconds)` the settings for each block.
    template <typename Input, typename At>
    Run run (double seconds, Input input, At at)
    {
        effects::Dilation fx;
        fx.prepare (sampleRate, blockSize);
        Run r;
        std::vector<float> l ((size_t) blockSize), rr ((size_t) blockSize);
        float* ptrs[] = { l.data(), rr.data() };
        long long t = 0;
        for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
        {
            for (size_t i = 0; i < (size_t) blockSize; ++i)
            {
                const auto [a, c] = input (t + (long long) i);
                l[i] = a;
                rr[i] = c;
            }
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) blockSize), at ((double) t / sampleRate));
            r.out[0].insert (r.out[0].end(), l.begin(), l.end());
            r.out[1].insert (r.out[1].end(), rr.begin(), rr.end());
            r.behind.push_back (fx.getBehindSeconds());
            r.speed.push_back (fx.getSpeed());
            t += blockSize;
        }
        return r;
    }

    Settings slowed (float slow, float ramp = 0.0f)
    {
        Settings s;
        s.on = true;
        s.slow = slow;
        s.ramp = ramp;
        s.tone = 1.0f;   // no darkening, unless a test asks
        return s;
    }

    std::pair<float, float> sine440 (long long i)
    {
        const auto v = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (double) i / sampleRate);
        return { v, v };
    }

    size_t at (double seconds) { return (size_t) (seconds * sampleRate); }
}

TEST_CASE ("DILATION: at SLOW 0, or off, the sound passes exactly as it was", "[dilation]")
{
    const auto input = [] (long long i) { const auto v = 0.3f * (float) std::sin (0.031 * (double) i); return std::pair<float, float> { v, -0.5f * v }; };
    std::vector<float> l, r;
    for (long long i = 0; i < (long long) (2.0 * sampleRate / blockSize) * blockSize; ++i)
    {
        const auto [a, b] = input (i);
        l.push_back (a);
        r.push_back (b);
    }
    const auto zero = run (2.0, input, [] (double) { return slowed (0.0f); });
    CHECK (zero.out[0] == l);
    CHECK (zero.out[1] == r);
    auto off = slowed (0.8f);
    off.on = false;
    CHECK (run (2.0, input, [off] (double) { return off; }).out[0] == l);
}

TEST_CASE ("DILATION: slowed, the pitch falls with the speed: half speed is an octave down", "[dilation]")
{
    for (const auto slow : { 0.25f, 0.5f, 1.0f })
    {
        const auto r = run (2.0, sine440, [slow] (double) { return slowed (slow); });
        const auto expected = 440.0 * effects::Dilation::speedFor (slow);
        const auto heard = frequencyOf (r.out[0], at (0.5), at (1.0));
        INFO ("SLOW " << slow << ": expected " << expected << " Hz, heard " << heard << " Hz");
        CHECK (std::abs (heard - expected) < expected * 0.005);
        // A clean slowdown: the slowed tone, not the original or rubbish.
        CHECK (db (levelAt (r.out[0], expected, at (0.5), at (0.5))) > db (0.4) - 1.0);
        CHECK (db (levelAt (r.out[0], 440.0, at (0.5), at (0.5))) < db (0.4) - 40.0);
    }
}

TEST_CASE ("DILATION: RAMP is how long the slide takes", "[dilation]")
{
    // RAMP 0.5 is about 0.45 s: a quarter of the way in, the speed is still well above where it's heading; by twice
    // RAMP it's there.
    const auto rampSeconds = effects::Dilation::rampSeconds (0.5f);
    const auto r = run (3.0, sine440, [] (double) { return slowed (0.5f, 0.5f); });
    const auto speedAt = [&r] (double seconds) { return r.speed[(size_t) (seconds * sampleRate / blockSize)]; };
    INFO ("RAMP " << rampSeconds << " s: speed at a quarter " << speedAt (rampSeconds / 4) << ", at twice " << speedAt (2 * rampSeconds));
    CHECK (speedAt (rampSeconds / 4) > 0.65f);   // (0.69: smoothly, most of the way over RAMP)
    CHECK (std::abs (speedAt (2.0 * rampSeconds) - 0.5f) < 0.01f);
}

TEST_CASE ("DILATION: it never falls further behind than REACH, and stays slowed past it", "[dilation]")
{
    auto s = slowed (0.5f);
    s.reach = 0.0f;   // 1 s
    const auto r = run (6.0, sine440, [s] (double) { return s; });
    const auto most = *std::max_element (r.behind.begin(), r.behind.end());
    INFO ("furthest behind " << most << " s");
    CHECK (most <= 1.01f);
    CHECK (most > 0.9f);
    // Half speed falls a second behind in two seconds: past that (and a jump back) it's still an octave down, and the
    // jump is faded (no sample steps further than the tone itself moves).
    CHECK (std::abs (frequencyOf (r.out[0], at (4.0), at (5.5)) - 220.0) < 2.0);
    auto step = 0.0f;
    for (auto i = at (1.0); i < r.out[0].size(); ++i)
        step = std::max (step, std::abs (r.out[0][i] - r.out[0][i - 1]));
    const auto sineStep = 0.4f * (float) (juce::MathConstants<double>::twoPi * 220.0 / sampleRate);
    INFO ("largest step " << step << ", the slowed sine's own " << sineStep);
    CHECK (step < sineStep * 1.6f);
}

TEST_CASE ("DILATION: GLIDE catches up faster than real time, then passes the sound untouched again", "[dilation]")
{
    // Slowed for a second, then SLOW back to 0.
    const auto r = run (6.0, sine440, [] (double t) { return slowed (t < 1.0 ? 0.5f : 0.0f, 0.3f); });
    const auto block = [] (double seconds) { return (size_t) (seconds * sampleRate / blockSize); };
    CHECK (r.behind[block (1.0)] > 0.3f);
    // Catching up: faster than real time (the pitch above 440 Hz).
    const auto fastest = *std::max_element (r.speed.begin() + (long) block (1.0), r.speed.end());
    INFO ("fastest " << fastest);
    CHECK (fastest > 1.2f);
    // Caught up within a few seconds; after that the sound itself, untouched.
    const auto caughtUp = std::find_if (r.behind.begin() + (long) block (1.0), r.behind.end(), [] (float b) { return b <= 0.0f; });
    REQUIRE (caughtUp != r.behind.end());
    const auto when = (double) (caughtUp - r.behind.begin()) * blockSize / sampleRate;
    INFO ("caught up at " << when << " s");
    CHECK (when < 4.0);
    auto differs = 0.0f;
    for (auto i = at (when + 0.5); i < r.out[0].size(); ++i)
        differs = std::max (differs, std::abs (r.out[0][i] - sine440 ((long long) i).first));
    CHECK (differs == 0.0f);
}

TEST_CASE ("DILATION: SNAP comes back to real time at once", "[dilation]")
{
    const auto r = run (2.0, sine440, [] (double t) { auto s = slowed (t < 1.0 ? 0.5f : 0.0f); s.returnMode = 1; return s; });
    CHECK (r.behind[(size_t) (1.1 * sampleRate / blockSize)] == 0.0f);
    auto differs = 0.0f;
    for (auto i = at (1.2); i < r.out[0].size(); ++i)
        differs = std::max (differs, std::abs (r.out[0][i] - sine440 ((long long) i).first));
    CHECK (differs == 0.0f);
}

TEST_CASE ("DILATION: TONE 0 darkens the slowed sound; TONE 1 leaves its top", "[dilation]")
{
    // A 4 kHz tone slowed to 2 kHz: at TONE 1 it keeps its level, at TONE 0 (the top at 20 kHz x 0.25 = 5 kHz) it's
    // a little down, and slowed to a quarter (1 kHz, the top at 1.25 kHz) well down.
    const auto bright = [] (long long i) { const auto v = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 4000.0 * (double) i / sampleRate); return std::pair<float, float> { v, v }; };
    const auto level = [&bright] (float slow, float tone)
    {
        auto s = slowed (slow);
        s.tone = tone;
        const auto r = run (1.5, bright, [s] (double) { return s; });
        return db (levelAt (r.out[0], 4000.0 * effects::Dilation::speedFor (slow), at (0.5), at (0.5)));
    };
    const auto open = level (1.0f, 1.0f), dark = level (1.0f, 0.0f);
    INFO ("a quarter speed: TONE 1 " << open << " dB, TONE 0 " << dark << " dB");
    CHECK (open > db (0.4) - 0.5);
    CHECK (dark < open - 1.5);
    CHECK (level (0.5f, 0.0f) > dark);
}

TEST_CASE ("DILATION: the sides stay apart", "[dilation]")
{
    const auto leftOnly = [] (long long i) { return std::pair<float, float> { sine440 (i).first, 0.0f }; };
    const auto r = run (3.0, leftOnly, [] (double t) { return slowed (t < 1.5 ? 0.7f : 0.0f, 0.4f); });
    CHECK (*std::max_element (r.out[1].begin(), r.out[1].end()) == 0.0f);
    CHECK (*std::min_element (r.out[1].begin(), r.out[1].end()) == 0.0f);
    CHECK (rms (r.out[0], at (0.5), at (1.5)) > 0.2);
}

TEST_CASE ("CPU: DILATION, slowed and catching up", "[dilation][cpu]")
{
    const auto started = std::chrono::steady_clock::now();
    run (20.0, sine440, [] (double t) { return slowed (std::fmod (t, 4.0) < 2.0 ? 0.6f : 0.0f, 0.5f); });
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("DILATION, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input included)");
    CHECK (seconds / 20.0 * 100.0 < 5.0);
}
