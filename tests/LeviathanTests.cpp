// LEVIATHAN (0.68.0): a vast voice beneath what you play. Checked by what it does to the sound: off and at MIX 0
// untouched; the voice (what's added) sings DEPTH below the lowest note sounding (its harmonics on that note's
// octave below, nothing between them without GROWL); it's about as loud as the sound at MIX 1; it glides to a new
// note; DELAY puts it behind; GROWL creaks (a lower shadow between the harmonics); its vowel wanders; silence stays
// silent; CPU.
#include "SynthTestUtils.h"

#include "dsp/effects/Leviathan.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Leviathan::Settings;

    struct Result
    {
        std::array<std::vector<float>, 2> out, voice;
    };

    // Runs the input through LEVIATHAN with `notes` sounding (changed to `later` at `switchAt` seconds, if given).
    template <typename Input>
    Result run (double seconds, Input input, Settings s, std::vector<float> notes, std::vector<float> later = {}, double switchAt = 1.0e9,
                effects::Leviathan* keep = nullptr)
    {
        effects::Leviathan local;
        auto& fx = keep != nullptr ? *keep : local;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (7);
        Result result;
        std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) std::ceil (seconds * sampleRate / blockSize); ++b)   // at least `seconds`
        {
            const auto& sounding = (double) t / sampleRate >= switchAt ? later : notes;
            fx.setNotes (sounding.data(), (int) sounding.size());
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
                result.voice[0].push_back (l[i] - inL[i]);
                result.voice[1].push_back (r[i] - inR[i]);
            }
            t += blockSize;
        }
        return result;
    }

    Settings calling (float mix = 1.0f)
    {
        Settings s;
        s.on = true;
        s.mix = mix;
        s.depth = 0;
        s.drift = 0.0f;
        s.growl = 0.0f;
        s.delay = 0.0f;
        s.size = 0.5f;
        return s;
    }

    size_t at (double seconds) { return (size_t) (seconds * sampleRate); }

    std::pair<float, float> sine (double hz, long long i, float level = 0.3f)
    {
        const auto v = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate);
        return { v, v };
    }

    // Power between two frequencies (summed over 16384-sample frames from `from` to `to`).
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
            for (int bin = (int) std::ceil (loHz * size / sampleRate); bin <= (int) (hiHz * size / sampleRate); ++bin)
                total += (double) data[(size_t) bin] * data[(size_t) bin];
        }
        return total;
    }

    // How much of the voice lies on the harmonics of `f0` (within 4 Hz) against halfway between them (within 4 Hz).
    double onAgainstBetween (const std::vector<float>& x, double f0, double from, double to, int harmonics)
    {
        double on = 0.0, between = 0.0;
        for (int h = 1; h <= harmonics; ++h)
        {
            on += powerBetween (x, from, to, f0 * h - 4.0, f0 * h + 4.0);
            between += powerBetween (x, from, to, f0 * (h + 0.5) - 4.0, f0 * (h + 0.5) + 4.0);
        }
        return 10.0 * std::log10 (on / juce::jmax (1.0e-20, between));
    }

    double both (const std::array<std::vector<float>, 2>& x, double from, double to)
    {
        return db (std::sqrt (0.5 * (std::pow (rms (x[0], at (from), at (to)), 2.0) + std::pow (rms (x[1], at (from), at (to)), 2.0))));
    }
}

TEST_CASE ("LEVIATHAN: off and at MIX 0 the sound passes exactly as it was", "[leviathan]")
{
    const auto input = [] (long long i) { return sine (220.0, i); };
    const auto zero = run (1.5, input, calling (0.0f), { 57.0f });
    CHECK (*std::max_element (zero.voice[0].begin(), zero.voice[0].end()) == 0.0f);
    CHECK (*std::min_element (zero.voice[0].begin(), zero.voice[0].end()) == 0.0f);
    auto off = calling (1.0f);
    off.on = false;
    const auto o = run (1.5, input, off, { 57.0f });
    CHECK (*std::max_element (o.voice[1].begin(), o.voice[1].end()) == 0.0f);
    CHECK (*std::min_element (o.voice[1].begin(), o.voice[1].end()) == 0.0f);
}

TEST_CASE ("LEVIATHAN: sings an octave or two below the lowest note sounding", "[leviathan]")
{
    // A3 (220 Hz) sounding: one octave down is 110 Hz, its harmonics 110, 220, 330...; between them, nothing.
    const auto one = run (3.0, [] (long long i) { return sine (220.0, i); }, calling(), { 57.0f });
    const auto oneOctave = onAgainstBetween (one.voice[0], 110.0, 1.0, 3.0, 8);
    INFO ("1 OCTAVE: on 110 Hz's harmonics against between them " << oneOctave << " dB");
    CHECK (oneOctave > 20.0);
    // Two octaves: 55 Hz.
    auto deeper = calling();
    deeper.depth = 1;
    const auto two = run (3.0, [] (long long i) { return sine (220.0, i); }, deeper, { 57.0f });
    const auto twoOctaves = onAgainstBetween (two.voice[0], 55.0, 1.0, 3.0, 16);
    INFO ("2 OCTAVES: on 55 Hz's harmonics against between them " << twoOctaves << " dB");
    CHECK (twoOctaves > 20.0);
    // A chord (A3, E4, A4): the lowest, A3, sets it: 110 Hz again (and not 165 Hz, E4's octave below).
    const auto chord = run (3.0, [] (long long i) { return sine (220.0, i); }, calling(), { 69.0f, 57.0f, 64.0f });
    CHECK (onAgainstBetween (chord.voice[0], 110.0, 1.0, 3.0, 8) > 20.0);
}

TEST_CASE ("LEVIATHAN: as loud as the sound at MIX 1, half as loud at MIX 0.5; it glides to a new note", "[leviathan]")
{
    const auto full = run (3.0, [] (long long i) { return sine (220.0, i); }, calling (1.0f), { 57.0f });
    const auto input = db (0.3 / std::sqrt (2.0));
    INFO ("the sound " << input << " dB, the voice at MIX 1 " << both (full.voice, 1.0, 3.0) << " dB");
    CHECK (std::abs (both (full.voice, 1.0, 3.0) - input) < 2.0);
    const auto half = run (3.0, [] (long long i) { return sine (220.0, i); }, calling (0.5f), { 57.0f });
    CHECK (std::abs (both (half.voice, 1.0, 3.0) - (input - 6.0)) < 2.0);

    // A3 to E3 at 1.5 s: by 2 s it sings E2 (82.4 Hz).
    const auto moved = run (4.0, [] (long long i) { return sine (220.0, i); }, calling(), { 57.0f }, { 52.0f }, 1.5);
    CHECK (onAgainstBetween (moved.voice[0], 82.41, 2.2, 4.0, 8) > 15.0);
}

TEST_CASE ("LEVIATHAN: DELAY puts it behind; silence stays silent", "[leviathan]")
{
    // A burst from 0.5 s to 1.5 s, the voice 1.5 s behind (DELAY 1): nothing until 2 s.
    const auto burst = [] (long long i) { return i >= (long long) (0.5 * sampleRate) && i < (long long) (1.5 * sampleRate) ? sine (220.0, i) : std::pair<float, float> { 0.0f, 0.0f }; };
    auto behind = calling();
    behind.delay = 1.0f;
    const auto d = run (4.0, burst, behind, { 57.0f });
    INFO ("0.6-1.9 s " << both (d.voice, 0.6, 1.9) << " dB, 2.2-2.9 s " << both (d.voice, 2.2, 2.9) << " dB");
    CHECK (both (d.voice, 0.6, 1.9) < -100.0);
    CHECK (both (d.voice, 2.2, 2.9) > -30.0);

    // Long after the sound stops, the output is exact silence (growl and drift on).
    auto rough = calling();
    rough.growl = 1.0f;
    rough.drift = 1.0f;
    const auto r = run (8.0, burst, rough, { 57.0f });
    float tail = 0.0f;
    for (auto i = at (6.0); i < r.out[0].size(); ++i)
        tail = std::max (tail, std::max (std::abs (r.out[0][i]), std::abs (r.out[1][i])));
    CHECK (tail == 0.0f);
}

TEST_CASE ("LEVIATHAN: GROWL creaks, a lower shadow between the harmonics; the vowel wanders", "[leviathan]")
{
    auto rough = calling();
    rough.growl = 1.0f;
    const auto smooth = onAgainstBetween (run (3.0, [] (long long i) { return sine (220.0, i); }, calling(), { 57.0f }).voice[0], 110.0, 1.0, 3.0, 8);
    const auto creaking = onAgainstBetween (run (3.0, [] (long long i) { return sine (220.0, i); }, rough, { 57.0f }).voice[0], 110.0, 1.0, 3.0, 8);
    INFO ("on the harmonics against between them: GROWL 0 " << smooth << " dB, GROWL 1 " << creaking << " dB");
    CHECK (creaking < smooth - 10.0);

    effects::Leviathan fx;
    auto wandering = calling();
    wandering.drift = 1.0f;   // a new place about every 0.8 s
    float lowest = 1.0f, highest = 0.0f;
    for (int k = 0; k < 8; ++k)
    {
        run (1.0, [] (long long i) { return sine (220.0, i); }, wandering, { 57.0f }, {}, 1.0e9, &fx);
        lowest = std::min (lowest, fx.getVowel());
        highest = std::max (highest, fx.getVowel());
    }
    INFO ("the vowel between " << lowest << " and " << highest);
    CHECK (highest - lowest > 0.1f);
}

TEST_CASE ("CPU: LEVIATHAN, a growling voice", "[leviathan][cpu]")
{
    auto s = calling (0.7f);
    s.growl = 0.6f;
    s.drift = 0.6f;
    s.delay = 0.3f;
    const auto started = std::chrono::steady_clock::now();
    run (20.0, [] (long long i) { return sine (220.0, i); }, s, { 57.0f });
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("LEVIATHAN, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input and bookkeeping included)");
    CHECK (seconds / 20.0 * 100.0 < 5.0);
}
