// SWEEP (0.87.0): a resonant analogue sweep locked to the song. Off and AMOUNT 0 untouched bit for bit; the sweep's
// brightness follows its shape in time with the song (whatever the tempo); RESONANCE makes a peak at the cutoff; the
// level is kept; silence stays silent; turned down it rests untouched; in the plugin its matrix destinations, MACRO 3,
// the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/Sweep.h"
#include "ui/RackSpecs.h"
#include "ui/SweepPage.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Sweep::Settings;

    std::vector<float> noise (double seconds, float level = 0.2f)
    {
        juce::Random random (1);
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (auto& v : x)
            v = level * (2.0f * random.nextFloat() - 1.0f);
        return x;
    }

    std::vector<float> saw (double hz, double seconds, float level = 0.3f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        auto phase = 0.0;
        for (auto& v : x)
        {
            v = level * (float) (2.0 * phase - 1.0);
            phase += hz / sampleRate;
            phase -= std::floor (phase);
        }
        return x;
    }

    struct Run
    {
        std::array<std::vector<float>, 2> out;
        bool resting = false;
    };

    // `ppqStart` -1: stopped (its own count). `changeAt`: `changed` from there (samples).
    Run runSweep (std::vector<float> left, std::vector<float> right, Settings s, double bpm = 120.0, double ppqStart = 0.0,
                  int changeAt = -1, Settings changed = {})
    {
        effects::Sweep fx;
        fx.prepare (sampleRate, blockSize);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            const auto& now = changeAt >= 0 && (int) start >= changeAt ? changed : s;
            const auto ppq = ppqStart < 0.0 ? -1.0 : ppqStart + (double) start * bpm / 60.0 / sampleRate;
            fx.process (juce::dsp::AudioBlock<float> (channels, 2, n), now, bpm, ppq);
        }
        return { { left, right }, fx.isResting() };
    }

    // The share of a window's power above 2 kHz (by a simple first difference: brighter, more).
    double brightness (const std::vector<float>& x, size_t from, size_t to)
    {
        double all = 0.0, edge = 0.0;
        for (auto i = from + 1; i < to; ++i)
        {
            all += (double) x[i] * x[i];
            const auto d = (double) x[i] - x[i - 1];
            edge += d * d;
        }
        return edge / juce::jmax (1.0e-12, all);
    }

    Settings full()
    {
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.length = 1;       // 1 BAR
        s.resonance = 0.0f;
        s.phaser = 0.0f;
        return s;
    }
}

TEST_CASE ("SWEEP: off and AMOUNT 0 leave the sound untouched bit for bit", "[sweep]")
{
    const auto left = noise (3.0), right = saw (220.0, 3.0);
    Settings off = full();
    off.on = false;
    auto run = runSweep (left, right, off);
    CHECK (run.out[0] == left);
    CHECK (run.out[1] == right);
    CHECK (run.resting);
    Settings zero = full();
    zero.amount = 0.0005f;
    run = runSweep (left, right, zero);
    CHECK (run.out[0] == left);
    CHECK (run.resting);
}

TEST_CASE ("SWEEP: the brightness follows the shape, in time with the song at any tempo", "[sweep]")
{
    for (const auto bpm : { 120.0, 90.0 })
    {
        INFO (bpm);
        const auto beat = 60.0 / bpm * sampleRate;
        const auto in = noise (16.0 * 60.0 / bpm);
        // UP over a bar: dark at each bar's start, bright at its end.
        auto s = full();
        s.shape = 0;
        auto run = runSweep (in, in, s, bpm);
        for (int bar = 1; bar < 3; ++bar)
        {
            const auto early = brightness (run.out[0], (size_t) ((4 * bar + 0.25) * beat), (size_t) ((4 * bar + 0.75) * beat));
            const auto late = brightness (run.out[0], (size_t) ((4 * bar + 3.2) * beat), (size_t) ((4 * bar + 3.8) * beat));
            CHECK (late > 8.0 * early);
        }
        // DOWN the other way round.
        s.shape = 1;
        run = runSweep (in, in, s, bpm);
        CHECK (brightness (run.out[0], (size_t) (4.25 * beat), (size_t) (4.75 * beat))
               > 8.0 * brightness (run.out[0], (size_t) (7.2 * beat), (size_t) (7.8 * beat)));
        // UP-DOWN: from the bottom, up over a bar to the top at beat 4 (and 12), down to the bottom at beat 8.
        s.shape = 2;
        run = runSweep (in, in, s, bpm);
        CHECK (brightness (run.out[0], (size_t) (11.6 * beat), (size_t) (12.4 * beat))
               > 8.0 * brightness (run.out[0], (size_t) (7.6 * beat), (size_t) (8.4 * beat)));
    }
    // The shape's arithmetic.
    using W = effects::Sweep;
    CHECK (W::positionAt (0.0, 0, 1) == 0.0);
    CHECK (W::positionAt (2.0, 0, 1) == 0.5);
    CHECK (W::positionAt (4.0, 2, 1) == 1.0);
    CHECK (W::positionAt (6.0, 2, 1) == 0.5);
    CHECK (W::positionAt (1.0, 1, 0) == 0.5);
}

TEST_CASE ("SWEEP: RESONANCE makes a peak at the cutoff", "[sweep]")
{
    // A slow sweep held near its top (where the resonance is all there): compare the band at the cutoff with and
    // without resonance.
    auto s = full();
    s.length = 4;      // 8 BARS (32 beats): barely moving over the second measured
    s.shape = 0;       // UP
    s.center = 0.3f;
    const auto in = noise (3.0);
    const auto plain = runSweep (in, in, s, 120.0, 26.0);   // the measured second is beats 28-30: 88-94% of the way up
    s.resonance = 1.0f;
    const auto ringing = runSweep (in, in, s, 120.0, 26.0);
    const auto hz = effects::Sweep::sweepHz (29.0 / 32.0, s);   // the cutoff in the middle of that second
    const auto from = (size_t) (1.0 * sampleRate), length = (size_t) (1.0 * sampleRate);
    // Level-kept outputs: the resonance's peak stands out from the band an octave below it.
    const auto peakPlain = db (levelAt (plain.out[0], hz, from, length)) - db (levelAt (plain.out[0], hz / 2.0, from, length));
    const auto peakRinging = db (levelAt (ringing.out[0], hz, from, length)) - db (levelAt (ringing.out[0], hz / 2.0, from, length));
    CHECK (peakRinging > peakPlain + 8.0);
}

TEST_CASE ("SWEEP: the level is kept, silence stays silent, and turned down it rests untouched", "[sweep]")
{
    auto s = full();
    s.resonance = 0.6f;
    s.phaser = 0.5f;
    const auto in = saw (110.0, 10.0);
    const auto run = runSweep (in, in, s);
    const auto before = db (rms (in, (size_t) (2.0 * sampleRate), (size_t) (10.0 * sampleRate)));
    const auto after = db (rms (run.out[0], (size_t) (2.0 * sampleRate), (size_t) (10.0 * sampleRate)));
    CHECK (std::abs (after - before) < 2.0);
    for (auto v : run.out[0])
        REQUIRE (std::isfinite (v));

    const std::vector<float> quiet ((size_t) (3.0 * sampleRate), 0.0f);
    const auto silent = runSweep (quiet, quiet, s);
    CHECK (silent.out[0] == quiet);

    // Turned up, then down at 3 s: resting again, untouched bit for bit.
    auto down = s;
    down.amount = 0.0f;
    const auto back = runSweep (in, in, s, 120.0, 0.0, (int) (3.0 * sampleRate), down);
    CHECK (back.resting);
    for (auto i = (size_t) (5.0 * sampleRate); i < in.size(); ++i)
        REQUIRE (back.out[0][i] == in[i]);
}

TEST_CASE ("CPU: SWEEP", "[sweep][cpu]")
{
    auto s = full();
    s.resonance = 0.8f;
    s.phaser = 0.6f;
    const auto in = saw (110.0, 8.0);
    const auto begin = std::chrono::steady_clock::now();
    runSweep (in, in, s);
    const auto percent = 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - begin).count() / 8.0;
    WARN ("SWEEP, stereo: " << percent << "% of a core (the test's own input included)");
    CHECK (percent < 10.0);
}

TEST_CASE ("SWEEP in the plugin: matrix destinations, MACRO 3 to rest, the SIGNATURE slot and its page", "[sweep][plugin]")
{
    for (const auto* id : { ParamIDs::sweepAmount, ParamIDs::sweepResonance, ParamIDs::sweepCenter })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Sweep");
    }
    CHECK (signatureNames().contains ("SWEEP"));
    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    processor.seedRandomForTest (1);
    const auto set = [&processor] (const char* id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    const auto play = [&processor] (double seconds)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    };
    processor.getKeyboardState().noteOn (1, 57, 0.9f);
    set (ParamIDs::sweepOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::sweepAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    set (ParamIDs::macros[2], 1.0f);
    play (1.0);
    CHECK (processor.getSweep().getAmount() > 0.9f);
    CHECK (processor.getSweep().getCutoffHz() < 19000.0f);
    set (ParamIDs::macros[2], 0.0f);
    play (2.0);
    CHECK (processor.getSweep().isResting());

    SweepPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 9);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
