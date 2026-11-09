// HYPERJUMP (0.86.0): a build and a drop in time with the song. Off and AMOUNT 0: untouched bit for bit; LOOP builds in
// the second half of every 2 x LENGTH bars and is untouched in the first; the climb steps up a beat at a time to RISE;
// the chop gets faster; the image narrows; the drop lands on the bar line with a boom (none in silence); ONCE builds
// once and stays landed; turned down it rests; in the plugin MACRO 3 moves it through the matrix and its page draws.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/HyperJump.h"
#include "ui/HyperJumpPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>

namespace
{
    using Settings = effects::HyperJump::Settings;
    constexpr double bpm = 120.0;                         // a beat is half a second
    constexpr double samplesPerBeat = 0.5 * sampleRate;

    std::vector<float> sine (double hz, double seconds, float level = 0.3f, double phase = 0.0)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate + phase);
        return x;
    }

    struct Run
    {
        std::array<std::vector<float>, 2> out;
        bool resting = false;
        int drops = 0;
    };

    // `ppqStart` -1: the song stopped (its own count). `changeAt` (samples): `changed` from there.
    Run runJump (std::vector<float> left, std::vector<float> right, Settings s, double ppqStart = 0.0, int changeAt = -1, Settings changed = {})
    {
        effects::HyperJump fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (1);
        Run run;
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            const auto& now = changeAt >= 0 && (int) start >= changeAt ? changed : s;
            const auto ppq = ppqStart < 0.0 ? -1.0 : ppqStart + (double) start / samplesPerBeat;
            fx.process (juce::dsp::AudioBlock<float> (channels, 2, n), now, bpm, ppq);
        }
        run.out = { left, right };
        run.resting = fx.isResting();
        run.drops = fx.getDrops();
        return run;
    }

    size_t beat (double b) { return (size_t) (b * samplesPerBeat); }

    double rmsOf (const std::vector<float>& x, double fromBeat, double toBeat) { return rms (x, beat (fromBeat), beat (toBeat)); }
}

TEST_CASE ("HYPERJUMP: off and AMOUNT 0 leave the sound untouched bit for bit", "[hyperjump]")
{
    const auto left = sine (220.0, 6.0), right = sine (330.0, 6.0);
    Settings off;
    off.amount = 1.0f;   // set up but switched off
    auto run = runJump (left, right, off);
    CHECK (run.out[0] == left);
    CHECK (run.out[1] == right);
    CHECK (run.resting);

    Settings zero;
    zero.on = true;
    zero.amount = 0.0005f;   // under 0.001 counts as 0
    run = runJump (left, right, zero);
    CHECK (run.out[0] == left);
    CHECK (run.resting);
}

TEST_CASE ("HYPERJUMP: LOOP leaves the first half untouched, climbs a step a beat, and drops on the bar line", "[hyperjump]")
{
    // LENGTH 1 BAR: bar 1 (beats 0-4) untouched, bar 2 (beats 4-8) the build, the drop at beat 8.
    Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.length = 0;
    s.rise = 1.0f;    // +12 by the last beat: 0, +4, +8, +12
    s.chop = 0.0f;    // no gate, to measure the pitch
    s.hit = 0.0f;
    const auto tone = sine (220.0, 6.0);
    auto run = runJump (tone, tone, s);
    // The first bar exactly as it came in (after the smoothing's first few ms the effect is awake but not building).
    for (size_t i = beat (0.1); i < beat (3.9); ++i)
        REQUIRE (run.out[0][i] == tone[i]);
    const auto semitonesIn = [&] (double fromBeat, double toBeat)
    {
        return 12.0 * std::log2 (frequencyOf (run.out[0], beat (fromBeat), beat (toBeat)) / 220.0);
    };
    CHECK_THAT (semitonesIn (4.2, 4.9), WithinAbs (0.0, 0.15));
    CHECK_THAT (semitonesIn (5.2, 5.9), WithinAbs (4.0, 0.15));
    CHECK_THAT (semitonesIn (6.2, 6.9), WithinAbs (8.0, 0.15));
    CHECK_THAT (semitonesIn (7.2, 7.9), WithinAbs (12.0, 0.15));
    // After the drop: back down at once (5 ms glide), and untouched again for the next clean bar.
    CHECK_THAT (semitonesIn (8.1, 8.9), WithinAbs (0.0, 0.15));
    for (size_t i = beat (8.2); i < beat (11.9); ++i)
        REQUIRE (run.out[0][i] == tone[i]);
    CHECK (run.drops >= 1);
}

TEST_CASE ("HYPERJUMP: the chop speeds up through the build and the image narrows", "[hyperjump]")
{
    Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.length = 0;   // 1 bar: build beats 4-8
    s.rise = 0.0f;
    s.chop = 1.0f;
    s.hit = 0.0f;
    // The chop on a centred tone (the image's narrowing measured below on a wide one).
    const auto tone = sine (1760.0, 5.0);   // many cycles in each 3 ms window, so the windows read the gate, not the wave
    const auto chopped = runJump (tone, tone, s);
    // Count the gate's openings in each beat of the build, against that beat's own loudest moment (the gate starts
    // shallow and deepens): quarters (1), eighths (2), sixteenths (4), 32nds (8).
    const auto openings = [&] (double fromBeat)
    {
        const auto window = (size_t) (0.003 * sampleRate);
        std::vector<double> levels;
        for (size_t i = beat (fromBeat); i + window < beat (fromBeat + 1.0); i += window / 2)
            levels.push_back (rms (chopped.out[0], i, i + window));
        const auto top = *std::max_element (levels.begin(), levels.end());
        int count = 0;
        bool open = false;   // each beat starts with the gate opening
        for (auto level : levels)
        {
            if (! open && level > 0.8 * top)
                ++count;
            open = level > 0.8 * top ? true : level < 0.55 * top ? false : open;
        }
        return count;
    };
    CHECK (openings (5.0) == 2);
    CHECK (openings (6.0) == 4);
    CHECK (openings (7.0) == 8);
    // A tone on the left, another on the right: wide.
    const auto left = sine (220.0, 5.0), right = sine (330.0, 5.0, 0.3f, 1.0);
    const auto run = runJump (left, right, s);
    // The image: the difference of the sides much smaller late in the build than in the clean bar.
    const auto sideOf = [&] (const std::array<std::vector<float>, 2>& x, double fromBeat, double toBeat)
    {
        std::vector<float> side (x[0].size());
        for (size_t i = 0; i < side.size(); ++i)
            side[i] = 0.5f * (x[0][i] - x[1][i]);
        std::vector<float> mid (x[0].size());
        for (size_t i = 0; i < mid.size(); ++i)
            mid[i] = 0.5f * (x[0][i] + x[1][i]);
        return rms (side, beat (fromBeat), beat (toBeat)) / juce::jmax (1.0e-9, rms (mid, beat (fromBeat), beat (toBeat)));
    };
    CHECK (sideOf (run.out, 7.2, 7.95) < 0.25 * sideOf (run.out, 1.0, 3.0));
}

TEST_CASE ("HYPERJUMP: the drop's boom lands on the bar line, follows the music, and never sounds in silence", "[hyperjump]")
{
    Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.length = 0;
    s.chop = 0.0f;
    s.rise = 0.0f;
    s.hit = 1.0f;
    const auto tone = sine (880.0, 6.0);
    auto hit = runJump (tone, tone, s);
    s.hit = 0.0f;
    auto none = runJump (tone, tone, s);
    // The boom (low, around 40-70 Hz) in the 0.3 s after beat 8, none before it.
    const auto lowAfter = levelAt (hit.out[0], 55.0, beat (8.0), (size_t) (0.25 * sampleRate));
    const auto lowNone = levelAt (none.out[0], 55.0, beat (8.0), (size_t) (0.25 * sampleRate));
    CHECK (db (lowAfter) > db (lowNone) + 20.0);
    CHECK (db (levelAt (hit.out[0], 55.0, beat (6.5), (size_t) (0.25 * sampleRate))) < db (lowAfter) - 20.0);
    // Its peak no louder than the music's.
    float peak = 0.0f;
    for (auto v : hit.out[0])
        peak = juce::jmax (peak, std::abs (v));
    CHECK (peak < 1.0f);

    // Silence in: silence out, build and drop included.
    s.hit = 1.0f;
    s.rise = 1.0f;
    s.chop = 1.0f;
    const std::vector<float> quiet ((size_t) (6.0 * sampleRate), 0.0f);
    const auto silent = runJump (quiet, quiet, s);
    CHECK (silent.out[0] == quiet);
    CHECK (silent.out[1] == quiet);
}

TEST_CASE ("HYPERJUMP: ONCE builds from the next bar line, drops once, stays landed; turned down it rests", "[hyperjump]")
{
    Settings s;
    s.on = true;
    s.amount = 0.0f;
    s.mode = 1;     // ONCE
    s.length = 0;   // 1 bar
    s.rise = 1.0f;
    s.chop = 0.0f;
    s.hit = 0.0f;
    Settings up = s;
    up.amount = 1.0f;
    const auto tone = sine (220.0, 10.0);
    // AMOUNT comes up at beat 1.5: the build waits for beat 4, drops at 8; untouched after, though AMOUNT stays up.
    auto run = runJump (tone, tone, s, 0.0, (int) beat (1.5), up);
    for (size_t i = beat (0.0); i < beat (3.9); ++i)
        REQUIRE (run.out[0][i] == tone[i]);
    CHECK_THAT (12.0 * std::log2 (frequencyOf (run.out[0], beat (7.2), beat (7.9)) / 220.0), WithinAbs (12.0, 0.15));
    for (size_t i = beat (8.5); i < beat (19.5); ++i)
        REQUIRE (run.out[0][i] == tone[i]);
    CHECK (run.drops == 1);

    // LOOP turned up, then back to 0 mid-build: it rests again, untouched bit for bit.
    Settings loop = up;
    loop.mode = 0;
    loop.hit = 1.0f;
    Settings down = loop;
    down.amount = 0.0f;
    run = runJump (tone, tone, loop, 0.0, (int) beat (6.0), down);
    CHECK (run.resting);
    for (size_t i = beat (12.0); i < tone.size(); ++i)
        REQUIRE (run.out[0][i] == tone[i]);
}

TEST_CASE ("HYPERJUMP: with the song stopped it starts a build at once and drops after LENGTH", "[hyperjump]")
{
    Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.length = 0;
    s.rise = 1.0f;
    s.chop = 0.0f;
    s.hit = 0.0f;
    const auto tone = sine (220.0, 3.0);
    const auto run = runJump (tone, tone, s, -1.0);
    CHECK_THAT (12.0 * std::log2 (frequencyOf (run.out[0], beat (3.2), beat (3.9)) / 220.0), WithinAbs (12.0, 0.15));
    CHECK_THAT (12.0 * std::log2 (frequencyOf (run.out[0], beat (4.2), beat (4.9)) / 220.0), WithinAbs (0.0, 0.15));
    CHECK (run.drops == 1);
}

TEST_CASE ("CPU: HYPERJUMP, building and dropping", "[hyperjump][cpu]")
{
    Settings s;
    s.on = true;
    s.amount = 1.0f;
    s.length = 0;
    s.rise = 1.0f;
    s.chop = 1.0f;
    s.hit = 1.0f;
    const auto tone = sine (220.0, 8.0);
    const auto begin = std::chrono::steady_clock::now();
    runJump (tone, tone, s);
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - begin).count();
    const auto percent = 100.0 * seconds / 8.0;
    WARN ("HYPERJUMP, stereo: " << percent << "% of a core (the test's own input included)");
    CHECK (percent < 10.0);
}

TEST_CASE ("HYPERJUMP in the plugin: MACRO 3 moves it through the matrix; its unit and page", "[hyperjump][plugin]")
{
    for (const auto* id : { ParamIDs::hyperjumpAmount, ParamIDs::hyperjumpRise, ParamIDs::hyperjumpChop })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Hyperjump");
    }
    // LIGHTSPEED is gone: its IDs aren't parameters any more.
    ParagonX1AudioProcessor processor;
    for (const auto* id : { "jumpOn", "jumpAmount", "jumpRise", "jumpStreak", "jumpWhoosh", "jumpWidth", "jumpTone" })
        CHECK (processor.getParameters().getParameter (id) == nullptr);
    CHECK_FALSE (signatureNames().contains ("LIGHTSPEED"));
    CHECK (signatureNames().contains ("HYPERJUMP"));

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
    set (ParamIDs::hyperjumpOn, 1.0f);
    // MACRO 3 to AMOUNT, as HYPERDRIVE's presets route it.
    auto matrix = processor.getModMatrix();
    matrix.routes.push_back ({ true, "macro3", ParamIDs::hyperjumpAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (matrix);
    set (ParamIDs::macros[2], 1.0f);
    play (1.0);
    CHECK (processor.getHyperJump().getAmount() > 0.9f);
    CHECK (processor.getHyperJump().getProgress() >= 0.0f);   // the song stopped: building from the start
    set (ParamIDs::macros[2], 0.0f);
    play (6.0);
    CHECK (processor.getHyperJump().isResting());

    HyperJumpPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 7);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
