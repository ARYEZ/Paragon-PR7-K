// ORNAMENT (1.6.0): trills, mordents and turns in key on the notes played, in time with the song. Each step of an
// ornament lands on a note of KEY and SCALE, one step up or down (an outside note counts from the scale note under
// it); off or AMOUNT 0 leaves every note untouched bit for bit; a trill alternates the note and the next note up on
// SPEED's steps for LENGTH, starting on the first step line 40 ms or more after the strike (at two tempos); a mordent
// and a turn play their few steps and settle on the note; AMOUNT is the share of notes ornamented, the same notes
// every time; turning it to 0 brings every note home; in the plugin its parameters, matrix destination, MACRO 3, the
// SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/synth/Ornament.h"
#include "ui/OrnamentPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
    using namespace synth;
    using Catch::Matchers::WithinAbs;

    constexpr int dKey = 2, harmonicMinor = 3;

    Player sinePlayer (ornament::Settings s, double bpm = 120.0)
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.filter.on = false;
        p.settings.amp.envelope.attackMs = 0.0f;
        p.settings.amp.envelope.sustain = 1.0f;
        p.settings.amp.envelope.releaseMs = 10.0f;
        p.settings.modulation.bpm = bpm;
        p.settings.ornament = s;
        return p;
    }

    ornament::Settings ornamenting (int style, float amount = 1.0f)
    {
        ornament::Settings s;
        s.on = true;
        s.amount = amount;
        s.style = style;
        s.speed = 0;    // 1/16
        s.length = 1;   // 1 BEAT
        s.key = dKey;
        s.scale = harmonicMinor;
        return s;
    }

    double semitonesBetween (const std::vector<float>& x, double fromSeconds, double toSeconds, int note)
    {
        return 12.0 * std::log2 (frequencyOf (x, (size_t) (fromSeconds * sampleRate), (size_t) (toSeconds * sampleRate)) / noteHz (note));
    }
}

TEST_CASE ("ORNAMENT: every step lands on the scale, one step up or down", "[ornament]")
{
    using ornament::semitonesFor;
    // D harmonic minor: D E F G A Bb C#.
    CHECK (semitonesFor (62, 1, dKey, harmonicMinor) == 2);    // D4 up to E4
    CHECK (semitonesFor (69, 1, dKey, harmonicMinor) == 1);    // A4 up to Bb4
    CHECK (semitonesFor (69, -1, dKey, harmonicMinor) == -2);  // A4 down to G4
    CHECK (semitonesFor (61, 1, dKey, harmonicMinor) == 1);    // C#4 up to D4
    CHECK (semitonesFor (62, -1, dKey, harmonicMinor) == -1);  // D4 down to C#4 (the leading note)
    CHECK (semitonesFor (60, 1, dKey, harmonicMinor) == 1);    // C4 (outside): from Bb3, up to C#4
    CHECK (semitonesFor (64, 0, dKey, harmonicMinor) == 0);
    for (int scale = 1; scale < arp::scaleNames.size(); ++scale)
        for (int key = 0; key < 12; ++key)
            for (int note = 36; note < 96; ++note)
                for (const auto steps : { -1, 1 })
                {
                    const auto to = note + semitonesFor (note, steps, key, scale);
                    CHECK (arp::inScale (scale, to - key));
                    CHECK ((steps > 0 ? to > note : to < note));
                }
    // The shapes, step by step.
    CHECK (ornament::stepAt (ornament::trill, 0, 4) == 0);
    CHECK (ornament::stepAt (ornament::trill, 1, 4) == 1);
    CHECK (ornament::stepAt (ornament::trill, 3, 4) == 1);
    CHECK (ornament::stepAt (ornament::trill, 4, 4) == 0);
    CHECK (ornament::stepAt (ornament::mordent, 0, 4) == 1);
    CHECK (ornament::stepAt (ornament::mordent, 1, 4) == 0);
    CHECK (ornament::stepAt (ornament::turn, 0, 4) == 1);
    CHECK (ornament::stepAt (ornament::turn, 2, 4) == -1);
    CHECK (ornament::stepAt (ornament::turn, 3, 4) == 0);
}

TEST_CASE ("ORNAMENT: off, or AMOUNT 0, leaves every note untouched bit for bit", "[ornament]")
{
    ornament::Settings off;
    auto plain = sinePlayer (off);
    plain.on (62);
    plain.on (69);
    const auto reference = plain.render (3.0);
    for (const auto amount : { 0.0f, 0.0005f })
    {
        auto p = sinePlayer (ornamenting (ornament::trill, amount));
        p.on (62);
        p.on (69);
        CHECK (p.render (3.0) == reference);
        CHECK (p.engine.getOrnamentCount() == 0);
    }
    auto unused = ornamenting (ornament::trill, 1.0f);
    unused.on = false;
    auto p = sinePlayer (unused);
    p.on (62);
    p.on (69);
    CHECK (p.render (3.0) == reference);
}

TEST_CASE ("ORNAMENT: a trill alternates the note and the step above on SPEED's steps for LENGTH, at two tempos", "[ornament]")
{
    for (const auto bpm : { 120.0, 90.0 })
    {
        INFO ("BPM " << bpm);
        auto p = sinePlayer (ornamenting (ornament::trill), bpm);
        p.on (69);   // A4: the trill to Bb4 (+1)
        const auto x = p.render (3.0)[0];
        const auto step = 0.25 * 60.0 / bpm;   // a 16th
        // The first line 40 ms or more after the strike is the first 16th (its own count from the key). Steps 0-3 of
        // a one-beat trill: the note, Bb, the note, Bb; then the note.
        const auto at = [&] (double k) { return semitonesBetween (x, step * (1.0 + k) + 0.25 * step, step * (1.0 + k) + 0.85 * step, 69); };
        CHECK_THAT (semitonesBetween (x, 0.01, 0.9 * step, 69), WithinAbs (0.0, 0.05));   // struck plain
        CHECK_THAT (at (0), WithinAbs (0.0, 0.05));
        CHECK_THAT (at (1), WithinAbs (1.0, 0.05));
        CHECK_THAT (at (2), WithinAbs (0.0, 0.05));
        CHECK_THAT (at (3), WithinAbs (1.0, 0.05));
        CHECK_THAT (at (5), WithinAbs (0.0, 0.05));
        CHECK_THAT (at (8), WithinAbs (0.0, 0.05));
        CHECK (p.engine.getOrnamentCount() == 1);
        CHECK (p.engine.getOrnamentLast() == ornament::trill * 1000 + 69);
    }
    // HELD: it goes on as long as the note.
    auto s = ornamenting (ornament::trill);
    s.length = 3;
    auto p = sinePlayer (s);
    p.on (69);
    const auto x = p.render (3.0)[0];
    const auto step = 0.125;
    CHECK_THAT (semitonesBetween (x, step * 17.25, step * 17.85, 69), WithinAbs (0.0, 0.05));   // step 16 (2.1 s): the note
    CHECK_THAT (semitonesBetween (x, step * 18.25, step * 18.85, 69), WithinAbs (1.0, 0.05));   // step 17: the step above
}

TEST_CASE ("ORNAMENT: a mordent flicks up and back, a turn goes above, home, below, home; then the note", "[ornament]")
{
    const auto step = 0.125;   // 1/16 at 120 BPM
    {
        auto p = sinePlayer (ornamenting (ornament::mordent));
        p.on (62);   // D4: up to E4 (+2)
        const auto x = p.render (2.0)[0];
        CHECK_THAT (semitonesBetween (x, step * 1.25, step * 1.85, 62), WithinAbs (2.0, 0.05));
        CHECK_THAT (semitonesBetween (x, step * 2.25, step * 2.85, 62), WithinAbs (0.0, 0.05));
        CHECK_THAT (semitonesBetween (x, step * 6.0, step * 10.0, 62), WithinAbs (0.0, 0.05));
    }
    {
        auto p = sinePlayer (ornamenting (ornament::turn));
        p.on (62);   // D4: E4, D4, C#4, D4
        const auto x = p.render (2.0)[0];
        CHECK_THAT (semitonesBetween (x, step * 1.25, step * 1.85, 62), WithinAbs (2.0, 0.05));
        CHECK_THAT (semitonesBetween (x, step * 2.25, step * 2.85, 62), WithinAbs (0.0, 0.05));
        CHECK_THAT (semitonesBetween (x, step * 3.25, step * 3.85, 62), WithinAbs (-1.0, 0.05));
        CHECK_THAT (semitonesBetween (x, step * 4.25, step * 4.85, 62), WithinAbs (0.0, 0.05));
        CHECK_THAT (semitonesBetween (x, step * 6.0, step * 10.0, 62), WithinAbs (0.0, 0.05));
    }
}

TEST_CASE ("ORNAMENT: AMOUNT is the share of notes ornamented, the same notes every time; AMOUNT to 0 brings them home", "[ornament]")
{
    const auto count = [] (float amount)
    {
        auto p = sinePlayer (ornamenting (ornament::mordent, amount));
        p.settings.play.voices = 1;
        int ornamented = 0;
        for (int k = 0; k < 120; ++k)
        {
            p.on (50 + k % 30);
            p.render (0.3);
            p.off (50 + k % 30);
            p.render (0.2);
        }
        ornamented = p.engine.getOrnamentCount();
        return ornamented;
    };
    const auto half = count (0.5f);
    CHECK (half > 40);
    CHECK (half < 80);
    CHECK (count (0.5f) == half);   // the same notes
    CHECK (count (1.0f) == 120);
    // A held trill, AMOUNT taken to 0: the note goes home and stays.
    auto s = ornamenting (ornament::trill);
    s.length = 3;
    auto p = sinePlayer (s);
    p.on (69);
    p.render (1.0);
    p.settings.ornament.amount = 0.0f;
    const auto x = p.render (1.0)[0];
    CHECK_THAT (semitonesBetween (x, 0.05, 0.95, 69), WithinAbs (0.0, 0.02));
}

TEST_CASE ("ORNAMENT in the plugin: parameters, matrix destination, MACRO 3, the SIGNATURE slot and its page", "[ornament][plugin]")
{
    REQUIRE (ModDestinations::find (ParamIDs::ornAmount) != nullptr);
    CHECK (ModDestinations::find (ParamIDs::ornAmount)->group == "FX Ornament");
    CHECK (signatureNames()[signatureNames().size() - 1] == "ORNAMENT");
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
    set (ParamIDs::ornOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::ornAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    processor.getKeyboardState().noteOn (1, 62, 0.9f);
    play (1.0);
    CHECK (processor.getOrnamentCount() == 0);   // MACRO 3 at its left: nothing ornamented
    processor.getKeyboardState().noteOff (1, 62, 0.0f);
    play (2.0);   // let it ring out
    set (ParamIDs::macros[2], 1.0f);
    play (0.3);
    CHECK_THAT (processor.getOrnamentSettings().amount, WithinAbs (1.0, 0.01));
    processor.getKeyboardState().noteOn (1, 64, 0.9f);
    play (1.0);
    CHECK (processor.getOrnamentCount() == 1);
    CHECK (signatureOn (processor, signatureNames().indexOf ("ORNAMENT")));

    OrnamentPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 7);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
