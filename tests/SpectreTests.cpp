// SPECTRE (0.67.0): a whispered ghost of the sound. Checked by what it does to the sound: off and at MIX 0 untouched;
// the ghost (what's added) is about as loud as the sound at MIX 1; it keeps the outline (a bright sound's ghost is
// bright, a dark one's dark) but not the pitch (between a saw's harmonics it's as loud as on them); SHIFT moves the
// outline an octave; TRAIL puts it behind; LINGER holds it after; the sides stay apart at DRIFT 0 and silence stays
// silent; CPU.
#include "SynthTestUtils.h"

#include "dsp/effects/Spectre.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Spectre::Settings;

    // Runs the input through SPECTRE; returns the output and the ghost (output less input), left and right.
    struct Result
    {
        std::array<std::vector<float>, 2> out, ghost;
    };

    template <typename Input>
    Result run (double seconds, Input input, Settings s)
    {
        effects::Spectre fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (7);
        Result result;
        std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) std::ceil (seconds * sampleRate / blockSize); ++b)   // at least `seconds`, so a measurement up to the end stays inside
        {
            std::vector<float> inL ((size_t) blockSize), inR ((size_t) blockSize);
            for (size_t i = 0; i < (size_t) blockSize; ++i)
            {
                const auto [a, c] = input (t + (long long) i);
                l[i] = inL[i] = a;
                r[i] = inR[i] = c;
            }
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) blockSize), s);
            for (size_t i = 0; i < (size_t) blockSize; ++i)
            {
                result.out[0].push_back (l[i]);
                result.out[1].push_back (r[i]);
                result.ghost[0].push_back (l[i] - inL[i]);
                result.ghost[1].push_back (r[i] - inR[i]);
            }
            t += blockSize;
        }
        return result;
    }

    Settings ghostly (float mix = 1.0f)
    {
        Settings s;
        s.on = true;
        s.mix = mix;
        s.trail = 0.0f;
        s.linger = 0.3f;
        s.air = 0.6f;
        s.drift = 0.0f;
        s.shift = 0.0f;
        return s;
    }

    size_t at (double seconds) { return (size_t) (seconds * sampleRate); }

    double both (const std::array<std::vector<float>, 2>& x, double from, double to)
    {
        return db (std::sqrt (0.5 * (std::pow (rms (x[0], at (from), at (to)), 2.0) + std::pow (rms (x[1], at (from), at (to)), 2.0))));
    }

    // Power between two frequencies (linear, summed over 16384-sample frames from `from` to `to`).
    double powerBetween (const std::vector<float>& x, double from, double to, double loHz, double hiHz)
    {
        constexpr int order = 14, size = 1 << order;
        juce::dsp::FFT fft (order);
        double total = 0.0;
        for (auto start = at (from); start + (size_t) size <= at (to); start += (size_t) size)
        {
            std::vector<float> data ((size_t) size * 2, 0.0f);
            for (int i = 0; i < size; ++i)
                data[(size_t) i] = x[start + (size_t) i] * (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (size - 1)));
            fft.performFrequencyOnlyForwardTransform (data.data());
            for (int bin = (int) (loHz * size / sampleRate); bin <= (int) (hiHz * size / sampleRate); ++bin)
                total += (double) data[(size_t) bin] * data[(size_t) bin];
        }
        return total;
    }

    // Pink-ish noise (white through a gentle one-pole), the same each run.
    struct Pink
    {
        std::uint32_t s = 12345u;
        float lp = 0.0f;
        float next()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            const auto white = (float) (s >> 8) / 8388608.0f - 1.0f;
            lp += 0.15f * (white - lp);
            return 0.6f * lp + 0.08f * white;
        }
    };

    std::pair<float, float> sine (double hz, long long i, float level = 0.3f)
    {
        const auto v = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate);
        return { v, v };
    }

    std::pair<float, float> saw (double hz, long long i)
    {
        const auto phase = std::fmod (hz * (double) i / sampleRate, 1.0);
        const auto v = 0.3f * (float) (2.0 * phase - 1.0);
        return { v, v };
    }
}

TEST_CASE ("SPECTRE: off and at MIX 0 the sound passes exactly as it was", "[spectre]")
{
    const auto input = [] (long long i) { const auto v = 0.3f * (float) std::sin (0.017 * (double) i); return std::pair<float, float> { v, -0.6f * v }; };
    auto zero = ghostly (0.0f);
    const auto z = run (1.5, input, zero);
    for (int side = 0; side < 2; ++side)
        CHECK (*std::max_element (z.ghost[(size_t) side].begin(), z.ghost[(size_t) side].end()) == 0.0f);
    auto off = ghostly (1.0f);
    off.on = false;
    const auto o = run (1.5, input, off);
    CHECK (*std::max_element (o.ghost[0].begin(), o.ghost[0].end()) == 0.0f);
    CHECK (*std::min_element (o.ghost[0].begin(), o.ghost[0].end()) == 0.0f);
}

TEST_CASE ("SPECTRE: at MIX 1 the ghost is about as loud as the sound", "[spectre]")
{
    Pink pink;
    std::vector<float> noise;
    for (int i = 0; i < (int) (4.0 * sampleRate); ++i)
        noise.push_back (pink.next());
    const auto r = run (4.0, [&noise] (long long i) { return std::pair<float, float> { noise[(size_t) i], noise[(size_t) i] }; }, ghostly());
    const auto input = db (rms (noise, at (1.0), at (4.0)));
    const auto ghost = both (r.ghost, 1.0, 4.0);
    INFO ("pink noise " << input << " dB, its ghost " << ghost << " dB");
    CHECK (std::abs (ghost - input) < 3.0);
    // A saw too (a sound with a pitch): within 4 dB.
    const auto s = run (3.0, [] (long long i) { return saw (220.0, i); }, ghostly());
    INFO ("saw " << both (s.out, 1.0, 1.0001) << ", its ghost " << both (s.ghost, 1.0, 3.0) << " dB");
    CHECK (std::abs (both (s.ghost, 1.0, 3.0) - db (0.3 / std::sqrt (3.0))) < 4.0);
}

TEST_CASE ("SPECTRE: the ghost keeps the outline but not the pitch", "[spectre]")
{
    // A tone at 3 kHz: its ghost sits round 3 kHz (within a third of an octave or so), little of it far off.
    const auto high = run (3.0, [] (long long i) { return sine (3000.0, i); }, ghostly());
    const auto nearHigh = powerBetween (high.ghost[0], 1.0, 3.0, 2300.0, 3900.0), lowDown = powerBetween (high.ghost[0], 1.0, 3.0, 100.0, 1200.0);
    INFO ("3 kHz tone's ghost: near " << 10.0 * std::log10 (nearHigh) << " dB, 100-1200 Hz " << 10.0 * std::log10 (lowDown) << " dB");
    CHECK (nearHigh > 20.0 * lowDown);
    // A 400 Hz tone's ghost sits low.
    const auto low = run (3.0, [] (long long i) { return sine (400.0, i); }, ghostly());
    const auto lowHome = powerBetween (low.ghost[0], 1.0, 3.0, 280.0, 560.0), lowUp = powerBetween (low.ghost[0], 1.0, 3.0, 2000.0, 8000.0);
    INFO ("400 Hz tone's ghost: round 400 Hz " << 10.0 * std::log10 (lowHome) << " dB, 2-8 kHz " << 10.0 * std::log10 (lowUp) << " dB");
    CHECK (lowHome > 20.0 * lowUp);

    // No pitch: a 200 Hz saw's ghost is breath. Between its harmonics (fifty hertz either side of halfway) it's
    // about as loud as on them; the saw itself is 40 dB and more quieter there.
    const auto sawGhost = run (3.0, [] (long long i) { return saw (200.0, i); }, ghostly());
    double on = 0.0, between = 0.0;
    for (int h = 4; h <= 12; ++h)
    {
        on += powerBetween (sawGhost.ghost[0], 1.0, 3.0, 200.0 * h - 15.0, 200.0 * h + 15.0);
        between += powerBetween (sawGhost.ghost[0], 1.0, 3.0, 200.0 * h + 85.0, 200.0 * h + 115.0);
    }
    INFO ("on the harmonics " << 10.0 * std::log10 (on) << " dB, between them " << 10.0 * std::log10 (between) << " dB");
    CHECK (std::abs (10.0 * std::log10 (on / between)) < 6.0);
}

TEST_CASE ("SPECTRE: SHIFT moves the outline up or down an octave", "[spectre]")
{
    auto up = ghostly();
    up.shift = 1.0f;
    const auto r = run (3.0, [] (long long i) { return sine (1000.0, i); }, up);
    const auto octaveUp = powerBetween (r.ghost[0], 1.0, 3.0, 1600.0, 2500.0), home = powerBetween (r.ghost[0], 1.0, 3.0, 800.0, 1250.0);
    INFO ("1 kHz, SHIFT +1: round 2 kHz " << 10.0 * std::log10 (octaveUp) << " dB, round 1 kHz " << 10.0 * std::log10 (home) << " dB");
    CHECK (octaveUp > 4.0 * home);
    auto down = ghostly();
    down.shift = -1.0f;
    const auto d = run (3.0, [] (long long i) { return sine (1000.0, i); }, down);
    const auto octaveDown = powerBetween (d.ghost[0], 1.0, 3.0, 400.0, 620.0), stayed = powerBetween (d.ghost[0], 1.0, 3.0, 800.0, 1250.0);
    INFO ("1 kHz, SHIFT -1: round 500 Hz " << 10.0 * std::log10 (octaveDown) << " dB, round 1 kHz " << 10.0 * std::log10 (stayed) << " dB");
    CHECK (octaveDown > 4.0 * stayed);
}

TEST_CASE ("SPECTRE: TRAIL puts it behind, LINGER holds it after", "[spectre]")
{
    // A burst from 0.5 s to 1.5 s.
    const auto burst = [] (long long i) { return i >= (long long) (0.5 * sampleRate) && i < (long long) (1.5 * sampleRate) ? sine (800.0, i) : std::pair<float, float> { 0.0f, 0.0f }; };
    auto behind = ghostly();
    behind.trail = 1.0f;   // 800 ms
    const auto t = run (3.0, burst, behind);
    INFO ("TRAIL 800 ms: 0.6-0.9 s " << both (t.ghost, 0.6, 0.9) << " dB, 1.5-1.8 s " << both (t.ghost, 1.5, 1.8) << " dB");
    CHECK (both (t.ghost, 0.6, 0.9) < -100.0);       // nothing yet
    CHECK (both (t.ghost, 1.5, 1.8) > -30.0);        // there now, after the sound itself
    const auto plain = run (3.0, burst, ghostly());
    CHECK (both (plain.ghost, 0.6, 0.9) > -30.0);    // TRAIL 0: with the sound

    auto brief = ghostly(), long_ = ghostly();
    brief.linger = 0.0f;   // 0.1 s to fall 60 dB
    long_.linger = 1.0f;   // 4 s
    const auto b = run (3.0, burst, brief), l = run (3.0, burst, long_);
    const auto briefAfter = both (b.ghost, 1.8, 2.0), longAfter = both (l.ghost, 1.8, 2.0), longDuring = both (l.ghost, 1.0, 1.4);
    INFO ("0.3-0.5 s after: LINGER 0 " << briefAfter << " dB, LINGER 1 " << longAfter << " dB (" << longDuring << " dB during)");
    CHECK (briefAfter < -100.0);
    CHECK (longAfter > longDuring - 12.0);
}

TEST_CASE ("SPECTRE: at DRIFT 0 the sides stay apart; silence stays silent", "[spectre]")
{
    const auto leftOnly = run (2.0, [] (long long i) { return std::pair<float, float> { sine (600.0, i).first, 0.0f }; }, ghostly());
    CHECK (*std::max_element (leftOnly.out[1].begin(), leftOnly.out[1].end()) == 0.0f);
    CHECK (*std::min_element (leftOnly.out[1].begin(), leftOnly.out[1].end()) == 0.0f);
    CHECK (both (leftOnly.ghost, 1.0, 2.0) > -30.0);
    // With DRIFT the bands wander over to the other side.
    auto drifting = ghostly();
    drifting.drift = 1.0f;
    const auto d = run (6.0, [] (long long i) { return std::pair<float, float> { sine (600.0, i).first, 0.0f }; }, drifting);
    CHECK (db (rms (d.ghost[1], at (1.0), at (6.0))) > -40.0);

    auto s = ghostly();
    s.trail = 1.0f;
    s.linger = 1.0f;
    s.drift = 1.0f;
    const auto r = run (14.0, [] (long long i) { return i < (long long) (2.0 * sampleRate) ? saw (220.0, i) : std::pair<float, float> { 0.0f, 0.0f }; }, s);
    float tail = 0.0f;
    for (auto i = at (12.0); i < r.out[0].size(); ++i)
        tail = std::max (tail, std::max (std::abs (r.out[0][i]), std::abs (r.out[1][i])));
    CHECK (tail == 0.0f);
}

TEST_CASE ("CPU: SPECTRE, a drifting ghost", "[spectre][cpu]")
{
    auto s = ghostly (0.7f);
    s.trail = 0.4f;
    s.drift = 0.8f;
    s.shift = 0.3f;
    const auto started = std::chrono::steady_clock::now();
    run (20.0, [] (long long i) { return saw (220.0, i); }, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("SPECTRE, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input and bookkeeping included)");
    CHECK (seconds / 20.0 * 100.0 < 8.0);
}

