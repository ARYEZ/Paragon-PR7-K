// DOWNPOUR (0.57.0): rain on the glass. Checked by what it does to the sound: the patter's count and randomness,
// the drops landing on the notes sounding (their pitch in the spectrum), RAIN making it louder, DISTANCE darker,
// WIDTH, LEVEL, FOLLOW (silence in is silence out, and it ends at exactly zero), off, CPU.
#include "dsp/effects/Downpour.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <numeric>
#include <vector>

namespace
{
    constexpr double rate = 48000.0;
    constexpr int block = 256;

    struct Run
    {
        std::vector<float> left, right;
        std::vector<float> dropNotes;
        long long patter = 0, drops = 0;
    };

    // `seconds` of rain over `input` (the level of a 220 Hz sine coming in; 0 silence), notes `notes` sounding.
    Run run (effects::Downpour::Settings s, double seconds, float input = 0.5f, std::vector<float> notes = { 69.0f }, std::uint32_t seed = 7)
    {
        effects::Downpour d;
        d.prepare (rate, block);
        d.seedRandom (seed);
        d.setNotes (notes.data(), (int) notes.size());
        Run out;
        // Plain vectors, not AudioBuffers (whose "cleared" flag let a clear() be skipped: the FOLLOW test's tail).
        std::vector<float> inL ((size_t) block), inR ((size_t) block), wetL ((size_t) block), wetR ((size_t) block);
        float* inPtrs[] = { inL.data(), inR.data() };
        float* wetPtrs[] = { wetL.data(), wetR.data() };
        long long t = 0;
        std::array<float, 256> taken {};
        for (int b = 0; b < (int) (seconds * rate / block); ++b)
        {
            for (size_t i = 0; i < (size_t) block; ++i)
                inL[i] = inR[i] = input * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) (t + (long long) i) / rate);
            std::fill (wetL.begin(), wetL.end(), 0.0f);
            std::fill (wetR.begin(), wetR.end(), 0.0f);
            d.render (juce::dsp::AudioBlock<const float> (inPtrs, 2, (size_t) block), s);
            d.addTo (juce::dsp::AudioBlock<float> (wetPtrs, 2, (size_t) block));
            const auto k = d.takeDropNotes (taken.data(), (int) taken.size());
            out.dropNotes.insert (out.dropNotes.end(), taken.begin(), taken.begin() + k);
            out.left.insert (out.left.end(), wetL.begin(), wetL.end());
            out.right.insert (out.right.end(), wetR.begin(), wetR.end());
            t += block;
        }
        out.patter = d.getTotalPatter();
        out.drops = d.getTotalDrops();
        return out;
    }

    effects::Downpour::Settings on()
    {
        effects::Downpour::Settings s;
        s.on = true;
        return s;
    }

    double rmsDb (const std::vector<float>& x)
    {
        double sum = 0.0;
        for (const auto v : x)
            sum += (double) v * v;
        return 10.0 * std::log10 (sum / (double) x.size() + 1.0e-30);
    }

    // The power spectrum of the left side, 2^14-point frames summed; bin k is k x rate / 16384 Hz.
    std::vector<double> spectrum (const std::vector<float>& x)
    {
        juce::dsp::FFT fft (14);
        std::vector<float> data ((size_t) 2 << 14, 0.0f);
        std::vector<double> sum ((size_t) 1 << 13, 0.0);
        for (size_t start = 0; start + (1u << 14) <= x.size(); start += 1u << 14)
        {
            std::fill (data.begin(), data.end(), 0.0f);
            for (size_t i = 0; i < (1u << 14); ++i)   // Hann
                data[i] = x[start + i] * (float) (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * (double) i / (1 << 14)));
            fft.performFrequencyOnlyForwardTransform (data.data());
            for (size_t k = 0; k < sum.size(); ++k)
                sum[k] += (double) data[k] * data[k];
        }
        return sum;
    }

    double centre (const std::vector<float>& x)
    {
        const auto s = spectrum (x);
        double num = 0.0, den = 0.0;
        for (size_t k = 1; k < s.size(); ++k)
        {
            num += s[k] * (double) k * rate / (1 << 14);
            den += s[k];
        }
        return num / den;
    }
}

TEST_CASE ("DOWNPOUR: off makes nothing; on, RAIN sets the patter's count, falling at random", "[downpour]")
{
    effects::Downpour::Settings off;
    const auto none = run (off, 2.0);
    CHECK (*std::max_element (none.left.begin(), none.left.end()) == 0.0f);
    CHECK (none.patter == 0);

    auto s = on();
    s.follow = 0.0f;
    s.rain = 0.3f;   // 360 a second
    CHECK (effects::Downpour::patterRate (0.0f) == 0.0f);
    CHECK (effects::Downpour::dropRate (0.0f, 1.0f) == 0.0f);
    CHECK (std::abs (effects::Downpour::patterRate (0.1f) - 40.0f) < 0.1f);
    CHECK (std::abs (effects::Downpour::patterRate (0.5f) - 1000.0f) < 1.0f);
    CHECK (std::abs (effects::Downpour::patterRate (1.0f) - 4000.0f) < 1.0f);
    const auto r = run (s, 20.0);
    INFO ("patter in 20 s: " << r.patter << " (7200 expected)");
    CHECK (std::abs ((double) r.patter - 7200.0) < 7200.0 * 0.06);

    // RAIN 0: on, but no rain at all (nothing made: exact silence, so a preset with it at 0 is unchanged).
    s.rain = 0.0f;
    s.drops = 1.0f;
    const auto dry = run (s, 4.0);
    CHECK (dry.patter == 0);
    CHECK (dry.drops == 0);
    CHECK (*std::max_element (dry.left.begin(), dry.left.end()) == 0.0f);
}

TEST_CASE ("DOWNPOUR: the drops ring on the notes sounding, OCTAVE up", "[downpour]")
{
    auto s = on();
    s.follow = 0.0f;
    s.rain = 1.0f;
    s.drops = 1.0f;   // 12 a second
    s.octave = 1;     // +2 octaves
    const auto chord = run (s, 20.0, 0.5f, { 60.0f, 64.0f, 67.0f });
    INFO ("drops in 20 s: " << chord.drops << " (240 expected)");
    CHECK (std::abs ((double) chord.drops - 240.0) < 50.0);
    int each[3] {};
    for (const auto n : chord.dropNotes)
    {
        // C, E or G two octaves up, now and then three.
        const auto ok = n == 84.0f || n == 88.0f || n == 91.0f || n == 96.0f || n == 100.0f || n == 103.0f;
        CHECK (ok);
        each[n == 84.0f || n == 96.0f ? 0 : n == 88.0f || n == 100.0f ? 1 : 2]++;
    }
    INFO ("C " << each[0] << ", E " << each[1] << ", G " << each[2]);
    for (const auto c : each)
        CHECK (c > (int) chord.dropNotes.size() / 6);   // every note of the chord gets its share

    // In the sound: one note (A4, so A6 at 1760 Hz): the drops' energy is at 1760 (or 3520), not two semitones under
    // (where each starts) nor anywhere else. Little patter (RAIN 0.05: 10 taps a second, 2.7 drops) so the drops are
    // what's heard.
    s.rain = 0.05f;
    s.distance = 0.0f;
    const auto a = run (s, 30.0, 0.5f, { 69.0f });
    const auto spec = spectrum (a.left);
    const auto band = [&spec] (double lo, double hi)
    {
        double e = 0.0;
        for (auto k = (size_t) (lo * (1 << 14) / rate); k <= (size_t) (hi * (1 << 14) / rate); ++k)
            e += spec[k];
        return e;
    };
    const auto total = band (100.0, 20000.0);
    const auto onNote = band (1700.0, 1820.0) + band (3400.0, 3640.0);
    const auto under = band (1540.0, 1600.0);   // two semitones under A6: 1568 Hz
    INFO ("share of the drops' energy on A6 / A7: " << onNote / total << ", two semitones under: " << under / total);
    CHECK (onNote / total > 0.6);
    CHECK (under / total < 0.05);
}

TEST_CASE ("DOWNPOUR: RAIN louder, DISTANCE darker, WIDTH spread, LEVEL in dB", "[downpour]")
{
    auto s = on();
    s.follow = 0.0f;
    s.drops = 0.4f;
    s.rain = 0.1f;   // a few spots
    const auto drizzle = rmsDb (run (s, 10.0).left);
    s.rain = 1.0f;
    const auto storm = rmsDb (run (s, 10.0).left);
    INFO ("drizzle " << drizzle << " dB, storm " << storm << " dB");
    CHECK (storm - drizzle > 10.0);

    s.distance = 0.0f;
    s.drops = 0.0f;   // the patter alone (the drops' own notes would pull the centre)
    const auto close = centre (run (s, 6.0).left);
    s.distance = 1.0f;
    const auto far = centre (run (s, 6.0).left);
    s.drops = 0.4f;
    INFO ("spectral centre: on the glass " << close << " Hz, in the street " << far << " Hz");
    CHECK (far < close * 0.6);

    s.distance = 0.3f;
    s.width = 0.0f;
    const auto centred = run (s, 4.0);
    s.width = 1.0f;
    const auto wide = run (s, 4.0);
    const auto correlation = [] (const Run& r)
    {
        double ll = 0.0, rr = 0.0, lr = 0.0;
        for (size_t i = 0; i < r.left.size(); ++i)
        {
            ll += (double) r.left[i] * r.left[i];
            rr += (double) r.right[i] * r.right[i];
            lr += (double) r.left[i] * r.right[i];
        }
        return lr / std::sqrt (ll * rr);
    };
    INFO ("left / right correlation: WIDTH 0 " << correlation (centred) << ", WIDTH 1 " << correlation (wide));
    CHECK (correlation (centred) > 0.999);
    CHECK (correlation (wide) < 0.7);

    s.levelDb = -12.0f;
    const auto loud = rmsDb (run (s, 4.0).left);
    s.levelDb = -24.0f;
    const auto quiet = rmsDb (run (s, 4.0).left);
    CHECK (std::abs ((loud - quiet) - 12.0) < 0.01);
}

TEST_CASE ("DOWNPOUR: FOLLOW rains only while the sound plays, and ends in exact silence", "[downpour]")
{
    auto s = on();
    s.rain = 0.8f;
    s.drops = 0.8f;
    const auto quiet = run (s, 4.0, 0.0f);
    CHECK (quiet.patter == 0);
    CHECK (quiet.drops == 0);
    CHECK (*std::max_element (quiet.left.begin(), quiet.left.end()) == 0.0f);

    // A sound for a second, then silence: the rain thins out with it (FOLLOW's 150 ms release, from full) and, within two
    // seconds, the output is exactly zero.
    effects::Downpour d;
    d.prepare (rate, block);
    d.seedRandom (3);
    const float note = 64.0f;
    d.setNotes (&note, 1);
    std::vector<float> inL ((size_t) block), inR ((size_t) block), wetL ((size_t) block), wetR ((size_t) block);
    float* inPtrs[] = { inL.data(), inR.data() };
    float* wetPtrs[] = { wetL.data(), wetR.data() };
    const auto magnitude = [] (const std::vector<float>& x)
    {
        float m = 0.0f;
        for (const auto v : x)
            m = juce::jmax (m, std::abs (v));
        return m;
    };
    float during = 0.0f, tail = 0.0f;
    for (int b = 0; b < (int) (4.0 * rate / block); ++b)
    {
        const auto playing = b * block < (int) rate;
        for (size_t i = 0; i < (size_t) block; ++i)
            inL[i] = inR[i] = playing ? 0.4f * (float) std::sin (0.03 * (double) ((size_t) b * block + i)) : 0.0f;
        std::fill (wetL.begin(), wetL.end(), 0.0f);
        std::fill (wetR.begin(), wetR.end(), 0.0f);
        d.render (juce::dsp::AudioBlock<const float> (inPtrs, 2, (size_t) block), s);
        d.addTo (juce::dsp::AudioBlock<float> (wetPtrs, 2, (size_t) block));
        if (playing)
            during = juce::jmax (during, magnitude (wetL));
        if (b * block > 3.0 * rate)
            tail = juce::jmax (tail, magnitude (wetL), magnitude (wetR));
    }
    INFO ("loudest while playing " << during << ", after " << tail << ", patter now " << d.getCurrentPatterRate() << ", drops now " << d.getCurrentDropRate());
    CHECK (during > 0.01f);
    CHECK (tail == 0.0f);
    CHECK (d.isSilent());
}

TEST_CASE ("CPU: DOWNPOUR at a storm", "[downpour][cpu]")
{
    auto s = on();
    s.follow = 0.0f;
    s.rain = 1.0f;
    s.drops = 1.0f;
    const auto started = std::chrono::steady_clock::now();
    run (s, 20.0, 0.5f, { 60.0f, 64.0f, 67.0f });
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    const auto load = seconds / 20.0 * 100.0;
    WARN ("DOWNPOUR at full RAIN and DROPS, stereo: " << load << "% of a core (the test's own sine and copying included)");
    CHECK (load < 5.0);
}
