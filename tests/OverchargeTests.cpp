// OVERCHARGE (0.77.0): unstable magic. Off and at CHARGE 0 the sound passes untouched bit for bit, and it comes back
// to untouched once CHARGE returns to 0 and what it added dies away; the first half charges (crystal tones an octave,
// an octave and a fifth and two octaves above the notes, no bursts yet); the second overloads in bursts, more often
// with more CHARGE and CHAOS; the level is kept; silence stays silent.
#include "SynthTestUtils.h"

#include "dsp/effects/Overcharge.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Overcharge::Settings;

    struct Run
    {
        std::array<std::vector<float>, 2> out;
        int bursts = 0;
        bool resting = false;
    };

    // Runs `left` / `right` through OVERCHARGE in blocks of 256 with `notes` sounding (CHARGE changed to `later` at
    // `changeAt` if given).
    Run runOvercharge (std::vector<float> left, std::vector<float> right, Settings s, std::vector<float> notes,
                       int changeAt = -1, Settings later = {})
    {
        effects::Overcharge fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (1);
        fx.setNotes (notes.data(), (int) notes.size());
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            juce::dsp::AudioBlock<float> block (channels, 2, n);
            fx.process (block, changeAt >= 0 && (int) start >= changeAt ? later : s);
        }
        return { { left, right }, fx.getBurstCount(), fx.isResting() };
    }

    std::vector<float> sine (double hz, double seconds, float amplitude = 0.3f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate);
        return x;
    }

    std::vector<float> saw (double hz, double seconds, float amplitude = 0.3f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        double phase = 0.0;
        for (auto& v : x)
        {
            v = amplitude * (float) (2.0 * phase - 1.0);
            phase += hz / sampleRate;
            phase -= std::floor (phase);
        }
        return x;
    }

    Settings at (float charge, float chaos = 0.6f)
    {
        Settings s;
        s.on = true;
        s.charge = charge;
        s.chaos = chaos;
        return s;
    }
}

TEST_CASE ("OVERCHARGE: off and at CHARGE 0 the sound passes untouched, and comes back untouched after charging", "[overcharge]")
{
    const auto x = saw (220.0, 2.0);
    Settings off;
    off.charge = 1.0f;   // off: CHARGE doesn't matter
    const auto a = runOvercharge (x, x, off, { 57.0f });
    CHECK (a.out[0] == x);
    CHECK (a.out[1] == x);
    const auto b = runOvercharge (x, x, at (0.0f), { 57.0f });
    CHECK (b.out[0] == x);
    CHECK (b.resting);

    // Charged and overloading for a second, then back to 0: untouched again once what it added has died away.
    const auto long_ = saw (220.0, 6.0);
    const auto c = runOvercharge (long_, long_, at (1.0f, 1.0f), { 57.0f }, (int) (1.0 * sampleRate), at (0.0f));
    CHECK (c.resting);
    auto differs = 0.0f;
    for (auto i = (size_t) (4.5 * sampleRate); i < long_.size(); ++i)
        differs = std::max (differs, std::abs (c.out[0][i] - long_[i]));
    CHECK (differs == 0.0f);
}

TEST_CASE ("OVERCHARGE: the first half charges: crystal tones above the note, no bursts yet", "[overcharge]")
{
    // A pure A3 (220 Hz): alone it has nothing at 440, 659 or 880 Hz; charged, the crystals ring there.
    const auto x = sine (220.0, 3.0);
    const auto plain = runOvercharge (x, x, at (0.0f), { 57.0f });
    const auto charged = runOvercharge (x, x, at (0.45f), { 57.0f });
    CHECK (charged.bursts == 0);
    const auto from = (size_t) (1.5 * sampleRate), length = (size_t) (1.0 * sampleRate);
    for (const auto hz : { 440.0, 659.26, 880.0 })
    {
        const auto before = levelAt (plain.out[0], hz, from, length), after = levelAt (charged.out[0], hz, from, length);
        INFO (hz << " Hz: " << db (before) << " dB alone, " << db (after) << " dB charged");
        CHECK (db (before) < -80.0);
        CHECK (db (after) > -45.0);
    }
    // GLOW 0 leaves the crystals out (only the climbing repeats, which are octaves of the sound: 440 and 880).
    auto noGlow = at (0.45f);
    noGlow.glow = 0.0f;
    const auto dim = runOvercharge (x, x, noGlow, { 57.0f });
    INFO ("659 Hz with GLOW 0: " << db (levelAt (dim.out[0], 659.26, from, length)) << " dB");
    CHECK (db (levelAt (dim.out[0], 659.26, from, length)) < db (levelAt (charged.out[0], 659.26, from, length)) - 20.0);
}

TEST_CASE ("OVERCHARGE: the second half overloads in bursts, more with more CHARGE and CHAOS", "[overcharge]")
{
    const auto x = saw (220.0, 8.0);
    const auto mild = runOvercharge (x, x, at (0.62f, 0.1f), { 57.0f });
    const auto wild = runOvercharge (x, x, at (1.0f, 1.0f), { 57.0f });
    INFO ("bursts in 8 s: mild " << mild.bursts << ", wild " << wild.bursts);
    CHECK (wild.bursts > 20);
    CHECK (wild.bursts > 3 * mild.bursts);
    CHECK (effects::Overcharge::burstRate (0.0f, 1.0f) == 0.0f);
    CHECK (effects::Overcharge::burstRate (1.0f, 1.0f) > effects::Overcharge::burstRate (0.5f, 1.0f));
}

TEST_CASE ("OVERCHARGE: wilder, not louder; silence stays silent", "[overcharge]")
{
    const auto x = saw (220.0, 6.0, 0.25f);
    const auto from = (size_t) (2.0 * sampleRate), to = x.size();
    for (const auto charge : { 0.45f, 0.75f, 1.0f })
    {
        const auto out = runOvercharge (x, x, at (charge, 1.0f), { 57.0f });
        const auto level = db (rms (out.out[0], from, to) / rms (x, from, to));
        INFO ("CHARGE " << charge << ": " << level << " dB");
        CHECK (std::abs (level) < 2.0);
    }
    const std::vector<float> silence ((size_t) (3.0 * sampleRate), 0.0f);
    const auto quiet = runOvercharge (silence, silence, at (1.0f, 1.0f), { 57.0f, 64.0f });
    auto peak = 0.0f;
    for (const auto& side : quiet.out)
        for (const auto v : side)
            peak = std::max (peak, std::abs (v));
    CHECK (peak == 0.0f);
}

TEST_CASE ("CPU: OVERCHARGE fully overloading with eight notes", "[overcharge][cpu]")
{
    const auto x = saw (220.0, 4.0);
    const auto start = std::chrono::steady_clock::now();
    runOvercharge (x, x, at (1.0f, 1.0f), { 45, 52, 57, 60, 64, 67, 69, 72 });
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const auto share = 100.0 * seconds / 4.0;
    WARN ("OVERCHARGE, eight notes, full: " << share << "% of a core");
    CHECK (share < 10.0);
}
