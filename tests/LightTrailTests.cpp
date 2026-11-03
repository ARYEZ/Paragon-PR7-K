// LIGHTTRAIL (0.61.0): every note's trail. Checked by what it does to the sound: off and MIX 0 leave it exactly as it
// was; the repeats come at the division in time with the song, each quieter by LENGTH; RISE moves each repeat's pitch
// by its semitones; SPREAD at full puts each repeat on the other side; the trail dies away to silence; CPU.
#include "SynthTestUtils.h"

#include "dsp/effects/LightTrail.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    constexpr double bpm = 120.0;   // a sixteenth: 0.125 s, 6000 samples

    template <typename Make>
    std::array<std::vector<float>, 2> run (effects::LightTrail::Settings s, double seconds, Make make)
    {
        effects::LightTrail fx;
        constexpr int block = 256;
        fx.prepare (sampleRate, block);
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
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) block), s, bpm);
            out[0].insert (out[0].end(), l.begin(), l.end());
            out[1].insert (out[1].end(), r.begin(), r.end());
            t += block;
        }
        return out;
    }

    effects::LightTrail::Settings trail (float rise = 0.0f, float spread = 0.0f)
    {
        effects::LightTrail::Settings s;
        s.on = true;
        s.mix = 1.0f;
        s.length = 0.6f;
        s.tone = 1.0f;
        s.riseSemitones = rise;
        s.spread = spread;
        return s;
    }

    // The loudest |sample| in [from, to) of x.
    float peakIn (const std::vector<float>& x, size_t from, size_t to)
    {
        auto p = 0.0f;
        for (auto i = from; i < to && i < x.size(); ++i)
            p = std::max (p, std::abs (x[i]));
        return p;
    }
}

TEST_CASE ("LIGHTTRAIL: off and MIX 0 leave the sound exactly as it was", "[lighttrail]")
{
    const auto input = [] (long long i) { const auto v = 0.3f * (float) std::sin (0.05 * (double) i); return std::pair<float, float> { v, -v }; };
    std::vector<float> expectedL, expectedR;
    const auto dry = run ({}, 1.0, input);
    for (size_t i = 0; i < dry[0].size(); ++i)
    {
        const auto [a, b] = input ((long long) i);
        expectedL.push_back (a);
        expectedR.push_back (b);
    }
    CHECK (dry[0] == expectedL);
    auto off = trail();
    off.on = false;
    CHECK (run (off, 1.0, input)[0] == expectedL);
    auto zero = trail();
    zero.mix = 0.0f;
    const auto z = run (zero, 1.0, input);
    CHECK (z[0] == expectedL);
    CHECK (z[1] == expectedR);
}

TEST_CASE ("LIGHTTRAIL: the repeats come every sixteenth at 120 BPM, each quieter by LENGTH", "[lighttrail]")
{
    // A click on the left at the start: the trail's repeats at 6000, 12000, 18000 samples.
    const auto out = run (trail(), 1.0, [] (long long i) { return std::pair<float, float> { i == 100 ? 0.8f : 0.0f, 0.0f }; });
    std::array<float, 4> peaks {};
    for (int k = 1; k <= 4; ++k)
    {
        const auto at = (size_t) (100 + k * 6000);
        peaks[(size_t) (k - 1)] = peakIn (out[0], at - 20, at + 20);
        INFO ("repeat " << k << ": " << peaks[(size_t) (k - 1)] << ", and between: " << peakIn (out[0], at + 200, at + 5800));
        CHECK (peaks[(size_t) (k - 1)] > 0.01f);
        CHECK (peakIn (out[0], at + 200, at + 5800) < peaks[(size_t) (k - 1)] * 0.05f);   // nothing between the repeats
    }
    // Each repeat about LENGTH's feedback (0.54) of the last (a little less: TONE's filter rounds the click).
    for (int k = 1; k < 4; ++k)
    {
        const auto ratio = peaks[(size_t) k] / peaks[(size_t) (k - 1)];
        INFO ("repeat " << k + 1 << " against " << k << ": " << ratio);
        CHECK (ratio > 0.3f);
        CHECK (ratio < 0.6f);
    }
}

TEST_CASE ("LIGHTTRAIL: RISE moves each repeat's pitch", "[lighttrail]")
{
    // A 440 Hz burst for one sixteenth; the first repeat (a sixteenth later) at 880 Hz with RISE +12, 440 with RISE 0.
    const auto burst = [] (long long i) {
        const auto v = i < 6000 ? 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (double) i / sampleRate) : 0.0f;
        return std::pair<float, float> { v, v };
    };
    const auto pitchOfFirstRepeat = [&burst] (float rise)
    {
        auto s = trail (rise);
        s.length = 0.3f;
        const auto out = run (s, 0.5, burst);
        const auto from = (size_t) 6000 + 1500, length = (size_t) 3000;   // the middle of the first repeat
        return std::pair<double, double> { db (levelAt (out[0], 440.0, from, length)), db (levelAt (out[0], 880.0, from, length)) };
    };
    const auto flat = pitchOfFirstRepeat (0.0f), up = pitchOfFirstRepeat (12.0f);
    INFO ("RISE 0: 440 Hz " << flat.first << " dB, 880 Hz " << flat.second << " dB; RISE +12: 440 Hz " << up.first << " dB, 880 Hz " << up.second << " dB");
    CHECK (flat.first > flat.second + 20.0);
    CHECK (up.second > up.first + 10.0);
}

TEST_CASE ("LIGHTTRAIL: SPREAD at full puts each repeat on the other side", "[lighttrail]")
{
    const auto out = run (trail (0.0f, 1.0f), 0.5, [] (long long i) { return std::pair<float, float> { i == 100 ? 0.8f : 0.0f, 0.0f }; });
    const auto first = (size_t) (100 + 6000), second = (size_t) (100 + 12000);
    INFO ("first repeat: left " << peakIn (out[0], first - 20, first + 20) << ", right " << peakIn (out[1], first - 20, first + 20)
                                << "; second: left " << peakIn (out[0], second - 20, second + 20) << ", right " << peakIn (out[1], second - 20, second + 20));
    // The click was on the left: its first repeat crosses to the right, the second back to the left.
    CHECK (peakIn (out[1], first - 20, first + 20) > 4.0f * peakIn (out[0], first - 20, first + 20));
    CHECK (peakIn (out[0], second - 20, second + 20) > 4.0f * peakIn (out[1], second - 20, second + 20));
}

TEST_CASE ("LIGHTTRAIL: the trail dies away to silence, even climbing at full LENGTH", "[lighttrail]")
{
    auto s = trail (7.0f, 0.7f);
    s.length = 1.0f;
    const auto out = run (s, 14.0, [] (long long i) {
        const auto v = i < 48000 ? 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / sampleRate) : 0.0f;
        return std::pair<float, float> { v, v };
    });
    const auto loud = peakIn (out[0], 0, (size_t) (2 * sampleRate));
    const auto tail = std::max (peakIn (out[0], (size_t) (13 * sampleRate), out[0].size()), peakIn (out[1], (size_t) (13 * sampleRate), out[1].size()));
    INFO ("while playing " << loud << "; 12 s after " << tail);
    CHECK (loud < 1.5f);         // the soft limit keeps a climbing trail from running away
    CHECK (tail < 1.0e-4f);
}

TEST_CASE ("CPU: LIGHTTRAIL, rising, every repeat crossing", "[lighttrail][cpu]")
{
    auto s = trail (7.0f, 0.8f);
    const auto started = std::chrono::steady_clock::now();
    run (s, 20.0, [] (long long i) {
        const auto v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 330.0 * (double) i / sampleRate);
        return std::pair<float, float> { v, v };
    });
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("LIGHTTRAIL, rising, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input included)");
    CHECK (seconds / 20.0 * 100.0 < 5.0);
}
