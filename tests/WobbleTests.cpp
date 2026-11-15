// WOBBLE (0.97.0): the dubstep wobble locked to the song. Off and AMOUNT 0 untouched bit for bit; the filter opens and
// shuts on the pattern's speed for each beat, in time with the song at any tempo; the pattern's speeds really change
// beat by beat; TALK reshapes the tone as a vowel; the level is kept; silence stays silent; turned down it rests
// untouched; in the plugin its matrix destinations, MACRO 3, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/Wobble.h"
#include "ui/RackSpecs.h"
#include "ui/WobblePage.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::Wobble::Settings;
    using W = effects::Wobble;

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
    Run runWobble (std::vector<float> left, std::vector<float> right, Settings s, double bpm = 120.0, double ppqStart = 0.0,
                   int changeAt = -1, Settings changed = {})
    {
        W fx;
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

    // The share of a window's power that's high (by a first difference: brighter, more).
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

    // Brightness in a short window round `beat` (a sixteenth of a wobble either side at this speed).
    double brightnessAt (const std::vector<float>& x, double beat, double rate, double bpm)
    {
        const auto samplesPerBeat = 60.0 / bpm * sampleRate;
        const auto half = samplesPerBeat / rate / 16.0;
        const auto centre = beat * samplesPerBeat;
        return brightness (x, (size_t) (centre - half), (size_t) (centre + half));
    }

    Settings full()
    {
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.resonance = 0.0f;
        s.talk = 0.0f;
        s.drive = 0.0f;
        return s;
    }
}

TEST_CASE ("WOBBLE: off and AMOUNT 0 leave the sound untouched bit for bit", "[wobble]")
{
    const auto left = noise (3.0), right = saw (220.0, 3.0);
    Settings off = full();
    off.on = false;
    auto run = runWobble (left, right, off);
    CHECK (run.out[0] == left);
    CHECK (run.out[1] == right);
    CHECK (run.resting);
    Settings zero = full();
    zero.amount = 0.0005f;
    run = runWobble (left, right, zero);
    CHECK (run.out[0] == left);
    CHECK (run.resting);
}

TEST_CASE ("WOBBLE: the filter opens and shuts on each beat's speed, in time with the song at any tempo", "[wobble]")
{
    for (const auto bpm : { 140.0, 100.0 })
    {
        INFO (bpm);
        const auto in = noise (12.0 * 60.0 / bpm);
        // STEADY eighths: open in the middle of each eighth, shut where each starts.
        auto s = full();
        s.pattern = 0;
        auto run = runWobble (in, in, s, bpm);
        for (int beat = 2; beat < 8; ++beat)
        {
            INFO (beat);
            CHECK (brightnessAt (run.out[0], beat + 0.25, 2.0, bpm) > 6.0 * brightnessAt (run.out[0], beat + 0.5, 2.0, bpm));
            CHECK (brightnessAt (run.out[0], beat + 0.75, 2.0, bpm) > 6.0 * brightnessAt (run.out[0], beat + 1.0, 2.0, bpm));
        }
        // TRIPLETS: open at a sixth of a beat, not at a quarter (where STEADY would be).
        s.pattern = 2;
        run = runWobble (in, in, s, bpm);
        CHECK (brightnessAt (run.out[0], 4.0 + 1.0 / 6.0, 3.0, bpm) > 6.0 * brightnessAt (run.out[0], 4.0 + 1.0 / 3.0, 3.0, bpm));
        // Stopped, it counts for itself from the pattern's start.
        s.pattern = 0;
        run = runWobble (in, in, s, bpm, -1.0);
        CHECK (brightnessAt (run.out[0], 3.25, 2.0, bpm) > 6.0 * brightnessAt (run.out[0], 3.5, 2.0, bpm));
    }
}

TEST_CASE ("WOBBLE: the pattern's speed changes beat by beat", "[wobble]")
{
    // ROLLS: quarters, eighths, sixteenths, then 32nds. Each beat is shut where its own speed says and open half a
    // wobble later.
    const auto bpm = 120.0;
    const auto in = noise (8.0);
    auto s = full();
    s.pattern = 4;
    const auto run = runWobble (in, in, s, bpm);
    for (int bar = 1; bar < 3; ++bar)
        for (int b = 0; b < 8; ++b)
        {
            const auto beat = 8.0 * bar + b;
            const auto rate = (double) W::patterns[4][(size_t) b];
            INFO (beat << " at " << rate << " a beat");
            const auto openAt = beat + 0.5 / rate, shutAt = beat + 1.0 / rate;
            CHECK (brightnessAt (run.out[0], openAt, rate, bpm) > 4.0 * brightnessAt (run.out[0], shutAt, rate, bpm));
        }
    // The table's arithmetic, and every speed whole a beat (so no speed change jumps).
    CHECK (W::rateAt (0.5, 3) == 2.0f);
    CHECK (W::rateAt (4.5, 3) == 3.0f);
    CHECK (W::rateAt (14.9, 3) == 4.0f);
    CHECK (W::rateName (3.0f) == "1/8 T");
    for (const auto& p : W::patterns)
        for (size_t b = 0; b < p.size(); ++b)
        {
            const auto rate = p[b];
            CHECK ((rate == std::floor (rate) || (rate == 0.5f && b % 2 == 0 && p[b + 1] == 0.5f)));
        }
    for (int shape = 0; shape < 3; ++shape)
    {
        INFO (shape);
        CHECK (W::openAt (0.0, 0, shape) < 0.01);   // shut as each wobble starts
        CHECK (W::openAt (0.999, 0, shape) < 0.1);
    }
}

TEST_CASE ("WOBBLE: TALK moves the tone as a vowel, OO shut to EH open", "[wobble]")
{
    // A low saw through a wobble whose filter stays high (LOW 800 Hz), slow (HALF-TIME at 60 BPM: one a second), so
    // what changes is mostly TALK's vowel. Shut, OO keeps the third harmonic (near OO's 350 Hz) over the tenth; open,
    // towards EH, the tenth (1.1 kHz, near AH / EH's second formant) comes up.
    const auto bpm = 60.0;
    const auto in = saw (110.0, 8.0);
    auto s = full();
    s.pattern = 1;
    s.low = 1.0f;
    s.range = 1.0f;
    s.talk = 1.0f;
    const auto talking = runWobble (in, in, s, bpm);
    s.talk = 0.0f;
    const auto plain = runWobble (in, in, s, bpm);
    const auto window = (size_t) (0.12 * sampleRate);
    const auto tilt = [window] (const std::vector<float>& x, double second)
    {
        const auto from = (size_t) (second * sampleRate) - window / 2;
        return db (levelAt (x, 1100.0, from, window)) - db (levelAt (x, 330.0, from, window));
    };
    double talkSwing = 0.0, plainSwing = 0.0;
    for (int beat = 3; beat < 7; ++beat)
    {
        talkSwing += tilt (talking.out[0], beat + 0.5) - tilt (talking.out[0], beat + 0.06);
        plainSwing += tilt (plain.out[0], beat + 0.5) - tilt (plain.out[0], beat + 0.06);
    }
    talkSwing /= 4.0;
    plainSwing /= 4.0;
    WARN ("TALK: the tenth harmonic against the third swings " << talkSwing << " dB shut to open, " << plainSwing << " dB without TALK");
    CHECK (talkSwing > plainSwing + 6.0);
}

TEST_CASE ("WOBBLE: the level is kept, silence stays silent, and turned down it rests untouched", "[wobble]")
{
    auto s = full();
    s.resonance = 0.6f;
    s.talk = 0.6f;
    s.drive = 0.5f;
    const auto in = saw (55.0, 10.0);
    const auto run = runWobble (in, in, s, 140.0);
    const auto before = db (rms (in, (size_t) (2.0 * sampleRate), (size_t) (10.0 * sampleRate)));
    const auto after = db (rms (run.out[0], (size_t) (2.0 * sampleRate), (size_t) (10.0 * sampleRate)));
    WARN ("WOBBLE on a 55 Hz saw: " << after - before << " dB");
    CHECK (std::abs (after - before) < 2.5);
    for (auto v : run.out[0])
        REQUIRE (std::isfinite (v));

    const std::vector<float> quiet ((size_t) (3.0 * sampleRate), 0.0f);
    const auto silent = runWobble (quiet, quiet, s);
    CHECK (silent.out[0] == quiet);

    // One side silent stays silent.
    const auto oneSide = runWobble (in, quiet, s);
    for (auto v : oneSide.out[1])
        REQUIRE (v == 0.0f);

    // Turned up, then down at 3 s: resting again, untouched bit for bit.
    auto down = s;
    down.amount = 0.0f;
    const auto back = runWobble (in, in, s, 140.0, 0.0, (int) (3.0 * sampleRate), down);
    CHECK (back.resting);
    for (auto i = (size_t) (5.0 * sampleRate); i < in.size(); ++i)
        REQUIRE (back.out[0][i] == in[i]);
}

TEST_CASE ("CPU: WOBBLE", "[wobble][cpu]")
{
    auto s = full();
    s.resonance = 0.8f;
    s.talk = 0.8f;
    s.drive = 0.6f;
    s.pattern = 5;
    const auto in = saw (55.0, 8.0);
    const auto begin = std::chrono::steady_clock::now();
    runWobble (in, in, s, 140.0);
    const auto percent = 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - begin).count() / 8.0;
    WARN ("WOBBLE, stereo, TALK and DRIVE up: " << percent << "% of a core (the test's own input included)");
    CHECK (percent < 10.0);
}

TEST_CASE ("WOBBLE in the plugin: matrix destinations, MACRO 3 to rest, the SIGNATURE slot and its page", "[wobble][plugin]")
{
    for (const auto* id : { ParamIDs::wobbleAmount, ParamIDs::wobbleTalk, ParamIDs::wobbleResonance })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Wobble");
    }
    CHECK (signatureNames().contains ("WOBBLE"));
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
    processor.getKeyboardState().noteOn (1, 45, 0.9f);
    set (ParamIDs::wobbleOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::wobbleAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    set (ParamIDs::macros[2], 1.0f);
    play (1.0);
    CHECK (processor.getWobble().getAmount() > 0.9f);
    CHECK (processor.getWobble().getCutoffHz() < 19000.0f);
    set (ParamIDs::macros[2], 0.0f);
    play (2.0);
    CHECK (processor.getWobble().isResting());

    WobblePage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 9);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
