// CANYON (0.84.0): the sound's echoes off rock walls. Off and AMOUNT 0 leave the sound untouched bit for bit; a click
// comes back from each near wall when SIZE says, from its own side, and from the far wall when the song's tempo says;
// REPEATS sets how many replies come back; TONE darkens them; turned down, the echoes ring out and then it rests,
// untouched again; silence stays silent.
#include "SynthTestUtils.h"

#include "dsp/effects/Canyon.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Canyon::Settings;

    struct Run
    {
        std::array<std::vector<float>, 2> out;
        bool resting = false;
    };

    Run runCanyon (std::vector<float> left, std::vector<float> right, Settings s, double bpm = 120.0,
                   int changeAt = -1, Settings changed = {})
    {
        effects::Canyon fx;
        fx.prepare (sampleRate, blockSize);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            juce::dsp::AudioBlock<float> block (channels, 2, n);
            fx.process (block, changeAt >= 0 && (int) start >= changeAt ? changed : s, bpm);
        }
        return { { left, right }, fx.isResting() };
    }

    // A short click (a 2 ms burst) at the start, then silence.
    std::vector<float> click (double seconds)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate), 0.0f);
        juce::Random random (3);
        for (size_t i = 0; i < (size_t) (0.002 * sampleRate); ++i)
            x[i] = random.nextFloat() * 1.6f - 0.8f;
        return x;
    }

    double energy (const std::vector<float>& x, double fromSeconds, double toSeconds)
    {
        double e = 0.0;
        for (auto i = (size_t) (fromSeconds * sampleRate); i < juce::jmin (x.size(), (size_t) (toSeconds * sampleRate)); ++i)
            e += (double) x[i] * x[i];
        return e;
    }

    // Where in [from, to] seconds the loudest 1 ms is.
    double loudestAt (const std::vector<float>& x, double fromSeconds, double toSeconds)
    {
        const auto window = (size_t) (0.001 * sampleRate);
        double best = -1.0, at = fromSeconds;
        for (auto i = (size_t) (fromSeconds * sampleRate); i + window < juce::jmin (x.size(), (size_t) (toSeconds * sampleRate)); i += window / 4)
        {
            double e = 0.0;
            for (size_t k = 0; k < window; ++k)
                e += (double) x[i + k] * x[i + k];
            if (e > best)
            {
                best = e;
                at = (double) i / sampleRate;
            }
        }
        return at;
    }

    double powerDb (double ratio) { return 10.0 * std::log10 (ratio + 1.0e-30); }
}

TEST_CASE ("CANYON: off and AMOUNT 0 leave the sound untouched, bit for bit", "[canyon]")
{
    const auto input = click (2.0);
    Settings off;
    off.amount = 1.0f;
    const auto a = runCanyon (input, input, off);
    CHECK (a.out[0] == input);
    CHECK (a.resting);
    Settings zero;
    zero.on = true;
    zero.amount = 0.0f;
    const auto b = runCanyon (input, input, zero);
    CHECK (b.out[0] == input);
    CHECK (b.out[1] == input);
    CHECK (b.resting);
}

TEST_CASE ("CANYON: the near walls answer when SIZE says, the first from the left, the second from the right", "[canyon]")
{
    for (const auto size : { 0.0f, 0.5f, 1.0f })
    {
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.size = size;
        s.repeats = 0.0f;
        s.width = 1.0f;
        s.far = 3;   // two bars: far out of the way
        const auto input = click (1.0);
        const auto run = runCanyon (input, input, s);
        const auto first = (double) effects::Canyon::nearSeconds (size);
        const auto heard = loudestAt (run.out[0], 0.02, first * 1.25);
        INFO ("SIZE " << size << ": first wall at " << first << " s, loudest left at " << heard << " s");
        CHECK (std::abs (heard - first) < 0.004);   // the wall's lowpass and rock smear it a few ms at most
        // The first wall on the left, the second on the right.
        const auto firstLeft = energy (run.out[0], first - 0.002, first + 0.012), firstRight = energy (run.out[1], first - 0.002, first + 0.012);
        const auto second = first * 1.53;
        const auto secondLeft = energy (run.out[0], second - 0.002, second + 0.012), secondRight = energy (run.out[1], second - 0.002, second + 0.012);
        INFO ("first wall L/R " << powerDb (firstLeft / firstRight) << " dB, second " << powerDb (secondRight / secondLeft) << " dB R over L");
        CHECK (powerDb (firstLeft / firstRight) > 10.0);
        CHECK (powerDb (secondRight / secondLeft) > 6.0);
    }
}

TEST_CASE ("CANYON: the far wall replies in time with the song, and REPEATS brings more back", "[canyon]")
{
    for (const auto bpm : { 120.0, 90.0 })
    {
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.size = 0.0f;
        s.far = 1;   // a half note: 2 beats
        s.repeats = 1.0f;
        const auto input = click (6.0);
        const auto run = runCanyon (input, input, s, bpm);
        const auto farAt = 2.0 * 60.0 / bpm;
        const auto heard = loudestAt (run.out[0], farAt - 0.2, farAt + 0.2);
        INFO (bpm << " BPM: far wall at " << farAt << " s, heard at " << heard << " s");
        CHECK (std::abs (heard - farAt) < 0.01);
        // The second reply comes from the other side, one more distance on.
        const auto replyLeft = energy (run.out[0], farAt - 0.01, farAt + 0.05), replyRight = energy (run.out[1], farAt - 0.01, farAt + 0.05);
        const auto backLeft = energy (run.out[0], 2.0 * farAt - 0.01, 2.0 * farAt + 0.05), backRight = energy (run.out[1], 2.0 * farAt - 0.01, 2.0 * farAt + 0.05);
        INFO ("first reply L over R " << powerDb (replyLeft / replyRight) << " dB, second R over L " << powerDb (backRight / backLeft) << " dB");
        CHECK (replyLeft > replyRight);
        CHECK (backRight > backLeft);
    }

    // REPEATS 0: one reply; at full, the third still comes back.
    const auto input = click (6.0);
    Settings once;
    once.on = true;
    once.amount = 1.0f;
    once.far = 1;
    once.repeats = 0.0f;
    auto many = once;
    many.repeats = 1.0f;
    const auto a = runCanyon (input, input, once), b = runCanyon (input, input, many);
    const auto third = [] (const Run& r) { return energy (r.out[0], 2.95, 3.1) + energy (r.out[1], 2.95, 3.1); };
    INFO ("third reply: REPEATS 0 " << powerDb (third (a)) << " dB, REPEATS 1 " << powerDb (third (b)) << " dB");
    CHECK (third (b) > 1000.0 * (third (a) + 1.0e-12));
}

TEST_CASE ("CANYON: TONE darkens the echoes; the sound itself passes unchanged", "[canyon]")
{
    // White noise for 50 ms: compare what comes back above 4 kHz.
    std::vector<float> input ((size_t) (2.0 * sampleRate), 0.0f);
    juce::Random random (5);
    for (size_t i = 0; i < (size_t) (0.05 * sampleRate); ++i)
        input[i] = random.nextFloat() * 0.6f - 0.3f;
    const auto bright = [&] (float tone)
    {
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.size = 0.5f;
        s.tone = tone;
        s.far = 3;
        const auto run = runCanyon (input, input, s);
        // The echoes alone: what's out less what went in; then a crude high-pass (first difference) for the top.
        double top = 0.0, all = 0.0;
        for (size_t i = 1; i < run.out[0].size(); ++i)
        {
            const auto e = run.out[0][i] - input[i], ePrev = run.out[0][i - 1] - input[i - 1];
            top += (double) (e - ePrev) * (e - ePrev);
            all += (double) e * e;
        }
        return top / all;
    };
    const auto dark = bright (0.0f), light = bright (1.0f);
    INFO ("top share: TONE 0 " << powerDb (dark) << " dB, TONE 1 " << powerDb (light) << " dB");
    CHECK (light > 3.0 * dark);

    // The dry part is unchanged: before the first echo, out equals in.
    Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.size = 1.0f;
    const auto run = runCanyon (input, input, s);
    CHECK (std::equal (run.out[0].begin(), run.out[0].begin() + (long) (0.15 * sampleRate), input.begin()));
}

TEST_CASE ("CANYON: turned down, the echoes ring out, then it rests untouched; silence stays silent", "[canyon]")
{
    // A tone for 1 s, AMOUNT up, then down to 0 after 1 s: the replies already out keep coming for a while.
    std::vector<float> input ((size_t) (16.0 * sampleRate), 0.0f);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = 0.2f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 330.0 * (double) i / sampleRate);
    Settings up;
    up.on = true;
    up.amount = 1.0f;
    up.repeats = 0.4f;   // each reply about 11 dB down on the last
    auto down = up;
    down.amount = 0.0f;
    const auto run = runCanyon (input, input, up, 120.0, (int) (1.0 * sampleRate), down);
    // Just after: still echoes on top of the tone.
    double after = 0.0;
    for (auto i = (size_t) (1.6 * sampleRate); i < (size_t) (2.0 * sampleRate); ++i)
        after += std::abs (run.out[0][i] - input[i]);
    CHECK (after > 1.0);
    // Long after: untouched again, bit for bit, and resting.
    CHECK (run.resting);
    CHECK (std::equal (run.out[0].begin() + (long) (15.0 * sampleRate), run.out[0].end(), input.begin() + (long) (15.0 * sampleRate)));

    Settings s;
    s.on = true;
    s.amount = 1.0f;
    const std::vector<float> silence ((size_t) (2.0 * sampleRate), 0.0f);
    const auto quiet = runCanyon (silence, silence, s);
    CHECK (quiet.out[0] == silence);
    CHECK (quiet.out[1] == silence);
}

TEST_CASE ("CANYON: CPU", "[canyon][cpu]")
{
    std::vector<float> input ((size_t) (10.0 * sampleRate));
    juce::Random random (7);
    for (auto& x : input)
        x = random.nextFloat() * 0.4f - 0.2f;
    Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.repeats = 1.0f;
    const auto t0 = std::chrono::steady_clock::now();
    runCanyon (input, input, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    WARN ("CANYON at full: " << 100.0 * seconds / 10.0 << "% of a core");
    CHECK (seconds / 10.0 < 0.05);
}
