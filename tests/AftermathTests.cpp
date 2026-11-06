// AFTERMATH (0.79.0): from under an open sky into a sealed concrete room. Off and SEAL 0 leave the sound untouched bit
// for bit; sealed in, the top darkens, the room rings on after the sound (longer with RING), the machinery hums at its
// pitch only while there's music, and the whole stays about as loud; back to SEAL 0 it rests untouched again.
#include "SynthTestUtils.h"

#include "dsp/effects/Aftermath.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Aftermath::Settings;

    std::array<std::vector<float>, 2> runAftermath (std::vector<float> left, std::vector<float> right, Settings s,
                                                    int changeAt = -1, Settings changed = {})
    {
        effects::Aftermath fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (1);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            juce::dsp::AudioBlock<float> block (channels, 2, n);
            fx.process (block, changeAt >= 0 && (int) start >= changeAt ? changed : s);
        }
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

    double power (const std::vector<float>& x, size_t from, size_t to)
    {
        double sum = 0.0;
        for (auto i = from; i < juce::jmin (to, x.size()); ++i)
            sum += (double) x[i] * x[i];
        return sum / (double) juce::jmax ((size_t) 1, juce::jmin (to, x.size()) - from);
    }

    double powerDb (double p) { return 10.0 * std::log10 (juce::jmax (1.0e-20, p)); }

    // Power above `hz` (a simple high-pass difference: the signal less a one-pole low-pass of it).
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

    // The strength of `hz` over [from, to) (one DFT bin).
    double strength (const std::vector<float>& x, double hz, size_t from, size_t to)
    {
        double re = 0.0, im = 0.0;
        for (auto i = from; i < to; ++i)
        {
            const auto p = juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate;
            re += x[i] * std::cos (p);
            im += x[i] * std::sin (p);
        }
        return std::sqrt (re * re + im * im) / (double) (to - from);
    }

    Settings sealed (float seal, float ring = 0.5f, float hum = 0.0f)
    {
        Settings s;
        s.on = true;
        s.seal = seal;
        s.ring = ring;
        s.hum = hum;
        return s;
    }
}

TEST_CASE ("AFTERMATH: off and at SEAL 0 the sound is untouched bit for bit", "[aftermath]")
{
    const auto l = noise (1.0, 1), r = noise (1.0, 2);
    Settings off;
    off.seal = 1.0f;
    const auto a = runAftermath (l, r, off);
    CHECK (a[0] == l);
    CHECK (a[1] == r);
    const auto b = runAftermath (l, r, sealed (0.0f, 0.8f, 1.0f));
    CHECK (b[0] == l);
    CHECK (b[1] == r);
}

TEST_CASE ("AFTERMATH: sealed in, the top of the sound darkens", "[aftermath]")
{
    const auto l = noise (2.0, 3), r = noise (2.0, 4);
    const auto open = runAftermath (l, r, sealed (0.0f));
    const auto shut = runAftermath (l, r, sealed (1.0f, 0.3f));
    const auto from = (size_t) (0.5 * sampleRate);
    const auto drop = powerDb (highPower (shut[0], 8000.0, from)) - powerDb (highPower (open[0], 8000.0, from));
    INFO ("above 8 kHz: " << drop << " dB");
    CHECK (drop < -12.0);
    // ...while music keeps about its level (a chord of saws: most of its energy below the darkening, unlike noise).
    std::vector<float> chord ((size_t) (3.0 * sampleRate), 0.0f);
    for (const auto hz : { 110.0, 164.81, 220.0, 261.63 })
        for (size_t i = 0; i < chord.size(); ++i)
        {
            const auto phase = std::fmod (hz * (double) i / sampleRate, 1.0);
            chord[i] += 0.08f * (float) (2.0 * phase - 1.0);
        }
    const auto plainChord = runAftermath (chord, chord, sealed (0.0f));
    const auto shutChord = runAftermath (chord, chord, sealed (1.0f, 0.3f));
    const auto level = powerDb (power (shutChord[0], from, chord.size()) + power (shutChord[1], from, chord.size()))
                     - powerDb (power (plainChord[0], from, chord.size()) + power (plainChord[1], from, chord.size()));
    INFO ("a chord's level " << level << " dB");
    CHECK (std::abs (level) < 2.0);
}

TEST_CASE ("AFTERMATH: the room rings on after the sound stops, longer with RING", "[aftermath]")
{
    // A 50 ms burst, then silence.
    auto burst = noise (1.5, 5, 0.3f);
    std::fill (burst.begin() + (int) (0.05 * sampleRate), burst.end(), 0.0f);
    const auto tail = [&] (float ring)
    {
        const auto out = runAftermath (burst, burst, sealed (1.0f, ring));
        return powerDb (power (out[0], (size_t) (0.25 * sampleRate), (size_t) (0.45 * sampleRate)));
    };
    const auto open = runAftermath (burst, burst, sealed (0.0f));
    CHECK (power (open[0], (size_t) (0.25 * sampleRate), (size_t) (0.45 * sampleRate)) == 0.0);
    const auto shortRing = tail (0.1f), longRing = tail (1.0f);
    const auto during = powerDb (power (burst, 0, (size_t) (0.05 * sampleRate)));
    INFO ("burst " << during << " dB; 0.2-0.4 s after: RING 0.1 " << shortRing << " dB, RING 1 " << longRing << " dB");
    CHECK (longRing > during - 45.0);           // still ringing a quarter of a second on
    CHECK (longRing > shortRing + 15.0);        // and much longer with RING up
    CHECK (effects::Aftermath::ringSeconds (0.4f, 1.0f) > effects::Aftermath::ringSeconds (0.4f, 0.0f) * 4.0f);
}

TEST_CASE ("AFTERMATH: the machinery hums at its pitch while there's music, and silence stays silent", "[aftermath]")
{
    const auto tone = sine (1000.0, 2.0);
    for (const auto hz : { 50, 60 })
    {
        auto s = sealed (1.0f, 0.5f, 1.0f);
        s.humHz = hz == 50 ? 0 : 1;
        const auto out = runAftermath (tone, tone, s);
        const auto from = (size_t) (0.5 * sampleRate), to = (size_t) (2.0 * sampleRate);
        const auto on = strength (out[0], hz, from, to), other = strength (out[0], hz == 50 ? 60.0 : 50.0, from, to);
        INFO (hz << " Hz: " << 20.0 * std::log10 (on) << " dB, the other " << 20.0 * std::log10 (other) << " dB");
        CHECK (on > 1.0e-3);           // about -45 dB under full scale at HUM 1 (-30 dB)
        CHECK (on > 10.0 * other);     // at its own pitch
    }
    std::vector<float> silence ((size_t) (2.0 * sampleRate), 0.0f);
    const auto quiet = runAftermath (silence, silence, sealed (1.0f, 1.0f, 1.0f));
    CHECK (std::all_of (quiet[0].begin(), quiet[0].end(), [] (float v) { return v == 0.0f; }));
    CHECK (std::all_of (quiet[1].begin(), quiet[1].end(), [] (float v) { return v == 0.0f; }));
}

TEST_CASE ("AFTERMATH: SEAL turned up and back down returns to the untouched sound bit for bit", "[aftermath]")
{
    const auto l = noise (4.0, 6), r = noise (4.0, 7);
    const auto out = runAftermath (l, r, sealed (1.0f, 0.6f, 0.5f), (int) (1.0 * sampleRate), sealed (0.0f, 0.6f, 0.5f));
    // Settled well before 3 s (the seal glides 30 ms, the gain back to 1, then 0.3 s quiet).
    const auto from = (size_t) (3.0 * sampleRate);
    CHECK (std::equal (out[0].begin() + (long) from, out[0].end(), l.begin() + (long) from));
    CHECK (std::equal (out[1].begin() + (long) from, out[1].end(), r.begin() + (long) from));
    // And no click where it seals in: on a smooth tone, no step much bigger than the tone's own.
    const auto tone = sine (220.0, 2.0);
    const auto toned = runAftermath (tone, tone, sealed (0.0f), (int) (1.0 * sampleRate), sealed (1.0f, 0.6f, 0.5f));
    auto biggest = 0.0f;
    for (size_t i = 1; i < toned[0].size(); ++i)
        biggest = juce::jmax (biggest, std::abs (toned[0][i] - toned[0][i - 1]));
    INFO ("biggest step " << biggest << " (the tone's own about 0.009)");
    CHECK (biggest < 0.03f);
}

TEST_CASE ("AFTERMATH: CPU", "[aftermath][cpu]")
{
    const auto l = noise (10.0, 8), r = noise (10.0, 9);
    const auto start = std::chrono::steady_clock::now();
    runAftermath (l, r, sealed (1.0f, 0.7f, 0.6f));
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const auto percent = 100.0 * seconds / 10.0;
    WARN ("AFTERMATH sealed: " << percent << "% of a core");
    CHECK (percent < 10.0);
}
