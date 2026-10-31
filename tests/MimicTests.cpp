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

//==============================================================================
// In the plugin: the made sound played, compared with the recording; the recording played beside it; the panel.
#include "PluginProcessor.h"
#include "ui/MimicPanel.h"

namespace
{
    constexpr int block = 512;

    struct MimicPlugin
    {
        ParagonX1AudioProcessor processor;
        MimicPlugin()
        {
            processor.setPlayConfigDetails (0, 2, rate, block);
            processor.prepareToPlay (rate, block);
            processor.seedRandomForTest (1);
        }
        // Plays `note` for `held` seconds of `seconds`: the left side.
        std::vector<float> play (int note, double held, double seconds)
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, block);
            juce::MidiBuffer midi;
            const auto blocks = (int) std::ceil (seconds * rate / block);
            const auto offBlock = (int) (held * rate / block);
            for (int b = 0; b < blocks; ++b)
            {
                midi.clear();
                if (b == 0 && note >= 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
                if (b == offBlock && note >= 0)
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
            }
            return out;
        }
    };

    // Harmonic k's level (dB against the fundamental) of `x` from `from` for `count` samples, at fundamental `hz`.
    std::vector<float> harmonicsOf (const std::vector<float>& x, size_t from, size_t count, double hz, int harmonics)
    {
        std::vector<double> amplitude ((size_t) harmonics + 1, 0.0);
        for (int k = 1; k <= harmonics; ++k)
        {
            double re = 0.0, im = 0.0;
            for (size_t i = 0; i < count; ++i)
            {
                const auto w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) i / (double) count);
                const auto phase = juce::MathConstants<double>::twoPi * k * hz * (double) (from + i) / rate;
                re += w * x[from + i] * std::cos (phase);
                im += w * x[from + i] * std::sin (phase);
            }
            amplitude[(size_t) k] = std::sqrt (re * re + im * im);
        }
        std::vector<float> db ((size_t) harmonics + 1, -200.0f);
        for (int k = 1; k <= harmonics; ++k)
            db[(size_t) k] = (float) juce::Decibels::gainToDecibels (amplitude[(size_t) k] / amplitude[1], -200.0);
        return db;
    }
}

TEST_CASE ("MIMIC in the plugin: the made sound plays the recording's tone at its note, and one undo takes it back", "[mimic]")
{
    const auto hz = 196.0;   // G3
    const auto sound = note (1.5, steady (hz), adsr (0.01, 0.05, 1.0, 1.2, 100.0), [] (int k, double) { return k % 2 == 1 ? 1.0 / k : 0.0; });
    const auto a = synth::mimic::analyse (sound, rate, "Hollow");
    REQUIRE (a.valid);
    MimicPlugin p;
    const auto before = p.processor.getCurrentRig().name;
    p.processor.applyMimic (mimic::makeRig (a));
    p.processor.flushUndoStep();
    CHECK (p.processor.getCurrentRig().name == "Mimic Hollow");
    CHECK (p.processor.isRigModified());

    p.processor.prepareToPlay (rate, block);
    const auto out = p.play (a.rootNote, 1.2, 1.4);
    const auto h = harmonicsOf (out, (size_t) (0.4 * rate), (size_t) (0.4 * rate), hz, 8);
    for (const auto k : { 3, 5, 7 })
    {
        INFO ("harmonic " << k << ": " << h[(size_t) k] << " dB (made " << juce::Decibels::gainToDecibels (1.0 / k) << ")");
        CHECK_THAT (h[(size_t) k], Catch::Matchers::WithinAbs (juce::Decibels::gainToDecibels (1.0 / k), 1.5));
    }
    for (const auto k : { 2, 4, 6 })
        CHECK (h[(size_t) k] < -25.0f);

    p.processor.undo();
    CHECK (p.processor.getCurrentRig().name == before);
}

TEST_CASE ("MIMIC in the plugin: the recording plays beside the synth, through OUTPUT, and stops at its end", "[mimic]")
{
    MimicPlugin p;
    juce::AudioBuffer<float> recording (1, (int) (0.2 * rate));
    for (int i = 0; i < recording.getNumSamples(); ++i)
        recording.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / rate));
    REQUIRE (p.processor.setMimicReference (recording, rate));
    CHECK_THAT (p.processor.getMimicReferenceSeconds(), Catch::Matchers::WithinAbs (0.2, 0.001));
    CHECK (p.play (-1, 0.0, 0.05).back() == 0.0f);   // not until asked
    p.processor.playMimicReference();
    CHECK (p.processor.isPlayingMimicReference());
    const auto out = p.play (-1, 0.0, 0.3);
    const auto gain = juce::Decibels::decibelsToGain (-6.0f);   // OUTPUT's default
    for (const auto i : { 0, 100, 5000 })
        CHECK_THAT (out[(size_t) i], Catch::Matchers::WithinAbs (recording.getSample (0, i) * gain, 1.0e-4));
    CHECK_FALSE (p.processor.isPlayingMimicReference());
    CHECK (out.back() == 0.0f);
}

TEST_CASE ("MIMIC's panel: what wasn't found can't be taken; MAKE THIS SOUND puts it in place", "[mimic]")
{
    const auto sound = note (1.5, steady (220.0), adsr (0.02, 0.05, 1.0, 1.2, 100.0), saw);
    const auto a = synth::mimic::analyse (sound, rate, "Plain");
    REQUIRE (a.valid);
    MimicPlugin p;
    MimicPanel panel (p.processor);
    panel.setSize (900, 600);
    panel.setAnalysis (a);
    const auto choices = panel.getChoices();
    CHECK (choices.tone);
    CHECK (choices.envelope);
    panel.makeSound();
    CHECK (p.processor.getCurrentRig().name == "Mimic Plain");
    CHECK (p.processor.getSynthSettings().oscillators[0].wave == synth::OscWave::wavetable);
}

//==============================================================================
// MIMIC part 2: the match score, the refiner.
#include "MimicRefiner.h"
#include "rigs/RigBuilder.h"

namespace
{
    std::vector<float> mono (const juce::AudioBuffer<float>& b)
    {
        return { b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples() };
    }
}

TEST_CASE ("MIMIC's match: the same sound scores 100% at any volume; a different loudness or tone scores lower", "[mimic][match]")
{
    const auto reference = mono (note (1.5, steady (220.0), adsr (0.02, 0.1, 0.6, 1.0, 80.0), saw));
    const auto target = synth::mimic::fingerprint (reference, rate);

    const auto same = synth::mimic::compare (target, synth::mimic::fingerprint (reference, rate));
    CHECK (same.match > 99.9);
    CHECK_THAT (same.gainDb, Catch::Matchers::WithinAbs (0.0, 0.01));

    auto quieter = reference;
    for (auto& s : quieter)
        s *= 0.5f;
    const auto q = synth::mimic::compare (target, synth::mimic::fingerprint (quieter, rate));
    CHECK (q.match > 99.0);
    CHECK_THAT (q.gainDb, Catch::Matchers::WithinAbs (6.02, 0.1));

    const auto slow = synth::mimic::compare (target, synth::mimic::fingerprint (mono (note (1.5, steady (220.0), adsr (0.4, 0.1, 0.6, 1.0, 80.0), saw)), rate));
    const auto hollow = synth::mimic::compare (target, synth::mimic::fingerprint (
        mono (note (1.5, steady (220.0), adsr (0.02, 0.1, 0.6, 1.0, 80.0), [] (int k, double) { return k % 2 == 1 ? 1.0 / k : 0.0; })), rate));
    WARN ("match: same " << same.match << "%, a slow attack " << slow.match << "% (loudness " << slow.loudnessDb
          << " dB apart), a hollow tone " << hollow.match << "% (tone " << hollow.toneDb << " dB apart)");
    CHECK (slow.match < 95.0);
    CHECK (slow.loudnessDb > slow.toneDb);
    CHECK (hollow.match < 90.0);
    CHECK (hollow.toneDb > hollow.loudnessDb);
}

TEST_CASE ("MIMIC's refiner: a sound PR7-K made with known settings, rebuilt and refined: the match rises, the envelope nears the truth", "[mimic][match]")
{
    // The truth: a saw, a slow attack, a straight-line decay to 40% and release (curves 0, where MIMIC guesses -1).
    rigbuilder::Builder truth ("Truth", 1, {}, {});
    truth.ampEnvelope (250.0f, 600.0f, 0.4f, 700.0f);
    MimicRefiner refiner;
    synth::mimic::Analysis shape;   // only what play() needs: the note, how long it's held, how long to record
    shape.rootNote = 57;
    shape.start = 0.0;
    shape.releaseStart = 1.4;
    shape.seconds = 2.6;
    const auto recording = refiner.play (truth.rig, shape);
    juce::AudioBuffer<float> buffer (1, (int) recording.size());
    buffer.copyFrom (0, 0, recording.data(), (int) recording.size());
    const auto a = synth::mimic::analyse (buffer, MimicRefiner::renderRate, "Truth");
    REQUIRE (a.valid);
    CHECK (a.rootNote == 57);

    const auto made = mimic::makeRig (a);
    refiner.begin (made, a, {});
    REQUIRE (refiner.isRefining());
    while (refiner.step()) {}
    const auto before = refiner.getFirstScore().match, after = refiner.getBestScore().match;
    const auto& best = refiner.getBest();
    const auto value = [] (const Rig& rig, const char* id) { return rig.parameters.at (id); };
    WARN ("match " << before << "% -> " << after << "% in " << refiner.getDone() << " plays; attack " << value (made, ParamIDs::ampAttack)
          << " -> " << value (best, ParamIDs::ampAttack) << " ms (truth 250), sustain " << value (made, ParamIDs::ampSustain) << " -> "
          << value (best, ParamIDs::ampSustain) << " (truth 0.4), release " << value (made, ParamIDs::ampRelease) << " -> "
          << value (best, ParamIDs::ampRelease) << " ms (truth 700, straight)");
    CHECK (after >= before);
    CHECK (after > 85.0);
    CHECK (refiner.getDone() <= refiner.getPlanned());
    // It refines the sound, not each setting: the attack and release come nearer the truth; the sustain can trade off
    // against the falls' shapes (MIMIC's natural curves against this sound's straight lines) and wander a little.
    CHECK (std::abs (value (best, ParamIDs::ampAttack) - 250.0f) <= std::abs (value (made, ParamIDs::ampAttack) - 250.0f));
    CHECK (std::abs (value (best, ParamIDs::ampRelease) - 700.0f) < std::abs (value (made, ParamIDs::ampRelease) - 700.0f));
    // Its LEVEL matches the recording's volume: what's left to make up is next to nothing.
    CHECK (std::abs (refiner.getBestScore().gainDb) < 1.5);
}

TEST_CASE ("MIMIC's panel, part 2: MAKE shows a match; REFINE puts the refined sound in place, one undo from the made one", "[mimic][match]")
{
    const auto sound = note (1.6, steady (196.0), adsr (0.12, 0.15, 0.5, 1.2, 60.0), saw);
    const auto a = synth::mimic::analyse (sound, rate, "Panel");
    REQUIRE (a.valid);
    MimicPlugin p;
    MimicPanel panel (p.processor);
    panel.setSize (1000, 640);
    panel.setAnalysis (a);
    panel.makeSound();
    p.processor.flushUndoStep();
    REQUIRE (panel.getScore().has_value());
    const auto first = panel.getScore()->match;
    const auto madeLevel = p.processor.getCurrentRig().name;
    panel.refine();
    panel.finishRefining();
    p.processor.flushUndoStep();
    REQUIRE (panel.getScore().has_value());
    WARN ("panel: match " << first << "% -> " << panel.getScore()->match << "%");
    CHECK (panel.getScore()->match >= first);
    CHECK (p.processor.getCurrentRig().name == "Mimic Panel");
    CHECK (p.processor.canUndo());
}
