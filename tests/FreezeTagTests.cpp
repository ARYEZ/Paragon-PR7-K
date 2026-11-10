// FREEZE TAG (0.89.0): in time with the song the sound plays, then freezes. Off and AMOUNT 0 untouched bit for bit; while
// it plays the sound is untouched; the freeze lands on its beat at any tempo; frozen, the live sound is cut and a ghost
// holds the moment it stopped, near its level; CAUGHT sinks it; silence and a silent side stay silent; turned down it
// rests untouched; in the plugin its matrix destinations, MACRO 3, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/FreezeTag.h"
#include "ui/FreezeTagPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using Settings = effects::FreezeTag::Settings;

    std::vector<float> sine (double hz, double seconds, float level = 0.3f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate);
        return x;
    }

    struct Run
    {
        std::array<std::vector<float>, 2> out;
        bool resting = false;
    };

    Run runFreeze (std::vector<float> left, std::vector<float> right, Settings s, double bpm = 120.0, int changeAt = -1,
                   Settings changed = {})
    {
        effects::FreezeTag fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (1);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            const auto& now = changeAt >= 0 && (int) start >= changeAt ? changed : s;
            fx.process (juce::dsp::AudioBlock<float> (channels, 2, n), now, bpm, (double) start * bpm / 60.0 / sampleRate);
        }
        return { { left, right }, fx.isResting() };
    }

    Settings full()
    {
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.cycle = 1;   // 2 BARS: frozen over beats 6-8 of every 8
        s.hold = 1;    // 2 BEATS
        s.caught = 0.0f;
        s.tremble = 0.0f;
        s.frost = 0.0f;
        return s;
    }
}

TEST_CASE ("FREEZE TAG: off and AMOUNT 0 leave the sound untouched bit for bit", "[freezetag]")
{
    const auto left = sine (220.0, 6.0), right = sine (330.0, 6.0);
    auto off = full();
    off.on = false;
    auto run = runFreeze (left, right, off);
    CHECK (run.out[0] == left);
    CHECK (run.out[1] == right);
    CHECK (run.resting);
    auto zero = full();
    zero.amount = 0.0005f;
    run = runFreeze (left, right, zero);
    CHECK (run.out[0] == left);
    CHECK (run.resting);
}

TEST_CASE ("FREEZE TAG: untouched while it plays, frozen on its beat at any tempo", "[freezetag]")
{
    for (const auto bpm : { 120.0, 90.0 })
    {
        INFO (bpm);
        const auto beat = 60.0 / bpm * sampleRate;
        const auto in = sine (220.0, 20.0 * 60.0 / bpm);
        const auto run = runFreeze (in, in, full(), bpm);
        // The second round (beats 8-16): untouched from just after the return to just before the freeze.
        for (auto i = (size_t) (8.0 * beat + 0.01 * sampleRate); i < (size_t) (14.0 * beat); ++i)
            REQUIRE (run.out[0][i] == in[i]);
        // The freeze starts on beat 14 (within a sample) and the return comes on beat 16.
        size_t first = 0;
        for (auto i = (size_t) (13.0 * beat); i < (size_t) (15.0 * beat); ++i)
            if (run.out[0][i] != in[i]) { first = i; break; }
        CHECK (std::abs ((double) first - 14.0 * beat) < 2.0);
    }
    using F = effects::FreezeTag;
    CHECK_FALSE (F::frozenAt (5.9, 1, 1));
    CHECK (F::frozenAt (6.0, 1, 1));
    CHECK (F::frozenAt (7.99, 1, 1));
    CHECK_FALSE (F::frozenAt (8.0, 1, 1));
    CHECK (F::holdBeats (0, 2) == 3.0);   // 4 beats in a 1-bar cycle: never more than the cycle less a beat
}

TEST_CASE ("FREEZE TAG: frozen, the new note is cut and a ghost holds the old one near its level", "[freezetag]")
{
    // A3 until the freeze (beat 6 at 120 BPM: 3 s), then E4: the frozen stretch should hold A3, not E4.
    const auto switchAt = (size_t) (3.0 * sampleRate);
    auto in = sine (220.0, 4.2);
    const auto e4 = sine (329.63, 4.2);
    for (auto i = switchAt; i < in.size(); ++i)
        in[i] = e4[i];
    const auto run = runFreeze (in, in, full());
    const auto from = (size_t) (3.1 * sampleRate), length = (size_t) (0.8 * sampleRate);
    const auto held = db (levelAt (run.out[0], 220.0, from, length));
    const auto cut = db (levelAt (run.out[0], 329.63, from, length));
    INFO ("A3 held " << held << " dB, E4 " << cut << " dB");
    CHECK (held > cut + 25.0);
    // The ghost near the level of what it froze (within 4 dB).
    const auto before = db (rms (in, (size_t) (2.0 * sampleRate), switchAt));
    const auto ghost = db (rms (run.out[0], from, from + length));
    INFO ("before " << before << " dB, ghost " << ghost);
    CHECK (std::abs (ghost - before - db (0.8)) < 4.0);
}

TEST_CASE ("FREEZE TAG: CAUGHT sinks the ghost", "[freezetag]")
{
    const auto in = sine (220.0, 4.2);
    auto s = full();
    s.caught = 1.0f;
    const auto run = runFreeze (in, in, s);
    // Early in the freeze the ghost is still near A3; by its end it has sunk most of an octave.
    const auto early = db (levelAt (run.out[0], 220.0, (size_t) (3.02 * sampleRate), (size_t) (0.15 * sampleRate)));
    const auto late = db (levelAt (run.out[0], 220.0, (size_t) (3.75 * sampleRate), (size_t) (0.2 * sampleRate)));
    INFO ("early " << early << " dB, late " << late);
    CHECK (late < early - 10.0);
    // Not caught: the ghost holds its pitch.
    const auto still = runFreeze (in, in, full());
    CHECK (db (levelAt (still.out[0], 220.0, (size_t) (3.75 * sampleRate), (size_t) (0.2 * sampleRate)))
           > db (levelAt (still.out[0], 220.0, (size_t) (3.02 * sampleRate), (size_t) (0.15 * sampleRate))) - 4.0);
}

TEST_CASE ("FREEZE TAG: silence and a silent side stay silent, and turned down it rests untouched", "[freezetag]")
{
    const auto in = sine (220.0, 8.0);
    const std::vector<float> quiet (in.size(), 0.0f);
    auto s = full();
    s.tremble = 0.8f;
    s.caught = 0.5f;
    CHECK (runFreeze (quiet, quiet, s).out[0] == quiet);
    CHECK (runFreeze (in, quiet, s).out[1] == quiet);
    const auto run = runFreeze (in, in, s);
    for (auto v : run.out[0])
        REQUIRE (std::isfinite (v));

    auto down = s;
    down.amount = 0.0f;
    const auto back = runFreeze (in, in, s, 120.0, (int) (2.0 * sampleRate), down);
    CHECK (back.resting);
    for (auto i = (size_t) (3.0 * sampleRate); i < in.size(); ++i)
        REQUIRE (back.out[0][i] == in[i]);
}

TEST_CASE ("CPU: FREEZE TAG", "[freezetag][cpu]")
{
    const auto in = sine (220.0, 8.0);
    auto s = full();
    s.tremble = 0.6f;
    s.frost = 0.5f;
    s.caught = 0.5f;
    s.cycle = 0;
    s.hold = 2;   // frozen 3 beats of every 4: the heavy case
    const auto begin = std::chrono::steady_clock::now();
    runFreeze (in, in, s);
    const auto percent = 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - begin).count() / 8.0;
    WARN ("FREEZE TAG, stereo: " << percent << "% of a core (the test's own input included)");
    CHECK (percent < 10.0);
}

TEST_CASE ("FREEZE TAG in the plugin: matrix destinations, MACRO 3 to rest, the SIGNATURE slot and its page", "[freezetag][plugin]")
{
    for (const auto* id : { ParamIDs::freezeAmount, ParamIDs::freezeTremble, ParamIDs::freezeCaught })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Freeze Tag");
    }
    CHECK (signatureNames().contains ("FREEZE TAG"));
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
    set (ParamIDs::freezeOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::freezeAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    set (ParamIDs::macros[2], 1.0f);
    play (5.0);   // past a freeze (its own count: beats 6-8 at 120 BPM)
    CHECK (processor.getFreezeTag().getAmount() > 0.9f);
    CHECK (processor.getFreezeTag().getFreezes() >= 1);
    set (ParamIDs::macros[2], 0.0f);
    play (2.0);
    CHECK (processor.getFreezeTag().isResting());

    FreezeTagPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 9);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
