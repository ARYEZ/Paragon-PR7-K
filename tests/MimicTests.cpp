// MIMIC (0.44.0): sounds made here with known pitch, loudness, tone and movement; MIMIC must measure them back.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "dsp/synth/Mimic.h"
#include "dsp/synth/Wavetable.h"
#include "rigs/MimicRig.h"

#include <cmath>
#include <functional>

namespace
{
    constexpr double rate = 48000.0;
    using Mimic = synth::mimic::Analysis;

    // A note: `hz (t)` its pitch, `level (t)` its loudness (0 to 1), `harmonic (k, t)` harmonic k's level against the
    // fundamental, summed up to just under half the sample rate (no aliasing), into a buffer of `channels`.
    juce::AudioBuffer<float> note (double seconds, const std::function<double (double)>& hz, const std::function<double (double)>& level,
                                   const std::function<double (int, double)>& harmonic, int channels = 1, int copies = 1,
                                   double detuneCents = 0.0, double noise = 0.0)
    {
        const auto length = (int) (seconds * rate);
        juce::AudioBuffer<float> b (channels, length);
        b.clear();
        juce::Random random (7);
        std::vector<double> phase ((size_t) copies, 0.0);
        for (int c = 0; c < copies; ++c)
            phase[(size_t) c] = c == 0 ? 0.0 : random.nextDouble();
        for (int i = 0; i < length; ++i)
        {
            const auto t = i / rate;
            double sum = 0.0;
            for (int c = 0; c < copies; ++c)
            {
                const auto offset = copies == 1 ? 0.0 : detuneCents * (2.0 * c / (copies - 1) - 1.0);
                const auto f = hz (t) * std::exp2 (offset / 1200.0);
                phase[(size_t) c] += f / rate;
                for (int k = 1; k * f < 0.45 * rate && k <= 40; ++k)
                    sum += harmonic (k, t) * std::sin (juce::MathConstants<double>::twoPi * k * phase[(size_t) c]);
            }
            sum = sum / copies + noise * (random.nextDouble() * 2.0 - 1.0);
            for (int ch = 0; ch < channels; ++ch)
                b.setSample (ch, i, (float) (0.3 * level (t) * sum));
        }
        return b;
    }
    const auto saw = [] (int k, double) { return 1.0 / k; };
    const auto steady = [] (double f) { return [f] (double) { return f; }; };

    // Attack (linear) to 1, decay to `sustain` (exponential, 1/e every `decayTau` seconds), held until `releaseAt`,
    // then falling `releaseDbPerSecond`.
    std::function<double (double)> adsr (double attack, double decayTau, double sustain, double releaseAt, double releaseDbPerSecond)
    {
        return [=] (double t)
        {
            if (t < attack)
                return t / attack;
            const auto held = sustain + (1.0 - sustain) * std::exp (-(t - attack) / decayTau);
            if (t < releaseAt)
                return held;
            const auto atRelease = sustain + (1.0 - sustain) * std::exp (-(releaseAt - attack) / decayTau);
            return atRelease * std::pow (10.0, -releaseDbPerSecond * (t - releaseAt) / 20.0);
        };
    }

    // A frame's harmonics in dB against its fundamental.
    std::vector<float> frameHarmonics (const Mimic& a, int frame, int count)
    {
        const auto* data = a.table.samples->data() + (size_t) frame * (size_t) a.table.frameSize;
        const auto levels = synth::Wavetable::harmonicLevels (data, a.table.frameSize, count);
        std::vector<float> db ((size_t) count + 1, -200.0f);
        for (int k = 1; k <= count; ++k)
            db[(size_t) k] = juce::Decibels::gainToDecibels (levels[(size_t) k] / levels[1], -200.0f);
        return db;
    }
}

TEST_CASE ("MIMIC: a held note's pitch, cents and loudness (attack, decay, sustain, release) come back as made", "[mimic]")
{
    const auto hz = 220.0 * std::exp2 (13.0 / 1200.0);   // A3, 13 cents sharp
    const auto sound = note (2.4, steady (hz), adsr (0.15, 0.1, 0.5, 1.6, 120.0), saw);
    const auto a = synth::mimic::analyse (sound, rate, "Held");
    REQUIRE (a.valid);
    CHECK (a.rootNote == 57);
    CHECK (synth::mimic::noteName (a.rootNote) == "A3");
    CHECK_THAT (a.cents, Catch::Matchers::WithinAbs (13.0, 3.0));
    CHECK (a.sustained);
    CHECK_THAT (a.sustain, Catch::Matchers::WithinAbs (0.5, 0.08));
    WARN ("attack " << a.attackMs << " ms, decay " << a.decayMs << " ms, release " << a.releaseMs << " ms, start " << a.start << " s");
    CHECK (a.attackMs > 100.0f);
    CHECK (a.attackMs < 220.0f);
    // Released at 120 dB/s: the 87 dB a curve of -1 covers takes 725 ms.
    CHECK (a.releaseMs > 725.0f * 0.6f);
    CHECK (a.releaseMs < 725.0f * 1.4f);
    // Decaying 1/e every 100 ms to 10% of the way: 230 ms, so about 1 s of a curve of -1.
    CHECK (a.decayMs > 500.0f);
    CHECK (a.decayMs < 1600.0f);
    CHECK_THAT (a.start, Catch::Matchers::WithinAbs (0.0, 0.03));
    CHECK (a.vibratoCents == 0.0f);
    CHECK (a.swoop.empty());
    CHECK (a.detuneCents == 0.0f);
    CHECK (a.noise == 0.0f);
}

TEST_CASE ("MIMIC: a plucked note that dies away is found as dying, and how fast", "[mimic]")
{
    const auto sound = note (2.5, steady (110.0), [] (double t) { return std::min (1.0, t / 0.003) * std::pow (10.0, -30.0 * t / 20.0); }, saw);
    const auto a = synth::mimic::analyse (sound, rate, "Pluck");
    REQUIRE (a.valid);
    CHECK (a.rootNote == 45);
    CHECK_FALSE (a.sustained);
    CHECK (a.sustain == 0.0f);
    WARN ("decay " << a.decayMs << " ms (30 dB/s: 2900 expected)");
    CHECK (a.decayMs > 2900.0f * 0.65f);
    CHECK (a.decayMs < 2900.0f * 1.35f);
    CHECK (a.attackMs < 30.0f);
}

TEST_CASE ("MIMIC: the wavetable holds the sound's own tone (harmonic for harmonic) and how it changes", "[mimic]")
{
    // A steady hollow tone: odd harmonics only, at 1/k.
    {
        const auto sound = note (1.5, steady (196.0), adsr (0.01, 0.05, 1.0, 1.2, 100.0),
                                 [] (int k, double) { return k % 2 == 1 ? 1.0 / k : 0.0; });
        const auto a = synth::mimic::analyse (sound, rate, "Hollow");
        REQUIRE (a.valid);
        REQUIRE (a.table.isValid());
        const auto h = frameHarmonics (a, a.table.numFrames() / 2, 9);
        for (const auto k : { 3, 5, 7, 9 })
        {
            INFO ("harmonic " << k << ": " << h[(size_t) k] << " dB");
            CHECK_THAT (h[(size_t) k], Catch::Matchers::WithinAbs (juce::Decibels::gainToDecibels (1.0 / k), 1.0));
        }
        for (const auto k : { 2, 4, 6, 8 })
            CHECK (h[(size_t) k] < -30.0f);
    }
    // A tone that darkens as it plays (as a filter closing): the last frame is darker than the first.
    {
        const auto sound = note (1.6, steady (164.8), adsr (0.005, 0.05, 1.0, 1.3, 100.0),
                                 [] (int k, double t) { return 1.0 / k * std::exp (-(k - 1) * t * 1.5); });
        const auto a = synth::mimic::analyse (sound, rate, "Closing");
        REQUIRE (a.valid);
        REQUIRE (a.table.numFrames() >= 8);
        const auto first = frameHarmonics (a, 0, 6), last = frameHarmonics (a, a.table.numFrames() - 1, 6);
        INFO ("harmonic 5: first frame " << first[5] << " dB, last " << last[5] << " dB");
        CHECK (last[5] < first[5] - 10.0f);
        CHECK_THAT (a.travelSeconds, Catch::Matchers::WithinAbs (1.3, 0.15));
    }
}

TEST_CASE ("MIMIC: vibrato (its rate, depth and late start) and a swoop into the note", "[mimic]")
{
    {
        const auto sound = note (2.5, [] (double t) { return 330.0 * std::exp2 ((t > 0.6 ? 40.0 * std::sin (juce::MathConstants<double>::twoPi * 5.5 * t) : 0.0) / 1200.0); },
                                 adsr (0.05, 0.1, 0.8, 2.2, 100.0), saw);
        const auto a = synth::mimic::analyse (sound, rate, "Vibrato");
        REQUIRE (a.valid);
        WARN ("vibrato " << a.vibratoHz << " Hz, " << a.vibratoCents << " cents, from " << a.vibratoDelayMs << " ms");
        CHECK_THAT (a.vibratoHz, Catch::Matchers::WithinAbs (5.5, 0.7));
        CHECK_THAT (a.vibratoCents, Catch::Matchers::WithinAbs (40.0, 15.0));
        CHECK (a.vibratoDelayMs > 300.0f);
        CHECK (a.vibratoDelayMs < 900.0f);
    }
    {
        // Starting 7 semitones high and falling into the note over about 100 ms.
        const auto sound = note (1.2, [] (double t) { return 110.0 * std::exp2 (7.0 * std::exp (-t / 0.035) / 12.0); },
                                 adsr (0.005, 0.1, 0.8, 1.0, 100.0), saw);
        const auto a = synth::mimic::analyse (sound, rate, "Swoop");
        REQUIRE (a.valid);
        CHECK (a.rootNote == 45);
        REQUIRE_FALSE (a.swoop.empty());
        WARN ("swoop: " << a.swoop.size() << " points, first " << a.swoop.front() << " semitones");
        CHECK (a.swoop.front() > 2.5f);   // the pitch is read 40 ms at a time: the first instants of a fast swoop blur
        CHECK (a.swoop.back() == 0.0f);
        CHECK (a.swoop.size() < 30);
        CHECK (a.vibratoCents == 0.0f);
    }
}

TEST_CASE ("MIMIC: unison (copies out of tune) and noise are heard; a single clean oscillator has neither", "[mimic]")
{
    for (const auto outer : { 8.0, 20.0 })
    {
        const auto sound = note (2.0, steady (220.0), adsr (0.02, 0.1, 1.0, 1.8, 100.0), saw, 1, 5, outer);
        const auto a = synth::mimic::analyse (sound, rate, "Unison");
        REQUIRE (a.valid);
        WARN ("unison of 5, outer copies " << outer << " cents: measured " << a.detuneCents << " cents");
        CHECK (a.detuneCents > outer / 2.5);
        CHECK (a.detuneCents < outer * 2.5);
        CHECK (a.rootNote == 57);
    }
    const auto noisy = synth::mimic::analyse (note (1.5, steady (220.0), adsr (0.02, 0.1, 1.0, 1.3, 100.0), saw, 1, 1, 0.0, 0.6),
                                              rate, "Noisy");
    REQUIRE (noisy.valid);
    WARN ("noise share " << noisy.noise << ", colour " << noisy.noiseColour);
    CHECK (noisy.noise > 0.05f);
    CHECK (noisy.noiseColour < 0.5f);   // white noise: light
    CHECK (noisy.detuneCents == 0.0f);
}

TEST_CASE ("MIMIC: what it can't use says why (too short, too quiet, no single pitch)", "[mimic]")
{
    juce::AudioBuffer<float> silence (1, 48000);
    silence.clear();
    const auto quiet = synth::mimic::analyse (silence, rate, "Silence");
    CHECK_FALSE (quiet.valid);
    CHECK (quiet.problem.contains ("quiet"));

    juce::AudioBuffer<float> hiss (1, 48000);
    juce::Random random (3);
    for (int i = 0; i < hiss.getNumSamples(); ++i)
        hiss.setSample (0, i, random.nextFloat() - 0.5f);
    const auto noise = synth::mimic::analyse (hiss, rate, "Hiss");
    CHECK_FALSE (noise.valid);
    CHECK (noise.problem.contains ("pitch"));

    juce::AudioBuffer<float> blip (1, 1000);
    blip.clear();
    CHECK_FALSE (synth::mimic::analyse (blip, rate, "Blip").valid);
}

TEST_CASE ("MIMIC: the sound it makes: the wavetable walked by TRAVEL, the loudness, the movement, as chosen", "[mimic]")
{
    const auto sound = note (2.4, [] (double t) { return 220.0 * std::exp2 ((t > 0.5 ? 25.0 * std::sin (juce::MathConstants<double>::twoPi * 5.0 * t) : 0.0) / 1200.0); },
                             adsr (0.15, 0.1, 0.5, 1.6, 120.0), [] (int k, double t) { return 1.0 / k * std::exp (-(k - 1) * t * 0.5); },
                             2, 1, 0.0, 0.1);
    const auto a = synth::mimic::analyse (sound, rate, "Everything");
    REQUIRE (a.valid);
    const auto rig = mimic::makeRig (a);
    const auto value = [&rig] (const juce::String& id) { return rig.parameters.at (id); };
    CHECK (rig.name == "Mimic Everything");
    REQUIRE (rig.wavetables.count (0) == 1);
    CHECK (rig.wavetables.at (0).numFrames() == a.table.numFrames());
    CHECK (value (ParamIDs::osc (0, "Wave")) == (float) synth::OscWave::wavetable);
    CHECK (value (ParamIDs::osc (0, "TrOn")) == 1.0f);
    CHECK_THAT (value (ParamIDs::osc (0, "TrRate")), Catch::Matchers::WithinRel (1.0 / a.travelSeconds, 0.01));
    CHECK (value (ParamIDs::ampSustain) == a.sustain);
    CHECK (value (ParamIDs::ampRelease) == a.releaseMs);
    CHECK (value (ParamIDs::osc (0, "VibAmount")) > 0.0f);
    CHECK (rig.notes.contains ("A3"));

    // Left out: a plain saw, the default loudness, no vibrato.
    mimic::Choices only;
    only.tone = only.envelope = only.vibrato = false;
    const auto plain = mimic::makeRig (a, only);
    CHECK (plain.wavetables.empty());
    CHECK (plain.parameters.count (ParamIDs::ampSustain) == 0);
    CHECK (plain.parameters.count (ParamIDs::osc (0, "VibAmount")) == 0);
}
