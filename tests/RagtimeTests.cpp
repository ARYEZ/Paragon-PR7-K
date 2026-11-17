// RAGTIME (0.92.0): every held note re-struck on a player-piano roll, in time. Off or AMOUNT 0: bit for bit untouched;
// the strikes land on the roll's steps (and SWING moves the odd ones late); STRIDE strikes the bass and the chord on
// their own steps; AMOUNT plays the tiers in order; ACCENT softens the off-beats; a sustained sound is struck too (the
// LIFT); a note the pedal holds is struck and still lets go when the pedal comes up; in the plugin its matrix
// destinations, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/synth/Ragtime.h"
#include "ui/RackSpecs.h"
#include "ui/RagtimePage.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    using namespace synth;

    // Sixteenth steps at 60 BPM: a step every 0.25 s; the song stopped, so the roll's own count starts with the key.
    constexpr double step = 0.25;

    // A pure sine, struck and dying away to 10% (a voice at sustain 0 ends once silent: nothing left to strike), so a
    // strike shows as the level jumping back up.
    Player pluckPlayer (ragtime::Settings r, float sustain = 0.1f)
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.filter.on = false;
        p.settings.amp.envelope.attackMs = 0.5f;
        p.settings.amp.envelope.decayMs = 150.0f;
        p.settings.amp.envelope.sustain = sustain;
        p.settings.amp.envelope.releaseMs = 30.0f;
        p.settings.modulation.bpm = 60.0;
        p.settings.ragtime = r;
        return p;
    }

    ragtime::Settings roll (int pattern, float amount = 1.0f)
    {
        ragtime::Settings r;
        r.on = true;
        r.amount = amount;
        r.pattern = pattern;
        r.division = 0;   // 1/16
        r.swing = 0.0f;
        r.accent = 0.0f;
        return r;
    }

    // The level just after time `t` against just before: a strike shows as a big rise.
    double riseAt (const std::vector<float>& x, double t)
    {
        const auto before = rms (x, (size_t) ((t - 0.03) * sampleRate), (size_t) ((t - 0.008) * sampleRate));
        const auto after = rms (x, (size_t) ((t + 0.004) * sampleRate), (size_t) ((t + 0.026) * sampleRate));
        return juce::Decibels::gainToDecibels (after / juce::jmax (1.0e-9, before));
    }

    // One note's level (its pitch alone) just after time `t`.
    double noteLevelAt (const std::vector<float>& x, double hz, double t)
    {
        return juce::Decibels::gainToDecibels (levelAt (x, hz, (size_t) ((t + 0.006) * sampleRate), (size_t) (0.03 * sampleRate)));
    }
}

TEST_CASE ("RAGTIME: off, or AMOUNT 0, leaves every note untouched bit for bit", "[ragtime]")
{
    ragtime::Settings off;
    auto plain = pluckPlayer (off, 0.6f);
    plain.on (48);
    plain.on (55);
    const auto reference = plain.render (3.0);

    for (const auto amount : { 0.0f, 0.0005f })
    {
        auto p = pluckPlayer (roll (4, amount), 0.6f);
        p.on (48);
        p.on (55);
        CHECK (p.render (3.0) == reference);
        CHECK (p.engine.getRagtimeStrikes() == 0);
    }
    auto unused = roll (4);   // set up but switched off
    unused.on = false;
    auto p = pluckPlayer (unused, 0.6f);
    p.on (48);
    p.on (55);
    CHECK (p.render (3.0) == reference);
}

TEST_CASE ("RAGTIME: the strikes land on the roll's steps, and SWING moves every second one late", "[ragtime]")
{
    // ROLL: every step strikes; a note dies away between strikes, so each strike lifts the level again.
    {
        auto p = pluckPlayer (roll (4));
        p.on (57);
        const auto out = p.render (2.2);
        for (int k = 1; k < 8; ++k)
        {
            INFO ("step " << k);
            CHECK (riseAt (out[0], k * step) > 6.0);
        }
        // Between the steps, no strike: the level only falls.
        CHECK (riseAt (out[0], 2.5 * step) < 0.5);
        CHECK (p.engine.getRagtimeStrikes() == 8);   // steps 1 to 8 (step 0 is the key's own strike)
    }
    // SWING all the way: the odd steps a third of a step late.
    {
        auto r = roll (4);
        r.swing = 1.0f;
        auto p = pluckPlayer (r);
        p.on (57);
        const auto out = p.render (2.2);
        for (const int k : { 1, 3, 5 })
        {
            INFO ("step " << k);
            CHECK (riseAt (out[0], k * step) < 0.5);                  // not on the step...
            CHECK (riseAt (out[0], (k + 1.0 / 3.0) * step) > 6.0);    // ...a third of it later
        }
        CHECK (riseAt (out[0], 2 * step) > 6.0);   // the even steps where they were
    }
}

TEST_CASE ("RAGTIME: STRIDE strikes the bass on its steps and the chord on its own", "[ragtime]")
{
    // STRIDE's beats: the bass on steps 0 and 8, the chord on 4 and 12. A3 the bass, E4 the chord.
    auto p = pluckPlayer (roll (0, 0.3f));
    p.on (45);
    p.on (64);
    const auto out = p.render (3.5);
    const auto bassHz = noteHz (45), chordHz = noteHz (64);
    // Step 4: the chord struck, the bass not (still at its floor from the start).
    CHECK (noteLevelAt (out[0], chordHz, 4 * step) - noteLevelAt (out[0], chordHz, 4 * step - 0.04) > 6.0);
    CHECK (noteLevelAt (out[0], bassHz, 4 * step) - noteLevelAt (out[0], bassHz, 4 * step - 0.04) < 1.0);
    // Step 8: the bass struck, the chord not.
    CHECK (noteLevelAt (out[0], bassHz, 8 * step) - noteLevelAt (out[0], bassHz, 8 * step - 0.04) > 6.0);
    CHECK (noteLevelAt (out[0], chordHz, 8 * step) - noteLevelAt (out[0], chordHz, 8 * step - 0.04) < 1.0);
    // STRIDE off: step 4 strikes both.
    auto r = roll (0, 0.3f);
    r.stride = false;
    auto both = pluckPlayer (r);
    both.on (45);
    both.on (64);
    const auto all = both.render (1.5);
    CHECK (noteLevelAt (all[0], bassHz, 4 * step) - noteLevelAt (all[0], bassHz, 4 * step - 0.04) > 6.0);
    // A lone note plays both hands' steps.
    auto lone = pluckPlayer (roll (0, 0.3f));
    lone.on (57);
    const auto one = lone.render (3.5);
    for (const int k : { 4, 8, 12 })
        CHECK (riseAt (one[0], k * step) > 6.0);
}

TEST_CASE ("RAGTIME: AMOUNT plays the roll's tiers in order", "[ragtime]")
{
    // RAG: tier 1 strikes 0 and 8 (both); tier 2 adds 3, 6, 11, 14; tier 3 adds 4 and 12. Count them over a bar.
    const auto strikes = [] (float amount)
    {
        auto p = pluckPlayer (roll (1, amount));
        p.on (57);
        p.render (16 * step - 0.01);
        return p.engine.getRagtimeStrikes();
    };
    CHECK (ragtime::tiersAt (0.0f) == 0);
    CHECK (ragtime::tiersAt (0.2f) == 1);
    CHECK (ragtime::tiersAt (0.5f) == 2);
    CHECK (ragtime::tiersAt (1.0f) == 3);
    CHECK (strikes (0.2f) == 1);   // step 8 (step 0 the key)
    CHECK (strikes (0.5f) == 5);   // + 3, 6, 11, 14
    CHECK (strikes (1.0f) == 7);   // + 4, 12
}

TEST_CASE ("RAGTIME: ACCENT plays the off-beats softer than the main beats", "[ragtime]")
{
    // ROLL: steps 4, 8, 12 accented, the rest not. Full ACCENT: the unaccented at half the velocity.
    const auto peakAfter = [] (const std::vector<float>& x, double t)
    {
        float peak = 0.0f;
        for (auto i = (size_t) ((t + 0.001) * sampleRate); i < (size_t) ((t + 0.02) * sampleRate); ++i)
            peak = std::max (peak, std::abs (x[i]));
        return juce::Decibels::gainToDecibels ((double) peak);
    };
    auto r = roll (4);
    r.accent = 1.0f;
    auto p = pluckPlayer (r);
    p.on (57, 1.0f);
    const auto out = p.render (2.2);
    const auto accented = peakAfter (out[0], 4 * step), soft = peakAfter (out[0], 5 * step);
    INFO ("accented " << accented << " dB, unaccented " << soft << " dB");
    CHECK (accented - soft > 4.0);
}

TEST_CASE ("RAGTIME: a sustained sound is struck too, by the damper's LIFT", "[ragtime]")
{
    // Full sustain: restarting the envelope alone would change nothing; the lift before each strike is what's heard.
    auto r = roll (4);
    r.lift = 1.0f;   // the damper all the way: down about 22 dB
    auto p = pluckPlayer (r, 1.0f);
    p.on (57);
    const auto out = p.render (2.2);
    for (int k = 1; k < 7; ++k)
    {
        // Just before the step, the sound dips well under its level mid-step.
        const auto mid = rms (out[0], (size_t) ((k - 0.5) * step * sampleRate), (size_t) (((k - 0.5) * step + 0.02) * sampleRate));
        const auto dip = rms (out[0], (size_t) ((k * step - 0.0015) * sampleRate), (size_t) ((k * step + 0.0005) * sampleRate));
        INFO ("step " << k);
        CHECK (juce::Decibels::gainToDecibels (dip / mid) < -6.0);
    }
    // LIFT 0: no dip.
    r.lift = 0.0f;
    auto flat = pluckPlayer (r, 1.0f);
    flat.on (57);
    const auto even = flat.render (2.2);
    const auto mid = rms (even[0], (size_t) (0.6 * sampleRate), (size_t) (0.62 * sampleRate));
    const auto at = rms (even[0], (size_t) (0.7485 * sampleRate), (size_t) (0.7505 * sampleRate));
    CHECK (std::abs (juce::Decibels::gainToDecibels (at / mid)) < 1.0);
}

TEST_CASE ("RAGTIME: a note the pedal holds is struck, and lets go when the pedal comes up", "[ragtime]")
{
    auto p = pluckPlayer (roll (4), 0.6f);
    p.midi (juce::MidiMessage::controllerEvent (1, 64, 127));
    p.render (0.01);
    p.on (57);
    p.render (0.3);
    p.off (57);   // the key up: the pedal holds it
    const auto held = p.render (1.0);
    CHECK (riseAt (held[0], 4 * step - 0.31) > 2.0);   // still struck on step 4 (1.0 s from the key; at 60% sustain a strike rises about 4 dB)
    p.midi (juce::MidiMessage::controllerEvent (1, 64, 0));
    p.render (0.5);
    const auto after = p.render (0.5);
    float peak = 0.0f;
    for (auto v : after[0])
        peak = std::max (peak, std::abs (v));
    CHECK (peak < 1.0e-4f);   // let go: no more strikes, silent
}

TEST_CASE ("RAGTIME in the plugin: matrix destinations, the SIGNATURE slot and its page", "[ragtime][plugin]")
{
    for (const auto* id : { ParamIDs::ragtimeAmount, ParamIDs::ragtimeSwing, ParamIDs::ragtimeAccent })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Ragtime");
    }
    CHECK (signatureNames().contains ("RAGTIME"));
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
    set (ParamIDs::ragtimeOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::ragtimeAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    processor.getKeyboardState().noteOn (1, 57, 0.9f);
    play (0.5);
    CHECK (processor.getRagtimeStrikes() == 0);   // MACRO 3 at its left: nothing struck
    set (ParamIDs::macros[2], 1.0f);
    play (3.0);
    CHECK (processor.getRagtimeStrikes() > 3);
    CHECK (processor.getRagtimeStep() >= 0);

    RagtimePage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 8);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}

