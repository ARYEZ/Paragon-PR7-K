// POLYMETER (1.4.0): the held notes struck again in several meters at once (4, 3, 5, 7 a cycle), in time with the
// song. AMOUNT brings the meters in one a quarter; the held notes from the lowest take them in turn and a lone note
// plays them all; off or AMOUNT 0 leaves every note untouched bit for bit; each note is struck exactly where its meter
// falls (4 on a bar's quarters, 3 on its thirds...), at two tempos and in two CYCLEs; a lone note plays the composite
// of 4 against 3 and nothing between; DENSITY doubles every meter; two meters that nearly meet strike once; the song's
// position is followed; a released note isn't struck; in the plugin its parameters, matrix destinations, MACRO 3, the
// SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/synth/Polymeter.h"
#include "ui/PolymeterPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
    using namespace synth;

    // Pure sines struck and dying to 10%, so a strike shows as the level jumping back up.
    Player polyPlayer (polymeter::Settings s, double bpm = 60.0, float sustain = 0.1f, float decayMs = 40.0f)
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.filter.on = false;
        p.settings.amp.envelope.attackMs = 0.3f;
        p.settings.amp.envelope.decayMs = decayMs;
        p.settings.amp.envelope.sustain = sustain;
        p.settings.amp.envelope.releaseMs = 20.0f;
        p.settings.modulation.bpm = bpm;
        p.settings.polymeter = s;
        return p;
    }

    polymeter::Settings meters (float amount, int cycle = 2, int density = 0)
    {
        polymeter::Settings s;
        s.on = true;
        s.amount = amount;
        s.cycle = cycle;
        s.density = density;
        s.accent = 0.0f;
        s.lift = 0.5f;
        return s;
    }

    double riseAt (const std::vector<float>& x, double t, double w = 0.004)
    {
        const auto before = rms (x, (size_t) ((t - w - 0.0015) * sampleRate), (size_t) ((t - 0.0015) * sampleRate));
        const auto after = rms (x, (size_t) ((t + 0.0008) * sampleRate), (size_t) ((t + 0.0008 + w) * sampleRate));
        return db (after / juce::jmax (1.0e-9, before));
    }
}

TEST_CASE ("POLYMETER: AMOUNT brings the meters in one a quarter; notes take them from the lowest", "[polymeter]")
{
    CHECK (polymeter::metersAt (0.0f) == 0);
    CHECK (polymeter::metersAt (0.0009f) == 0);
    CHECK (polymeter::metersAt (0.01f) == 1);
    CHECK (polymeter::metersAt (0.249f) == 1);
    CHECK (polymeter::metersAt (0.25f) == 2);
    CHECK (polymeter::metersAt (0.5f) == 3);
    CHECK (polymeter::metersAt (0.75f) == 4);
    CHECK (polymeter::metersAt (1.0f) == 4);
    CHECK (polymeter::meters == std::array<int, 4> { 4, 3, 5, 7 });
    // Two notes, two meters: one each, the lowest on 4.
    CHECK (polymeter::metersFor (0, 2, 2) == 0b0001);
    CHECK (polymeter::metersFor (1, 2, 2) == 0b0010);
    // A lone note plays every meter in.
    CHECK (polymeter::metersFor (0, 1, 4) == 0b1111);
    // Two notes, four meters: the lowest on 4, the other the rest.
    CHECK (polymeter::metersFor (0, 2, 4) == 0b0001);
    CHECK (polymeter::metersFor (1, 2, 4) == 0b1110);
    // Five notes, three meters: round again.
    CHECK (polymeter::metersFor (3, 5, 3) == 0b0001);
    CHECK (polymeter::metersFor (4, 5, 3) == 0b0010);
    CHECK (polymeter::metersFor (0, 0, 3) == 0);
}

TEST_CASE ("POLYMETER: off, or AMOUNT 0, leaves every note untouched bit for bit", "[polymeter]")
{
    polymeter::Settings off;
    auto plain = polyPlayer (off, 60.0, 0.6f);
    plain.on (48);
    plain.on (55);
    const auto reference = plain.render (3.0);
    for (const auto amount : { 0.0f, 0.0005f })
    {
        auto p = polyPlayer (meters (amount), 60.0, 0.6f);
        p.on (48);
        p.on (55);
        CHECK (p.render (3.0) == reference);
        CHECK (p.engine.getPolyStrikes() == 0);
    }
    auto unused = meters (1.0f);
    unused.on = false;
    auto p = polyPlayer (unused, 60.0, 0.6f);
    p.on (48);
    p.on (55);
    CHECK (p.render (3.0) == reference);
}

TEST_CASE ("POLYMETER: two notes, 4 against 3: each struck where its own meter falls, at two tempos and two cycles", "[polymeter]")
{
    // C3 on 4, G4 on 3 (AMOUNT 0.3: two meters in).
    for (const auto bpm : { 60.0, 120.0 })
        for (const auto cycle : { 2, 1 })   // a bar, two beats
        {
            const auto cycleSeconds = polymeter::cycleBeats[(size_t) cycle] * 60.0 / bpm;
            auto p = polyPlayer (meters (0.3f, cycle), bpm, 0.1f, juce::jmin (40.0f, (float) (cycleSeconds / 3.0 * 400.0)));
            p.settings.amp.spread = 1.0f;   // the two notes on opposite sides, so each side is one note
            p.on (48);
            p.on (67);
            const auto seconds = cycleSeconds * 2.0 + 0.05;
            const auto out = p.render (seconds);
            const auto leftIsLow = frequencyOf (out[0], 0, (size_t) (0.05 * sampleRate)) < 200.0;
            const auto& low = out[leftIsLow ? 0 : 1];
            const auto& high = out[leftIsLow ? 1 : 0];
            REQUIRE (rms (high, 0, (size_t) (0.05 * sampleRate)) > 0.01);
            REQUIRE (rms (low, 0, (size_t) (0.05 * sampleRate)) > 0.01);
            INFO ("BPM " << bpm << ", cycle " << polymeter::cycleNames[cycle]);
            const auto w = juce::jmin (0.004, cycleSeconds / 7.0 * 0.25);
            for (int k = 1; k < 8; ++k)
            {
                const auto t = cycleSeconds * k / 4.0;
                INFO ("4: strike " << k);
                CHECK (riseAt (low, t, w) > 4.0);
                if (k % 4 != 0)
                {
                    // ...and the 3's note isn't struck there (unless the cycle's start, shared).
                    CHECK (riseAt (high, t, w) < 1.5);
                }
            }
            for (int k = 1; k < 6; ++k)
            {
                const auto t = cycleSeconds * k / 3.0;
                INFO ("3: strike " << k);
                CHECK (riseAt (high, t, w) > 4.0);
                if (k % 3 != 0)
                    CHECK (riseAt (low, t, w) < 1.5);
            }
            CHECK (p.engine.getPolyMeterNote (0) == 48);
            CHECK (p.engine.getPolyMeterNote (1) == 67);
        }
}

TEST_CASE ("POLYMETER: a lone note plays every meter in, and nothing between", "[polymeter]")
{
    // A bar of 4 against 3 at 60 BPM (4 s): strikes at 0, 1, 1.333, 2, 2.667, 3 s; halfway between them nothing.
    auto p = polyPlayer (meters (0.3f), 60.0);
    p.on (57);
    const auto out = p.render (4.2);
    for (const auto t : { 1.0, 4.0 / 3.0, 2.0, 8.0 / 3.0, 3.0, 4.0 })
    {
        INFO ("at " << t << " s");
        CHECK (riseAt (out[0], t) > 4.0);
    }
    for (const auto t : { 0.5, 1.17, 1.67, 2.33, 2.83, 3.5 })
    {
        INFO ("between, at " << t << " s");
        CHECK (riseAt (out[0], t) < 1.5);
    }
    // 4 + 3 strikes a bar less the shared start (the note's own attack at 0, then the cycle at 4 s).
    CHECK (std::abs (p.engine.getPolyStrikes() - 6) <= 1);
    // All four meters on a lone note: 4 + 3 + 5 + 7 a cycle less the three that meet at its start.
    auto all = polyPlayer (meters (1.0f), 120.0, 0.1f, 20.0f);
    all.on (57);
    all.render (2.0 - 0.01);   // one cycle, its own start not counted (the attack)
    CHECK (std::abs (all.engine.getPolyStrikes() - (3 + 2 + 4 + 6)) <= 1);
}

TEST_CASE ("POLYMETER: DENSITY strikes every meter twice as often; ACCENT softens the subdivisions", "[polymeter]")
{
    auto s = meters (0.1f, 2, 1);   // the pulse alone, x2: eighths of a bar at 60 BPM, every 0.5 s
    s.accent = 1.0f;
    auto p = polyPlayer (s, 60.0, 0.05f, 30.0f);
    p.on (57);
    const auto out = p.render (2.1);
    for (int k = 1; k < 4; ++k)
    {
        INFO ("eighth " << k);
        CHECK (riseAt (out[0], 0.5 * k) > 4.0);
    }
    // The subdivisions (0.5, 1.5 s) strike softer than the meter's own beats (1, 2 s).
    const auto peakAfter = [&] (double t)
    {
        float peak = 0.0f;
        for (auto i = (size_t) (t * sampleRate); i < (size_t) ((t + 0.03) * sampleRate); ++i)
            peak = std::max (peak, std::abs (out[0][i]));
        return peak;
    };
    CHECK (db (peakAfter (0.5) / peakAfter (1.0)) < -6.0);
    CHECK (db (peakAfter (1.5) / peakAfter (2.0)) < -6.0);
}

TEST_CASE ("POLYMETER follows the song's position, and leaves a released note alone", "[polymeter]")
{
    // At 120 BPM the song starts at beat 0.3, a cycle of a beat (the pulse alone: quarters of a beat, every 0.125 s):
    // the first strike at beat 0.5, 0.1 s in.
    auto p = polyPlayer (meters (0.1f, 0), 120.0, 0.1f, 20.0f);
    p.settings.modulation.ppq = 0.3;
    p.on (57);
    std::array<std::vector<float>, 2> out;
    for (int b = 0; b < (int) (0.6 * sampleRate / blockSize); ++b)
    {
        const auto block = p.render ((double) blockSize / sampleRate);
        for (int c = 0; c < 2; ++c)
            out[(size_t) c].insert (out[(size_t) c].end(), block[(size_t) c].begin(), block[(size_t) c].end());
        p.settings.modulation.ppq += (double) blockSize / sampleRate * 2.0;
    }
    CHECK (riseAt (out[0], 0.1, 0.003) > 4.0);
    CHECK (riseAt (out[0], 0.225, 0.003) > 4.0);
    CHECK (riseAt (out[0], 0.16, 0.003) < 1.5);
    // Released: never struck again.
    auto r = polyPlayer (meters (1.0f), 120.0, 0.6f);
    r.on (57);
    r.render (0.3);
    r.off (57);
    r.render (0.05);
    const auto strikes = r.engine.getPolyStrikes();
    r.render (1.5);
    CHECK (r.engine.getPolyStrikes() == strikes);
}

TEST_CASE ("POLYMETER in the plugin: parameters, matrix destinations, MACRO 3, the SIGNATURE slot and its page", "[polymeter][plugin]")
{
    for (const auto* id : { ParamIDs::polyAmount, ParamIDs::polyAccent, ParamIDs::polyLift })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Polymeter");
    }
    CHECK (signatureNames()[signatureNames().size() - 1] == "POLYMETER");
    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    processor.seedRandomForTest (1);
    const auto set = [&processor] (const char* id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto play = [&] (double seconds)
    {
        juce::MidiBuffer midi;
        for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    };
    set (ParamIDs::polyOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::polyAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    processor.getKeyboardState().noteOn (1, 57, 0.9f);
    play (2.5);
    CHECK (processor.getPolyStrikes() == 0);   // MACRO 3 at its left: nothing struck
    set (ParamIDs::macros[2], 1.0f);
    play (0.3);
    CHECK_THAT (processor.getPolymeterSettings().amount, Catch::Matchers::WithinAbs (1.0, 0.01));
    CHECK (processor.getPolymeterSettings().activeMeters() == 4);
    play (2.5);
    CHECK (processor.getPolyStrikes() > 6);
    CHECK (processor.getPolyMeterNote (3) == 57);
    CHECK (signatureOn (processor, signatureNames().indexOf ("POLYMETER")));

    PolymeterPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 6);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
