// NUCLEARSPRING (0.69.0): a vintage spring reverb tank. Checked by what it does to the sound: off and at MIX 0
// untouched; the tank about as loud as the sound at MIX 1; it falls 60 dB in about DECAY's time; DRIP spreads a
// click out in time (its low part and high part arriving apart); BOING kicks a sudden hit harder than the same
// level faded in; nothing under 100 Hz goes in; WIDTH 0 puts the tail in the middle; CPU.
#include "SynthTestUtils.h"

#include "dsp/effects/NuclearSpring.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::NuclearSpring::Settings;

    struct Result
    {
        std::array<std::vector<float>, 2> out, wet;
    };

    template <typename Input>
    Result run (double seconds, Input input, Settings s)
    {
        effects::NuclearSpring fx;
        fx.prepare (sampleRate, blockSize);
        Result result;
        std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) std::ceil (seconds * sampleRate / blockSize); ++b)   // at least `seconds`
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
                result.wet[0].push_back (l[i] - inL[i]);
                result.wet[1].push_back (r[i] - inR[i]);
            }
            t += blockSize;
        }
        return result;
    }

    Settings tank (float mix = 1.0f)
    {
        Settings s;
        s.on = true;
        s.mix = mix;
        s.decay = 0.4f;
        s.drip = 0.5f;
        s.tone = 0.6f;
        s.size = 0.5f;
        s.boing = 0.0f;
        s.width = 1.0f;
        return s;
    }

    size_t at (double seconds) { return (size_t) (seconds * sampleRate); }

    double both (const std::array<std::vector<float>, 2>& x, double from, double to)
    {
        return db (std::sqrt (0.5 * (std::pow (rms (x[0], at (from), at (to)), 2.0) + std::pow (rms (x[1], at (from), at (to)), 2.0))));
    }

    std::pair<float, float> sine (double hz, long long i, float level = 0.3f)
    {
        const auto v = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate);
        return { v, v };
    }

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

    // The time (seconds after `from`) at which half of a band's energy in [from, to) has arrived, the band picked out
    // by a two-pole band-pass at `hz`.
    double arrival (const std::vector<float>& x, double from, double to, double hz)
    {
        const auto g = std::tan (juce::MathConstants<double>::pi * hz / sampleRate), k = 1.0 / 1.5;
        const auto a1 = 1.0 / (1.0 + g * (g + k));
        double ic1 = 0.0, ic2 = 0.0, total = 0.0;
        std::vector<double> energy;
        for (auto i = at (from); i < at (to); ++i)
        {
            const auto v1 = a1 * (ic1 + g * ((double) x[i] - ic2));
            const auto v2 = ic2 + g * v1;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            energy.push_back (v1 * v1);
            total += v1 * v1;
        }
        double sum = 0.0;
        for (size_t i = 0; i < energy.size(); ++i)
            if ((sum += energy[i]) >= 0.5 * total)
                return (double) i / sampleRate;
        return to - from;
    }
}

TEST_CASE ("NUCLEARSPRING: off and at MIX 0 the sound passes exactly as it was", "[nuclearspring]")
{
    const auto input = [] (long long i) { const auto v = 0.3f * (float) std::sin (0.017 * (double) i); return std::pair<float, float> { v, -0.6f * v }; };
    const auto z = run (1.5, input, tank (0.0f));
    for (int side = 0; side < 2; ++side)
    {
        CHECK (*std::max_element (z.wet[(size_t) side].begin(), z.wet[(size_t) side].end()) == 0.0f);
        CHECK (*std::min_element (z.wet[(size_t) side].begin(), z.wet[(size_t) side].end()) == 0.0f);
    }
    auto off = tank (1.0f);
    off.on = false;
    const auto o = run (1.5, input, off);
    CHECK (*std::max_element (o.wet[0].begin(), o.wet[0].end()) == 0.0f);
    CHECK (*std::min_element (o.wet[0].begin(), o.wet[0].end()) == 0.0f);
}

TEST_CASE ("NUCLEARSPRING: the tank about as loud as the sound at MIX 1; it falls 60 dB in about DECAY's time", "[nuclearspring]")
{
    Pink pink;
    std::vector<float> noise;
    for (int i = 0; i < (int) (4.0 * sampleRate); ++i)
        noise.push_back (pink.next());
    const auto r = run (4.0, [&noise] (long long i) { return std::pair<float, float> { noise[(size_t) i], noise[(size_t) i] }; }, tank());
    const auto input = db (rms (noise, at (1.5), at (4.0)));
    INFO ("pink noise " << input << " dB, the tank " << both (r.wet, 1.5, 4.0) << " dB");
    CHECK (std::abs (both (r.wet, 1.5, 4.0) - input) < 4.0);

    // A short burst; DECAY 0.4 (about 1.6 s to fall 60 dB): over 0.8 s of tail it falls about 30 dB.
    auto s = tank();
    const auto seconds = effects::NuclearSpring::decaySeconds (s.decay);
    const auto burst = [] (long long i) { return i < (long long) (0.2 * sampleRate) ? std::pair<float, float> { (float) (0.3 * std::sin (0.13 * (double) i)), (float) (0.3 * std::sin (0.13 * (double) i)) } : std::pair<float, float> { 0.0f, 0.0f }; };
    const auto t = run (4.0, burst, s);
    const auto early = both (t.wet, 0.4, 0.5), later = both (t.wet, 0.4 + seconds / 2.0, 0.5 + seconds / 2.0);
    INFO ("DECAY " << seconds << " s: " << early << " dB, half that later " << later << " dB");
    CHECK (early - later > 22.0);
    CHECK (early - later < 40.0);
}

TEST_CASE ("NUCLEARSPRING: DRIP spreads a click out in time, its low and high parts arriving apart", "[nuclearspring]")
{
    const auto click = [] (long long i) { return i == (long long) (0.1 * sampleRate) ? std::pair<float, float> { 0.8f, 0.8f } : std::pair<float, float> { 0.0f, 0.0f }; };
    const auto spread = [&click] (float drip)
    {
        auto s = tank();
        s.drip = drip;
        s.tone = 1.0f;
        const auto r = run (0.5, click, s);
        return std::abs (arrival (r.wet[0], 0.1, 0.2, 400.0) - arrival (r.wet[0], 0.1, 0.2, 3000.0));
    };
    const auto plain = spread (0.0f), wet = spread (1.0f);
    INFO ("400 Hz against 3 kHz, the gap in arrival: DRIP 0 " << plain * 1000.0 << " ms, DRIP 1 " << wet * 1000.0 << " ms");
    CHECK (wet > plain + 0.002);
}

TEST_CASE ("NUCLEARSPRING: BOING kicks a sudden hit harder than the same level faded in", "[nuclearspring]")
{
    const auto sudden = [] (long long i) { return i >= (long long) (0.3 * sampleRate) && i < (long long) (0.5 * sampleRate) ? sine (900.0, i, 0.4f) : std::pair<float, float> { 0.0f, 0.0f }; };
    const auto faded = [] (long long i)
    {
        const auto t = (double) i / sampleRate;
        if (t < 0.15 || t >= 0.5) return std::pair<float, float> { 0.0f, 0.0f };
        const auto ramp = (float) juce::jmin (1.0, (t - 0.15) / 0.15);   // up over 150 ms, full from 0.3 s
        const auto [a, b] = sine (900.0, i, 0.4f);
        return std::pair<float, float> { a * ramp, b * ramp };
    };
    const auto gap = [&] (float boing)
    {
        auto s = tank();
        s.boing = boing;
        return both (run (1.0, sudden, s).wet, 0.32, 0.42) - both (run (1.0, faded, s).wet, 0.32, 0.42);
    };
    const auto without = gap (0.0f), with = gap (1.0f);
    INFO ("the sudden hit's tank against the faded-in one's: BOING 0 " << without << " dB, BOING 1 " << with << " dB");
    CHECK (with > without + 3.0);
}

TEST_CASE ("NUCLEARSPRING: nothing under 100 Hz goes in; WIDTH 0 puts the tail in the middle", "[nuclearspring]")
{
    const auto low = run (2.0, [] (long long i) { return sine (40.0, i); }, tank());
    const auto mid = run (2.0, [] (long long i) { return sine (1000.0, i); }, tank());
    INFO ("the tank on 40 Hz " << both (low.wet, 1.0, 2.0) << " dB, on 1 kHz " << both (mid.wet, 1.0, 2.0) << " dB");
    CHECK (both (low.wet, 1.0, 2.0) < both (mid.wet, 1.0, 2.0) - 20.0);

    auto narrow = tank();
    narrow.width = 0.0f;
    const auto leftOnly = run (1.5, [] (long long i) { return std::pair<float, float> { sine (1000.0, i).first, 0.0f }; }, narrow);
    float apart = 0.0f;
    for (size_t i = 0; i < leftOnly.wet[0].size(); ++i)
        apart = std::max (apart, std::abs (leftOnly.wet[0][i] - leftOnly.wet[1][i]));
    CHECK (apart == 0.0f);
}

TEST_CASE ("CPU: NUCLEARSPRING, a full tank", "[nuclearspring][cpu]")
{
    auto s = tank (0.7f);
    s.boing = 0.6f;
    s.decay = 0.8f;
    const auto started = std::chrono::steady_clock::now();
    run (20.0, [] (long long i) { return sine (220.0, i); }, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("NUCLEARSPRING, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input and bookkeeping included)");
    CHECK (seconds / 20.0 * 100.0 < 6.0);
}
