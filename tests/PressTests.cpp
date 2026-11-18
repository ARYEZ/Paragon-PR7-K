// PRESS (1.3.0): notes pressed up the scale after they're struck, as a koto player presses the string, in time with
// the song. The press always lands on a note of KEY and SCALE (an outside note from the scale note under it), one or
// two steps up; the new scales are the ones meant; off or AMOUNT 0 leaves every note untouched bit for bit; the press
// waits for the next WHEN line (at two tempos and two divisions); each STYLE moves as it should (PRESS up and held,
// PUSH up and back, RELEASE struck pressed and let down, WOBBLE a swing that never goes below the note); AMOUNT is the
// share of notes pressed, the same notes every time; turning it to 0 lets the presses down; SFZ notes follow; in the
// plugin its parameters, matrix destinations, MACRO 3, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/synth/Press.h"
#include "ui/PressPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
    using namespace synth;
    using Catch::Matchers::WithinAbs;

    constexpr int aKey = 9, minorScale = 2;

    Player sinePlayer (press::Settings s, double bpm = 120.0)
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.filter.on = false;
        p.settings.amp.envelope.attackMs = 0.0f;
        p.settings.amp.envelope.sustain = 1.0f;
        p.settings.amp.envelope.releaseMs = 10.0f;
        p.settings.modulation.bpm = bpm;
        p.settings.press = s;
        return p;
    }

    press::Settings pressing (int style, float amount = 1.0f)
    {
        press::Settings s;
        s.on = true;
        s.amount = amount;
        s.style = style;
        s.division = 1;   // 1/8
        s.steps = 0;      // 1 STEP
        s.key = aKey;
        s.scale = minorScale;
        s.speed = 0.0f;   // 40 ms
        return s;
    }

    double semitonesBetween (const std::vector<float>& x, double fromSeconds, double toSeconds, int note)
    {
        return 12.0 * std::log2 (frequencyOf (x, (size_t) (fromSeconds * sampleRate), (size_t) (toSeconds * sampleRate)) / noteHz (note));
    }
}

TEST_CASE ("PRESS: a press lands on the scale, one or two steps up; the koto's scales are right", "[press]")
{
    press::Settings s;
    s.key = aKey;
    s.scale = minorScale;
    s.steps = 0;
    CHECK (press::semitonesFor (57, s) == 2.0f);   // A3 to B3
    CHECK (press::semitonesFor (59, s) == 1.0f);   // B3 to C4
    CHECK (press::semitonesFor (64, s) == 1.0f);   // E4 to F4
    s.steps = 1;
    CHECK (press::semitonesFor (57, s) == 3.0f);   // A3 to C4
    CHECK (press::semitonesFor (67, s) == 4.0f);   // G4 to B4 (past A, over the octave line)
    // A note outside the scale (A#3) lands two steps above the scale note under it (A3 + 2 steps = C4), in key.
    CHECK (press::pressedNote (58, 2, aKey, minorScale) == 60);
    // Every pressed note, from any note, in any scale and key, is in the scale.
    for (int scale = 0; scale < arp::scaleNames.size(); ++scale)
        for (int key = 0; key < 12; ++key)
            for (int note = 36; note < 96; ++note)
                for (int steps = 1; steps <= 2; ++steps)
                {
                    const auto to = press::pressedNote (note, steps, key, scale);
                    CHECK (to > note);
                    CHECK (arp::inScale (scale, to - key));
                }
    // The koto's scales, by their notes over the key.
    const auto notesOf = [] (const char* name)
    {
        std::array<int, 12> out {};
        const auto count = sirocco::scaleNotes (arp::scaleNames.indexOf (name), out);
        return std::vector<int> (out.begin(), out.begin() + count);
    };
    CHECK (notesOf ("Hirajoshi") == std::vector<int> { 0, 2, 3, 7, 8 });
    CHECK (notesOf ("In") == std::vector<int> { 0, 1, 5, 7, 8 });
    CHECK (notesOf ("Yo") == std::vector<int> { 0, 2, 5, 7, 9 });
    CHECK (press::Settings().scale == arp::scaleNames.indexOf ("In"));
    CHECK (arp::scaleNames.size() == (int) arp::scaleMasks.size());
}

TEST_CASE ("PRESS: off, or AMOUNT 0, leaves every note untouched bit for bit", "[press]")
{
    press::Settings off;
    auto plain = sinePlayer (off);
    plain.on (57);
    plain.on (64);
    const auto reference = plain.render (3.0);
    for (const auto amount : { 0.0f, 0.0005f })
    {
        auto p = sinePlayer (pressing (press::hold, amount));
        p.on (57);
        p.on (64);
        CHECK (p.render (3.0) == reference);
        CHECK (p.engine.getPressCount() == 0);
    }
    auto unused = pressing (press::hold, 1.0f);
    unused.on = false;
    auto p = sinePlayer (unused);
    p.on (57);
    p.on (64);
    CHECK (p.render (3.0) == reference);
}

TEST_CASE ("PRESS: PRESS waits for the next WHEN line, then rises and holds, at two tempos and two divisions", "[press]")
{
    for (const auto bpm : { 120.0, 90.0 })
        for (const auto division : { 1, 2 })   // 1/8, 1/4
        {
            auto s = pressing (press::hold);
            s.division = division;
            auto p = sinePlayer (s, bpm);
            p.on (57);
            const auto out = p.render (2.0);
            const auto line = press::beatsPerLine (division) * 60.0 / bpm;   // the first line after the strike
            INFO ("BPM " << bpm << ", " << press::divisionNames[division] << ", line at " << line << " s");
            CHECK_THAT (semitonesBetween (out[0], 0.04, line - 0.02, 57), WithinAbs (0.0, 0.03));
            CHECK_THAT (semitonesBetween (out[0], line + 0.08, line + 0.4, 57), WithinAbs (2.0, 0.03));
            CHECK_THAT (semitonesBetween (out[0], 1.6, 1.95, 57), WithinAbs (2.0, 0.03));
            CHECK (p.engine.getPressCount() == 1);
            CHECK (p.engine.getPressLast() == press::hold * 1000 + 57);
        }
}

TEST_CASE ("PRESS follows the song's position; a note struck just before a line waits for the one after", "[press]")
{
    // At 120 BPM the song is at beat 0.48 when the note is struck: the 1/8 line at beat 0.5 is 10 ms away, under
    // minimumGap, so the press waits for the line at beat 1.0, 0.26 s on.
    auto p = sinePlayer (pressing (press::hold));
    p.on (57);
    std::array<std::vector<float>, 2> out;
    const auto total = (int) (1.0 * sampleRate);
    for (auto& c : out)
        c.resize ((size_t) total);
    for (int start = 0; start < total; start += blockSize)
    {
        const auto n = juce::jmin (blockSize, total - start);
        p.settings.modulation.ppq = 0.48 + (double) start / sampleRate * 2.0;
        p.engine.process (out[0].data() + start, out[1].data() + start, n, p.pending, 0, p.settings);
        p.pending.clear();
    }
    CHECK_THAT (semitonesBetween (out[0], 0.03, 0.24, 57), WithinAbs (0.0, 0.03));
    CHECK_THAT (semitonesBetween (out[0], 0.34, 0.95, 57), WithinAbs (2.0, 0.03));
}

TEST_CASE ("PRESS: PUSH goes up and comes back, RELEASE is struck pressed and let down", "[press]")
{
    {
        auto p = sinePlayer (pressing (press::push));
        p.on (57);
        const auto out = p.render (1.0);
        // Line at 0.25 s, 40 ms up, held to 0.35 s, 40 ms back.
        CHECK_THAT (semitonesBetween (out[0], 0.05, 0.22, 57), WithinAbs (0.0, 0.03));
        CHECK_THAT (semitonesBetween (out[0], 0.30, 0.345, 57), WithinAbs (2.0, 0.05));
        CHECK_THAT (semitonesBetween (out[0], 0.45, 0.95, 57), WithinAbs (0.0, 0.03));
    }
    {
        auto p = sinePlayer (pressing (press::release));
        p.on (57);
        const auto out = p.render (1.0);
        CHECK_THAT (semitonesBetween (out[0], 0.02, 0.22, 57), WithinAbs (2.0, 0.03));
        CHECK_THAT (semitonesBetween (out[0], 0.35, 0.95, 57), WithinAbs (0.0, 0.03));
    }
}

TEST_CASE ("PRESS: WOBBLE swings above the note and never below it", "[press]")
{
    auto p = sinePlayer (pressing (press::wobble));
    p.on (57);
    const auto out = p.render (2.0);
    double low = 1.0e9, high = -1.0e9;
    for (double t = 0.6; t < 1.9; t += 0.02)
    {
        const auto st = semitonesBetween (out[0], t, t + 0.02, 57);
        low = std::min (low, st);
        high = std::max (high, st);
    }
    CHECK (low > -0.08);
    CHECK (high > 0.6);    // half the press (a whole tone), near the swing's top
    CHECK (high < 1.15);
}

TEST_CASE ("PRESS: AMOUNT is the share of notes pressed, the same ones every time", "[press]")
{
    const auto run = [] (float amount)
    {
        auto p = sinePlayer (pressing (press::hold, amount));
        p.settings.amp.envelope.releaseMs = 1.0f;
        std::vector<float> all;
        for (int k = 0; k < 64; ++k)
        {
            const auto note = 48 + (k * 7) % 24;
            p.on (note);
            const auto out = p.render (0.25);
            p.off (note);
            const auto gap = p.render (0.01);
            all.insert (all.end(), out[0].begin(), out[0].end());
            all.insert (all.end(), gap[0].begin(), gap[0].end());
        }
        return std::pair { p.engine.getPressCount(), all };
    };
    const auto [none, quiet] = run (0.0f);
    CHECK (none == 0);
    const auto [half, first] = run (0.5f);
    CHECK (half > 64 * 0.3);
    CHECK (half < 64 * 0.7);
    const auto [again, second] = run (0.5f);
    CHECK (again == half);
    CHECK (second == first);
    const auto [every, full] = run (1.0f);
    CHECK (every == 64);
}

TEST_CASE ("PRESS: turned to 0 mid-note, the press lets go", "[press]")
{
    auto p = sinePlayer (pressing (press::hold));
    p.on (57);
    const auto pressed = p.render (1.0);
    CHECK_THAT (semitonesBetween (pressed[0], 0.6, 0.95, 57), WithinAbs (2.0, 0.03));
    p.settings.press.amount = 0.0f;
    const auto down = p.render (1.0);
    CHECK_THAT (semitonesBetween (down[0], 0.4, 0.95, 57), WithinAbs (0.0, 0.03));
}

TEST_CASE ("PRESS in the plugin: parameters, matrix destinations, MACRO 3, the SIGNATURE slot and its page", "[press][plugin]")
{
    for (const auto* id : { ParamIDs::pressAmount, ParamIDs::pressSpeed })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Press");
    }
    CHECK (signatureNames().contains ("PRESS"));
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
    set (ParamIDs::pressOn, 1.0f);
    CHECK (processor.getPressSettings().scale == synth::arp::scaleNames.indexOf ("In"));
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::pressAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    processor.getKeyboardState().noteOn (1, 57, 0.9f);
    play (1.0);
    CHECK (processor.getPressCount() == 0);   // MACRO 3 at its left: nothing pressed
    processor.getKeyboardState().noteOff (1, 57, 0.0f);
    play (0.2);
    set (ParamIDs::macros[2], 1.0f);
    play (0.2);
    CHECK_THAT (processor.getPressSettings().amount, WithinAbs (1.0, 0.01));
    processor.getKeyboardState().noteOn (1, 57, 0.9f);
    play (1.0);
    CHECK (processor.getPressCount() == 1);
    CHECK (processor.getPressNow() > 0);
    CHECK (signatureOn (processor, signatureNames().indexOf ("PRESS")));

    PressPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 8);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
