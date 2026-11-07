// COLOSSUS (0.82.0): the sound made enormous. Off and SIZE 0 leave the sound untouched bit for bit; at full the sound
// gains its octave under, driven weight, a vast hall that rings on, and a boom under each new note (none in silence),
// its level kept; back to 0 it rests untouched again.
#include "SynthTestUtils.h"

#include "dsp/effects/Colossus.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Colossus::Settings;

    std::array<std::vector<float>, 2> runColossus (std::vector<float> left, std::vector<float> right, Settings s,
                                                   int changeAt = -1, Settings changed = {}, int* booms = nullptr,
                                                   std::vector<size_t> notesAt = {})
    {
        effects::Colossus fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (1);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            for (const auto note : notesAt)   // the synth's key presses
                if (note >= start && note < start + n)
                    fx.addNote ((int) (note - start), 0.9f);
            float* channels[] = { left.data() + start, right.data() + start };
            juce::dsp::AudioBlock<float> block (channels, 2, n);
            fx.process (block, changeAt >= 0 && (int) start >= changeAt ? changed : s);
        }
        if (booms != nullptr)
            *booms = fx.getBoomsStarted();
        return { left, right };
    }

    std::vector<float> sine (double hz, double seconds, float amplitude = 0.3f, double silentAfter = 1.0e9)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = (double) i / sampleRate < silentAfter ? amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate) : 0.0f;
        return x;
    }

    std::vector<float> noise (double seconds, int seed, float amplitude = 0.2f)
    {
        juce::Random random (seed);
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (auto& v : x)
            v = amplitude * (2.0f * random.nextFloat() - 1.0f);
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

    double strength (const std::vector<float>& x, double hz, size_t from, size_t to)
    {
        double re = 0.0, im = 0.0;
        for (auto i = from; i < to; ++i)
        {
            const auto w = juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate;
            re += x[i] * std::cos (w);
            im += x[i] * std::sin (w);
        }
        return 2.0 * std::sqrt (re * re + im * im) / (double) (to - from);
    }

    Settings big (float size = 1.0f)
    {
        Settings s;
        s.on = true;
        s.size = size;
        s.depth = 1.0f;
        s.weight = 0.0f;
        s.hall = 0.0f;
        s.boom = 0.0f;
        return s;
    }

    size_t at (double seconds) { return (size_t) (seconds * sampleRate); }
}

TEST_CASE ("COLOSSUS: off and SIZE 0 leave the sound untouched bit for bit", "[colossus]")
{
    const auto left = noise (1.0, 1), right = noise (1.0, 2);
    auto off = big();
    off.on = false;
    const auto a = runColossus (left, right, off);
    CHECK (a[0] == left);
    CHECK (a[1] == right);
    const auto b = runColossus (left, right, big (0.0f));
    CHECK (b[0] == left);
    CHECK (b[1] == right);
    const auto c = runColossus (left, right, big (0.0005f));   // under 0.001 counts as 0 (the matrix eases down)
    CHECK (c[0] == left);
}

TEST_CASE ("COLOSSUS: the sound gains its octave under, as loud as DEPTH says", "[colossus]")
{
    const auto tone = sine (220.0, 2.0);
    const auto from = at (1.0), to = at (1.5);
    auto s = big();
    const auto deep = runColossus (tone, tone, s);
    s.depth = 0.0f;
    const auto none = runColossus (tone, tone, s);
    const auto octave = strength (deep[0], 110.0, from, to) / strength (deep[0], 220.0, from, to);
    const auto without = strength (none[0], 110.0, from, to) / strength (none[0], 220.0, from, to);
    INFO ("110 Hz against 220 Hz: DEPTH 1 " << octave << ", DEPTH 0 " << without);
    CHECK (octave > 0.4);
    CHECK (without < 0.05);
}

TEST_CASE ("COLOSSUS: WEIGHT drives it (harmonics up), its level kept", "[colossus]")
{
    const auto tone = sine (220.0, 2.0, 0.4f);
    const auto from = at (1.0), to = at (1.5);
    auto s = big();
    s.depth = 0.0f;
    s.weight = 1.0f;
    const auto heavy = runColossus (tone, tone, s);
    const auto third = strength (heavy[0], 660.0, from, to) / strength (heavy[0], 220.0, from, to);
    INFO ("3rd harmonic against the fundamental: " << third);
    CHECK (third > 0.05);
    CHECK (std::abs (powerDb (power (heavy[0], from, to)) - powerDb (power (tone, from, to))) < 1.5);
}

TEST_CASE ("COLOSSUS: at full the whole stays about as loud (the hall added on top)", "[colossus]")
{
    const auto music = sawChord (6.0);
    const auto from = at (2.0), to = at (6.0);
    for (const auto size : { 0.5f, 1.0f })
    {
        Settings s;   // the defaults: depth, weight, hall, boom, tone, width
        s.on = true;
        s.size = size;
        const auto out = runColossus (music, music, s);
        const auto difference = powerDb (power (out[0], from, to)) - powerDb (power (music, from, to));
        INFO ("SIZE " << size << ": " << difference << " dB");
        CHECK (difference > -2.0);
        CHECK (difference < 4.0);
    }
}

TEST_CASE ("COLOSSUS: the hall rings on after the sound stops, as long as HALL says", "[colossus]")
{
    const auto tone = sine (330.0, 6.0, 0.3f, 1.0);
    auto s = big();
    s.depth = 0.0f;
    s.hall = 0.0f;   // 2 s
    const auto shortHall = runColossus (tone, tone, s);
    s.hall = 1.0f;   // 10 s
    const auto longHall = runColossus (tone, tone, s);
    const auto held = powerDb (power (longHall[0], at (0.5), at (0.9)));
    const auto shortLater = powerDb (power (shortHall[0], at (3.0), at (3.5)));
    const auto longLater = powerDb (power (longHall[0], at (3.0), at (3.5)));
    INFO ("held " << held << " dB; 2 s after the end: 2 s hall " << shortLater << ", 10 s hall " << longLater);
    CHECK (longLater > held - 40.0);
    CHECK (longLater > shortLater + 15.0);
}

TEST_CASE ("COLOSSUS: a boom under each new note played (one for a chord), none in silence", "[colossus]")
{
    // Four notes half a second apart, each 0.3 s long.
    std::vector<float> notes ((size_t) (2.5 * sampleRate), 0.0f);
    for (int k = 0; k < 4; ++k)
        for (size_t i = at (0.1 + 0.5 * k); i < at (0.4 + 0.5 * k); ++i)
            notes[i] = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (double) i / sampleRate);
    auto s = big();
    s.depth = 0.0f;
    s.boom = 1.0f;
    int count = 0;
    // The four notes pressed, a chord's three notes together on the second (one boom for them).
    const std::vector<size_t> presses { at (0.1), at (0.6), at (0.6) + 10, at (0.6) + 20, at (1.1), at (1.6) };
    const auto out = runColossus (notes, notes, s, -1, {}, &count, presses);
    INFO ("booms: " << count);
    CHECK (count == 4);
    // A held sound whose level wobbles sets none off: only notes played do.
    std::vector<float> wobble ((size_t) (2.0 * sampleRate));
    for (size_t i = 0; i < wobble.size(); ++i)
        wobble[i] = 0.3f * (0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 6.0 * (double) i / sampleRate))
                  * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) i / sampleRate);
    int wobbleBooms = -1;
    runColossus (wobble, wobble, s, -1, {}, &wobbleBooms, { 0 });
    CHECK (wobbleBooms == 1);
    // The boom is low: more 50 Hz just after a note starts than in the plain notes.
    CHECK (strength (out[0], 50.0, at (0.1), at (0.3)) > 5.0 * strength (notes, 50.0, at (0.1), at (0.3)) + 1.0e-4);

    const std::vector<float> silence ((size_t) (3.0 * sampleRate), 0.0f);
    int none = -1;
    auto loud = big();
    loud.boom = 1.0f;
    loud.hall = 1.0f;
    const auto quiet = runColossus (silence, silence, loud, -1, {}, &none);
    CHECK (none == 0);
    CHECK (std::all_of (quiet[0].begin(), quiet[0].end(), [] (float v) { return v == 0.0f; }));
}

TEST_CASE ("COLOSSUS: back to SIZE 0, it rests untouched again", "[colossus]")
{
    const auto left = noise (4.0, 6), right = noise (4.0, 7);
    Settings s;
    s.on = true;
    s.size = 1.0f;
    s.boom = 1.0f;
    const auto out = runColossus (left, right, s, (int) (1.0 * sampleRate), big (0.0f));
    const auto late = at (3.0);
    CHECK (std::equal (out[0].begin() + (long) late, out[0].end(), left.begin() + (long) late));
    CHECK (std::equal (out[1].begin() + (long) late, out[1].end(), right.begin() + (long) late));
}

TEST_CASE ("COLOSSUS: CPU", "[colossus][cpu]")
{
    const auto music = sawChord (10.0);
    Settings s;
    s.on = true;
    s.size = 1.0f;
    s.boom = 1.0f;
    const auto t0 = std::chrono::steady_clock::now();
    runColossus (music, music, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const auto share = 100.0 * seconds / 10.0;
    WARN ("COLOSSUS at full: " << share << "% of a core");
    CHECK (share < 10.0);
}
