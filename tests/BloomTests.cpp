// BLOOM (0.74.0): GRAND CINEMA's strings ringing along and a hall opening out. Checked by what it does to the sound:
// off and at AMOUNT 0 untouched bit for bit, and back to untouched once it has settled; silence stays silent; the
// strings ring on after the note at the notes they're tuned to (and an octave below's harmonics), not at a pitch the
// music doesn't share with them, and for as long as RING says; the hall opens as a chord is held (SWELL); levels.
#include "SynthTestUtils.h"

#include "dsp/effects/Bloom.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    using Settings = effects::Bloom::Settings;

    // Runs `seconds` of input through one Bloom with `notes` sounding, the settings a function of time.
    template <typename Input, typename At>
    std::array<std::vector<float>, 2> run (effects::Bloom& fx, double seconds, Input input, At settingsAt,
                                           std::vector<float> notes = {})
    {
        std::array<std::vector<float>, 2> out;
        std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);
        float* ptrs[] = { l.data(), r.data() };
        long long t = 0;
        for (int b = 0; b < (int) std::ceil (seconds * sampleRate / blockSize); ++b)
        {
            for (size_t i = 0; i < (size_t) blockSize; ++i)
            {
                const auto [a, c] = input (t + (long long) i);
                l[i] = a;
                r[i] = c;
            }
            fx.setNotes (notes.data(), (int) notes.size());
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) blockSize), settingsAt ((double) t / sampleRate));
            out[0].insert (out[0].end(), l.begin(), l.end());
            out[1].insert (out[1].end(), r.begin(), r.end());
            t += blockSize;
        }
        return out;
    }

    double bandLevel (const std::vector<float>& x, double hz, size_t from, size_t length)
    {
        double power = 0.0;
        for (auto f = hz - 3.0; f <= hz + 3.0; f += 1.0)
            power += std::pow (levelAt (x, f, from, length), 2.0);
        return std::sqrt (power);
    }

    double hzOf (int note) { return 440.0 * std::exp2 ((note - 69) / 12.0); }
}

TEST_CASE ("BLOOM: off and at AMOUNT 0 untouched; silence stays silent; back to untouched once settled", "[bloom]")
{
    juce::Random random (3);
    std::vector<float> noise (48000 * 10);
    for (auto& v : noise)
        v = random.nextFloat() - 0.5f;
    const auto input = [&] (long long t) { const auto v = noise[(size_t) t % noise.size()]; return std::make_pair (v, -v); };
    for (const auto on : { false, true })
    {
        effects::Bloom fx;
        fx.prepare (sampleRate, blockSize);
        Settings s;
        s.on = on;
        s.amount = on ? 0.0005f : 1.0f;   // on: under 0.001 counts as 0
        const auto out = run (fx, 1.0, input, [&] (double) { return s; }, { 60.0f });
        auto same = true;
        for (size_t i = 0; i < out[0].size(); ++i)
            same = same && out[0][i] == noise[i] && out[1][i] == -noise[i];
        CHECK (same);
    }
    {
        effects::Bloom fx;
        fx.prepare (sampleRate, blockSize);
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        const auto out = run (fx, 2.0, [] (long long) { return std::make_pair (0.0f, 0.0f); }, [&] (double) { return s; }, { 60.0f });
        CHECK (rms (out[0], 0, out[0].size()) == 0.0);
    }
    {
        // Up for 3 s, then down: once settled (the strings and hall emptied), untouched bit for bit.
        effects::Bloom fx;
        fx.prepare (sampleRate, blockSize);
        const auto out = run (fx, 6.0, input, [] (double t) { Settings s; s.on = true; s.amount = t < 3.0 ? 1.0f : 0.0f; return s; }, { 48.0f, 55.0f, 60.0f });
        auto changed = false, same = true;
        for (size_t i = 0; i < (size_t) (3.0 * sampleRate); ++i)
            changed = changed || out[0][i] != noise[i];
        for (auto i = (size_t) (4.0 * sampleRate); i < out[0].size(); ++i)
            same = same && out[0][i] == noise[i] && out[1][i] == -noise[i];
        CHECK (changed);
        CHECK (same);
        CHECK (fx.isResting());
    }
}

TEST_CASE ("BLOOM: the strings ring on at the notes sounding, not at a pitch the music doesn't share, for RING", "[bloom]")
{
    // A tone for 1 s, then nothing; C4 sounding. The strings' own part is what RESONANCE 1 adds over RESONANCE 0 (all
    // else the same, it's linear): measured at the tone's pitch after the tone has stopped. SIZE short.
    const auto strings = [] (double toneHz, float ring, double from, double to)
    {
        std::array<std::vector<float>, 2> outs;
        for (const auto resonance : { 0.0f, 1.0f })
        {
            effects::Bloom fx;
            fx.prepare (sampleRate, blockSize);
            Settings s;
            s.on = true;
            s.amount = 1.0f;
            s.resonance = resonance;
            s.ring = ring;
            s.size = 0.0f;
            s.swell = 0.0f;
            outs[resonance > 0.0f ? 1 : 0] = run (fx, to + 0.1, [toneHz] (long long t)
            {
                const auto v = t < (long long) sampleRate ? 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * toneHz * (double) t / sampleRate) : 0.0f;
                return std::make_pair (v, v);
            }, [&] (double) { return s; }, { 60.0f })[0];
        }
        std::vector<float> own (outs[0].size());
        for (size_t i = 0; i < own.size(); ++i)
            own[i] = outs[1][i] - outs[0][i];
        return bandLevel (own, toneHz, (size_t) (from * sampleRate), (size_t) ((to - from) * sampleRate));
    };
    const auto c4 = strings (hzOf (60), 0.5f, 1.3, 2.3), g5 = strings (hzOf (79), 0.5f, 1.3, 2.3), cSharp4 = strings (hzOf (61), 0.5f, 1.3, 2.3);
    INFO ("the strings ringing after the tone: C4 " << c4 << ", G5 (the octave and a fifth's string) " << g5 << ", C#4 (no string) " << cSharp4);
    CHECK (c4 > 10.0 * cSharp4);   // the note's own string rings
    CHECK (g5 > 5.0 * cSharp4);    // and the octave-and-a-fifth's
    // RING: longer at the top, shorter at the bottom (3 s after the tone).
    CHECK (strings (hzOf (60), 1.0f, 4.0, 5.0) > 4.0 * strings (hzOf (60), 0.0f, 4.0, 5.0));
}

TEST_CASE ("BLOOM: the hall opens as a chord is held (SWELL); the added sound sits under the chord", "[bloom]")
{
    // A held chord (C3 E4 G4 as sines): the added sound (out less in) early against later in the hold.
    const auto chord = [] (long long t)
    {
        auto v = 0.0f;
        for (const auto note : { 48, 64, 67 })
            v += 0.15f * (float) std::sin (juce::MathConstants<double>::twoPi * hzOf (note) * (double) t / sampleRate);
        return v;
    };
    effects::Bloom fx;
    fx.prepare (sampleRate, blockSize);
    Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.swell = 0.7f;   // about 1.4 s to open
    const auto out = run (fx, 6.0, [&] (long long t) { const auto v = chord (t); return std::make_pair (v, v); },
                          [&] (double) { return s; }, { 48.0f, 64.0f, 67.0f });
    std::vector<float> added (out[0].size()), dry (out[0].size());
    for (size_t i = 0; i < added.size(); ++i)
    {
        dry[i] = chord ((long long) i);
        added[i] = out[0][i] - dry[i];
    }
    const auto early = rms (added, (size_t) (0.1 * sampleRate), (size_t) (0.4 * sampleRate));
    const auto late = rms (added, (size_t) (5.0 * sampleRate), (size_t) (6.0 * sampleRate));
    const auto dryLevel = rms (dry, (size_t) (5.0 * sampleRate), (size_t) (6.0 * sampleRate));
    INFO ("added early " << juce::Decibels::gainToDecibels (early) << " dB, late " << juce::Decibels::gainToDecibels (late)
          << " dB, the chord " << juce::Decibels::gainToDecibels (dryLevel) << " dB");
    CHECK (late > 2.0 * early);                                       // it blooms
    CHECK (juce::Decibels::gainToDecibels (late / dryLevel) < 3.0);   // and doesn't swamp the chord at full
    CHECK (juce::Decibels::gainToDecibels (late / dryLevel) > -12.0); // but is clearly there
    CHECK (std::isfinite (rms (out[0], 0, out[0].size())));
}

TEST_CASE ("BLOOM: CPU", "[bloom][performance]")
{
    effects::Bloom fx;
    fx.prepare (sampleRate, blockSize);
    Settings s;
    s.on = true;
    s.amount = 1.0f;
    std::vector<float> notes { 36, 48, 55, 60, 64, 67, 72 };
    const auto started = std::chrono::steady_clock::now();
    run (fx, 10.0, [] (long long t) { const auto v = 0.2f * (float) std::sin (0.01 * (double) t); return std::make_pair (v, v); },
         [&] (double) { return s; }, notes);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("BLOOM: " << 100.0 * seconds / 10.0 << "% of a core, 24 strings a side and the hall");
    CHECK (seconds / 10.0 < 0.1);
}

// Not run with the others: the strings' and the hall's levels apart, against a held chord (for setting their make-ups).
TEST_CASE ("BLOOM: levels apart", "[.bloomlevels]")
{
    const auto chord = [] (long long t)
    {
        auto v = 0.0f;
        for (const auto note : { 48, 64, 67 })
            v += 0.15f * (float) std::sin (juce::MathConstants<double>::twoPi * hzOf (note) * (double) t / sampleRate);
        return v;
    };
    for (const auto [resonance, size] : { std::pair { 1.0f, 0.5f }, std::pair { 0.0f, 0.5f }, std::pair { 0.0f, 0.0f }, std::pair { 0.0f, 1.0f } })
    {
        effects::Bloom fx;
        fx.prepare (sampleRate, blockSize);
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.resonance = resonance;
        s.size = size;
        s.swell = 0.0f;
        const auto out = run (fx, 6.0, [&] (long long t) { const auto v = chord (t); return std::make_pair (v, v); },
                              [&] (double) { return s; }, { 48.0f, 64.0f, 67.0f });
        std::vector<float> added (out[0].size()), dry (out[0].size());
        for (size_t i = 0; i < added.size(); ++i)
        {
            dry[i] = chord ((long long) i);
            added[i] = out[0][i] - dry[i];
        }
        WARN ("resonance " << resonance << " size " << size << ": added "
              << juce::Decibels::gainToDecibels (rms (added, (size_t) (5.0 * sampleRate), (size_t) (6.0 * sampleRate)) / rms (dry, (size_t) (5.0 * sampleRate), (size_t) (6.0 * sampleRate))) << " dB against the chord");
    }
}
