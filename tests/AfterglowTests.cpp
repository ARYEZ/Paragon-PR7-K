// AFTERGLOW (0.60.0): the glow of a 1980s polysynth. Checked by what it does to the sound.
// The notes: off, or on at AGE and DRIFT 0, every sample as before (the synth's other random choices untouched); AGE
// puts each note at its own tuning (within its limit) and brightness; DRIFT sways a held note's tuning slowly.
// The effect: off and at 0 untouched; ENSEMBLE spreads a centred sound wide at about the same level; TAPE's wow sways
// the pitch by about the cents planned, its saturation adds harmonics, its top is softer; it fades in without a jump.
#include "SynthTestUtils.h"

#include "dsp/effects/Afterglow.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>

using namespace synth;

namespace
{
    // The frequency of a near-sine over x[from, from + length): rising zero crossings, placed between samples.
    double crossingHz (const std::vector<float>& x, size_t from, size_t length)
    {
        double first = -1.0, last = -1.0;
        int crossings = 0;
        for (auto i = from + 1; i < from + length && i < x.size(); ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                const auto at = (double) (i - 1) + (double) (-x[i - 1]) / (double) (x[i] - x[i - 1]);
                if (first < 0.0)
                    first = at;
                else
                    ++crossings;
                last = at;
            }
        return crossings > 0 ? sampleRate * crossings / (last - first) : 0.0;
    }

    double centsOf (double hz, double reference) { return 1200.0 * std::log2 (hz / reference); }

    Player sinePlayer()
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.amp.envelope.attackMs = 1.0f;
        p.settings.amp.envelope.releaseMs = 5.0f;
        return p;
    }

    // The effect: `seconds` of `make (t)` (left, right) through it; both sides out.
    template <typename Make>
    std::array<std::vector<float>, 2> run (effects::Afterglow::Settings s, double seconds, Make make)
    {
        effects::Afterglow fx;
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
                const auto [a, c] = make ((double) (t + (long long) i) / sampleRate);
                l[i] = a;
                r[i] = c;
            }
            fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, (size_t) block), s);
            out[0].insert (out[0].end(), l.begin(), l.end());
            out[1].insert (out[1].end(), r.begin(), r.end());
            t += block;
        }
        return out;
    }

    std::pair<float, float> centred (double hz, double t, float level = 0.4f)
    {
        const auto v = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * t);
        return { v, v };
    }
}

TEST_CASE ("AFTERGLOW's notes: off, or on at AGE and DRIFT 0, every sample as before", "[afterglow]")
{
    const auto play = [] (bool on, float age, float drift)
    {
        Player p;   // a saw with unison: its copies' random phases must come out the same too
        p.osc().unisonVoices = 5;
        p.osc().unisonMix = 0.8f;
        p.osc().unisonDetune = 0.2f;
        p.settings.filter.on = true;
        p.settings.afterglow.on = on;
        p.settings.afterglow.age = age;
        p.settings.afterglow.drift = drift;
        for (const auto note : { 57, 60, 64 })
            p.on (note);
        auto out = p.render (0.8);
        p.off (60);
        auto more = p.render (0.4);
        out[0].insert (out[0].end(), more[0].begin(), more[0].end());
        return out[0];
    };
    const auto reference = play (false, 0.0f, 0.0f);
    CHECK (play (false, 1.0f, 1.0f) == reference);   // off: AGE and DRIFT don't matter
    CHECK (play (true, 0.0f, 0.0f) == reference);    // on at 0
    CHECK (play (true, 1.0f, 0.0f) != reference);
}

TEST_CASE ("AFTERGLOW: AGE gives each note its own tuning, within its limit, and its own brightness", "[afterglow]")
{
    auto p = sinePlayer();
    p.settings.afterglow.on = true;
    p.settings.afterglow.age = 1.0f;
    std::vector<double> cents;
    for (int k = 0; k < 24; ++k)
    {
        p.on (69);
        const auto out = p.render (0.3);
        p.off (69);
        p.render (0.05);
        cents.push_back (centsOf (crossingHz (out[0], (size_t) (0.05 * sampleRate), (size_t) (0.24 * sampleRate)), 440.0));
    }
    const auto [low, high] = std::minmax_element (cents.begin(), cents.end());
    INFO ("24 notes of A4 at AGE 1: from " << *low << " to " << *high << " cents");
    CHECK (*high - *low > 8.0);                                               // they differ, as old voices do
    CHECK (std::abs (*low) <= Settings::Afterglow::ageCents + 0.5);           // within the limit
    CHECK (std::abs (*high) <= Settings::Afterglow::ageCents + 0.5);

    // The filter: a saw through a low-pass, played again and again: each note's brightness its own.
    Player q;
    q.settings.filter.on = true;
    q.settings.filter.cutoffHz = 1500.0f;
    q.settings.afterglow.on = true;
    q.settings.afterglow.age = 1.0f;
    std::vector<double> bright;
    for (int k = 0; k < 12; ++k)
    {
        q.on (45);
        const auto out = q.render (1.5);
        q.off (45);
        q.render (0.3);
        const Spectrum s (out[0], (size_t) (0.05 * sampleRate));
        bright.push_back (s.at (110.0 * 20) - s.at (110.0));   // the 20th harmonic against the fundamental
    }
    const auto [dim, lit] = std::minmax_element (bright.begin(), bright.end());
    INFO ("the 20th harmonic against the fundamental: " << *dim << " to " << *lit << " dB");
    CHECK (*lit - *dim > 3.0);
}

TEST_CASE ("AFTERGLOW: DRIFT sways a held note's tuning slowly", "[afterglow]")
{
    auto p = sinePlayer();
    p.settings.afterglow.on = true;
    p.settings.afterglow.drift = 1.0f;
    p.on (69);
    const auto out = p.render (6.0);
    std::vector<double> cents;
    for (double t = 0.2; t + 0.2 < 6.0; t += 0.2)
        cents.push_back (centsOf (crossingHz (out[0], (size_t) (t * sampleRate), (size_t) (0.2 * sampleRate)), 440.0));
    const auto [low, high] = std::minmax_element (cents.begin(), cents.end());
    double fastest = 0.0;
    for (size_t k = 1; k < cents.size(); ++k)
        fastest = std::max (fastest, std::abs (cents[k] - cents[k - 1]));
    INFO ("a held A4 at DRIFT 1 over 6 s: " << *low << " to " << *high << " cents; at most " << fastest << " cents per 0.2 s");
    CHECK (*high - *low > 4.0);                                        // it wanders...
    CHECK (std::abs (*low) <= Settings::Afterglow::driftCents + 0.5);   // ...within its limit
    CHECK (std::abs (*high) <= Settings::Afterglow::driftCents + 0.5);
    CHECK (fastest < 8.0);                                             // ...slowly, not as a wobble
}

TEST_CASE ("AFTERGLOW's effect: off and at 0 untouched; ENSEMBLE spreads a centred sound wide at about its level", "[afterglow]")
{
    const auto input = [] (double t) { return centred (440.0, t); };
    const auto dry = run ({}, 1.0, input);
    effects::Afterglow::Settings off;
    off.ensemble = off.tape = 1.0f;   // off: they don't matter
    CHECK (run (off, 1.0, input) == dry);
    effects::Afterglow::Settings zero;
    zero.on = true;
    CHECK (run (zero, 1.0, input) == dry);

    effects::Afterglow::Settings ens;
    ens.on = true;
    ens.ensemble = 1.0f;
    const auto wide = run (ens, 3.0, input);
    const auto from = (size_t) (0.5 * sampleRate), to = wide[0].size();
    double mid = 0.0, side = 0.0;
    for (auto i = from; i < to; ++i)
    {
        const auto m = 0.5 * (wide[0][i] + wide[1][i]), s = 0.5 * (wide[0][i] - wide[1][i]);
        mid += m * m;
        side += s * s;
    }
    const auto level = db (rms (wide[0], from, to)) - db (rms (dry[0], (size_t) (0.5 * sampleRate), dry[0].size()));
    INFO ("side against mid: " << 10.0 * std::log10 (side / mid) << " dB; level " << level << " dB against dry");
    CHECK (10.0 * std::log10 (side / mid) > -12.0);   // a centred sine comes out wide
    CHECK (std::abs (level) < 3.0);
}

TEST_CASE ("AFTERGLOW's TAPE: the wow sways the pitch, the saturation adds harmonics, the top is softer", "[afterglow]")
{
    effects::Afterglow::Settings tape;
    tape.on = true;
    tape.tape = 1.0f;
    const auto out = run (tape, 6.0, [] (double t) { return centred (1000.0, t, 0.2f); });
    std::vector<double> cents;
    for (double t = 0.5; t + 0.1 < 6.0; t += 0.1)
        cents.push_back (centsOf (crossingHz (out[0], (size_t) (t * sampleRate), (size_t) (0.1 * sampleRate)), 1000.0));
    const auto [low, high] = std::minmax_element (cents.begin(), cents.end());
    INFO ("1 kHz through TAPE: " << *low << " to " << *high << " cents");
    // The wow: 1.6 ms at 0.55 Hz sways about +-9.6 cents (measured over 0.1 s windows, a little less).
    CHECK (*high > 5.0);
    CHECK (*low < -5.0);
    CHECK (*high < 14.0);
    CHECK (*low > -14.0);

    // Saturation: a loud 220 Hz sine comes out with a 3rd harmonic.
    const auto loud = run (tape, 1.5, [] (double t) { return centred (220.0, t, 0.9f); });
    const auto plain = run ({}, 1.5, [] (double t) { return centred (220.0, t, 0.9f); });
    const auto third = [] (const std::vector<float>& x) {
        return db (levelAt (x, 660.0, (size_t) (0.5 * sampleRate), (size_t) (0.9 * sampleRate))) - db (levelAt (x, 220.0, (size_t) (0.5 * sampleRate), (size_t) (0.9 * sampleRate)));
    };
    INFO ("3rd harmonic: plain " << third (plain[0]) << " dB, through TAPE " << third (loud[0]) << " dB");
    CHECK (third (loud[0]) > third (plain[0]) + 40.0);

    // The top: with 500 Hz and 14 kHz together, the 14 kHz comes out quieter against the 500 Hz (the level is kept
    // overall, so it's the balance that tells).
    const auto mixed = [] (double t) {
        const auto v = 0.15f * (float) std::sin (juce::MathConstants<double>::twoPi * 500.0 * t)
                     + 0.15f * (float) std::sin (juce::MathConstants<double>::twoPi * 14000.0 * t);
        return std::pair<float, float> { v, v };
    };
    const auto top = run (tape, 2.0, mixed);
    const auto from = (size_t) (1.0 * sampleRate), length = (size_t) (0.9 * sampleRate);
    const auto tilt = db (levelAt (top[0], 14000.0, from, length)) - db (levelAt (top[0], 500.0, from, length));
    INFO ("14 kHz against 500 Hz through TAPE: " << tilt << " dB (0 going in)");
    CHECK (tilt < -3.0);
}

TEST_CASE ("AFTERGLOW's effect fades in without a jump as TAPE and ENSEMBLE turn up from 0", "[afterglow]")
{
    effects::Afterglow fx;
    fx.prepare (sampleRate, 256);
    std::vector<float> l (256), r (256);
    float* ptrs[] = { l.data(), r.data() };
    double t = 0.0;
    float last = 0.0f, biggestStep = 0.0f;
    for (int b = 0; b < 400; ++b)
    {
        effects::Afterglow::Settings s;
        s.on = true;
        s.tape = s.ensemble = juce::jlimit (0.0f, 1.0f, (float) (b - 100) / 200.0f);   // 0 for a while, then rising
        for (size_t i = 0; i < 256; ++i, t += 1.0 / sampleRate)
            l[i] = r[i] = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 110.0 * t);
        fx.process (juce::dsp::AudioBlock<float> (ptrs, 2, 256), s);
        for (size_t i = 0; i < 256; ++i)
        {
            biggestStep = std::max (biggestStep, std::abs (l[i] - last));
            last = l[i];
        }
    }
    // A 110 Hz sine at 0.4 moves at most 0.4 x 2 pi x 110 / 48000 = 0.0058 a sample; a jump would be far more.
    INFO ("the biggest step from one sample to the next: " << biggestStep);
    CHECK (biggestStep < 0.012f);
}

TEST_CASE ("CPU: AFTERGLOW's effect, ENSEMBLE and TAPE at full", "[afterglow][cpu]")
{
    effects::Afterglow::Settings s;
    s.on = true;
    s.ensemble = s.tape = 1.0f;
    const auto started = std::chrono::steady_clock::now();
    run (s, 20.0, [] (double t) { return centred (220.0, t); });
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
    WARN ("AFTERGLOW's effect, stereo: " << seconds / 20.0 * 100.0 << "% of a core (the test's own input included)");
    CHECK (seconds / 20.0 * 100.0 < 5.0);
}
