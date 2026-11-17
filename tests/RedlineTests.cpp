// REDLINE (0.88.0): the sound as a revving engine in time with the song. Off and THROTTLE 0 untouched bit for bit; the
// throb puts a growl an octave under the note; the scream climbs across each gear and drops back at the gear change
// (whatever the tempo), an octave up and rising by RISE; the level is kept; silence stays silent, a silent side too;
// turned down it rests untouched; in the plugin its matrix destinations, MACRO 3, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/Redline.h"
#include "ui/RackSpecs.h"
#include "ui/RedlinePage.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Redline::Settings;

    std::vector<float> sine (double hz, double seconds, float level = 0.3f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate);
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

    // `note`: the MIDI note sounding. `changeAt`: `changed` from there (samples).
    Run runRedline (std::vector<float> left, std::vector<float> right, Settings s, float note, double bpm = 120.0,
                    double ppqStart = 0.0, int changeAt = -1, Settings changed = {})
    {
        effects::Redline fx;
        fx.prepare (sampleRate, blockSize);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            const auto& now = changeAt >= 0 && (int) start >= changeAt ? changed : s;
            const auto ppq = ppqStart < 0.0 ? -1.0 : ppqStart + (double) start * bpm / 60.0 / sampleRate;
            fx.setNotes (&note, 1);
            fx.process (juce::dsp::AudioBlock<float> (channels, 2, n), now, bpm, ppq);
        }
        return { { left, right }, fx.isResting() };
    }

    Settings full()
    {
        Settings s;
        s.on = true;
        s.throttle = 1.0f;
        s.gear = 1;   // 1 BAR
        return s;
    }

    // The power (dB) of `x` minus `dry` in a window: what REDLINE added.
    double addedDb (const std::vector<float>& x, const std::vector<float>& dry, size_t from, size_t to)
    {
        double sum = 0.0;
        for (auto i = from; i < to; ++i)
            sum += ((double) x[i] - dry[i]) * ((double) x[i] - dry[i]);
        return 10.0 * std::log10 (sum / (double) (to - from) + 1.0e-20);
    }
}

TEST_CASE ("REDLINE: off and THROTTLE 0 leave the sound untouched bit for bit", "[redline]")
{
    const auto left = saw (110.0, 3.0), right = sine (220.0, 3.0);
    auto off = full();
    off.on = false;
    auto run = runRedline (left, right, off, 45.0f);
    CHECK (run.out[0] == left);
    CHECK (run.out[1] == right);
    CHECK (run.resting);
    auto zero = full();
    zero.throttle = 0.0005f;
    run = runRedline (left, right, zero, 45.0f);
    CHECK (run.out[0] == left);
    CHECK (run.resting);
}

TEST_CASE ("REDLINE: the throb growls an octave under the note", "[redline]")
{
    // A pure A3 (220 Hz): the throb's pulse at 110 Hz puts sidebands at 110 Hz (an octave down) and 330 Hz.
    const auto in = sine (220.0, 4.0);
    auto s = full();
    s.scream = 0.0f;
    s.whine = 0.0f;
    s.shift = 0.0f;
    const auto run = runRedline (in, in, s, 57.0f);
    const auto from = (size_t) (1.0 * sampleRate), length = (size_t) (2.0 * sampleRate);
    const auto sub = db (levelAt (run.out[0], 110.0, from, length)), plainSub = db (levelAt (in, 110.0, from, length));
    const auto note = db (levelAt (run.out[0], 220.0, from, length));
    INFO ("sub " << sub << " dB, note " << note << " dB, plain sub " << plainSub);
    CHECK (sub > plainSub + 40.0);
    CHECK (sub > note - 20.0);
    // It follows the note: on A2 the growl is at 55 Hz.
    const auto low = sine (110.0, 4.0);
    const auto run2 = runRedline (low, low, s, 45.0f);
    CHECK (db (levelAt (run2.out[0], 55.0, from, length)) > db (levelAt (low, 55.0, from, length)) + 40.0);
    // Deeper as the revs climb.
    using R = effects::Redline;
    CHECK (R::throbDepth (1.0f, 1.0f) > 1.5f * R::throbDepth (1.0f, 0.0f));
}

TEST_CASE ("REDLINE: the scream climbs across each gear and drops back at the change, at any tempo", "[redline]")
{
    for (const auto bpm : { 120.0, 90.0 })
    {
        INFO (bpm);
        const auto beat = 60.0 / bpm * sampleRate;
        const auto in = saw (110.0, 14.0 * 60.0 / bpm);
        auto s = full();
        s.throb = 0.0f;
        s.whine = 0.0f;
        s.shift = 0.0f;
        const auto run = runRedline (in, in, s, 45.0f, bpm);
        for (int bar = 1; bar < 3; ++bar)
        {
            // What it adds early in the bar (low revs) against late (high revs).
            const auto early = addedDb (run.out[0], in, (size_t) ((4 * bar + 0.25) * beat), (size_t) ((4 * bar + 0.75) * beat));
            const auto late = addedDb (run.out[0], in, (size_t) ((4 * bar + 3.2) * beat), (size_t) ((4 * bar + 3.8) * beat));
            INFO ("bar " << bar << ": early " << early << " dB, late " << late << " dB");
            CHECK (late > early + 10.0);
        }
    }
    // The gear change's dip: just after the bar line the level falls, then comes back.
    {
        const auto beat = 0.5 * sampleRate;
        const auto in = saw (110.0, 7.0);
        auto s = full();
        s.throb = s.scream = s.whine = 0.0f;
        s.shift = 1.0f;
        const auto run = runRedline (in, in, s, 45.0f);
        const auto after = db (rms (run.out[0], (size_t) (8.0 * beat + 0.005 * sampleRate), (size_t) (8.0 * beat + 0.035 * sampleRate)));
        const auto later = db (rms (run.out[0], (size_t) (9.0 * beat), (size_t) (10.0 * beat)));
        INFO ("after the change " << after << " dB, a beat on " << later);
        CHECK (after < later - 4.0);
    }
    using R = effects::Redline;
    CHECK (R::revsAt (0.0, 1) == 0.0);
    CHECK (R::revsAt (2.0, 1) == 0.5);
    CHECK (R::revsAt (5.0, 0) == 0.5);
    CHECK (R::revsAt (12.0, 3) == 0.75);
    CHECK (R::gearCount (9.0, 1) == 2);
}

TEST_CASE ("REDLINE: the scream is an octave up, rising by RISE over the gear", "[redline]")
{
    // A pure A3 through the scream alone: near the top of the gear the octave (440 Hz) at RISE 0; with RISE +12 at the
    // top it has climbed most of an octave more (towards 880 Hz).
    const auto in = sine (220.0, 6.0);
    auto s = full();
    s.throb = s.whine = s.shift = 0.0f;
    s.rise = 0.0f;
    const auto flat = runRedline (in, in, s, 57.0f);
    const auto from = (size_t) (2.0 * 4.0 * 0.5 * sampleRate + 1.6 * sampleRate), length = (size_t) (0.3 * sampleRate);   // bar 3, near its end
    CHECK (db (levelAt (flat.out[0], 440.0, from, length)) > db (levelAt (in, 440.0, from, length)) + 30.0);
    s.rise = 1.0f;
    const auto risen = runRedline (in, in, s, 57.0f);
    // At revs about 0.85 the octave has climbed about 10 semitones: 440 x 2^(10/12) = 784 Hz.
    const auto at = 440.0 * std::pow (2.0, 12.0 * (1.75 / 2.0) / 12.0);
    CHECK (db (levelAt (risen.out[0], at, from, length)) > db (levelAt (risen.out[0], 440.0, from, length)) + 6.0);
}

TEST_CASE ("REDLINE: the level is kept, silence stays silent, a silent side too, and turned down it rests untouched", "[redline]")
{
    const auto in = saw (110.0, 10.0);
    const auto run = runRedline (in, in, full(), 45.0f);
    const auto before = db (rms (in, (size_t) (2.0 * sampleRate), (size_t) (10.0 * sampleRate)));
    const auto after = db (rms (run.out[0], (size_t) (2.0 * sampleRate), (size_t) (10.0 * sampleRate)));
    INFO ("before " << before << " dB, after " << after);
    CHECK (std::abs (after - before) < 2.0);
    for (auto v : run.out[0])
        REQUIRE (std::isfinite (v));

    const std::vector<float> quiet (in.size(), 0.0f);
    const auto silent = runRedline (quiet, quiet, full(), 45.0f);
    CHECK (silent.out[0] == quiet);
    const auto oneSide = runRedline (in, quiet, full(), 45.0f);
    CHECK (oneSide.out[1] == quiet);

    // Turned up, then down at 3 s: resting again, untouched bit for bit.
    auto down = full();
    down.throttle = 0.0f;
    const auto back = runRedline (in, in, full(), 45.0f, 120.0, 0.0, (int) (3.0 * sampleRate), down);
    CHECK (back.resting);
    for (auto i = (size_t) (6.0 * sampleRate); i < in.size(); ++i)
        REQUIRE (back.out[0][i] == in[i]);
}

TEST_CASE ("CPU: REDLINE", "[redline][cpu]")
{
    const auto in = saw (110.0, 8.0);
    const auto begin = std::chrono::steady_clock::now();
    runRedline (in, in, full(), 45.0f);
    const auto percent = 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - begin).count() / 8.0;
    WARN ("REDLINE, stereo: " << percent << "% of a core (the test's own input included)");
    CHECK (percent < 10.0);
}

TEST_CASE ("REDLINE in the plugin: matrix destinations, MACRO 3 to rest, the SIGNATURE slot and its page", "[redline][plugin]")
{
    for (const auto* id : { ParamIDs::redlineThrottle, ParamIDs::redlineThrob, ParamIDs::redlineScream })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Redline");
    }
    CHECK (signatureNames().contains ("REDLINE"));
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
    set (ParamIDs::redlineOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::redlineThrottle, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    set (ParamIDs::macros[2], 1.0f);
    play (1.0);
    CHECK (processor.getRedline().getThrottle() > 0.9f);
    // The throb follows the note played (A3: 220 Hz).
    CHECK (std::abs (processor.getRedline().getNoteHz() - 220.0f) < 2.0f);
    set (ParamIDs::macros[2], 0.0f);
    play (2.0);
    CHECK (processor.getRedline().isResting());

    RedlinePage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 9);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
