// NAVICOMPUTER (0.86.0): every note moved by a pattern in key, in time. Off or AMOUNT 0: bit for bit untouched; the
// pattern's moves land on the step's pitch; moves keep to the scale (each note by its own steps, so B's "fifth" in C
// major is F); AMOUNT turns steps on in the reveal order; GLIDE slides; the song position decides the step; in the
// plugin it's a parameter set with matrix destinations, and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/synth/NaviComputer.h"
#include "ui/NaviComputerPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    using namespace synth;

    // A pure sine on one note, NAVICOMPUTER set as given; quarter-note steps at 120 BPM (0.5 s each), the song stopped
    // (its own count, started by the key).
    Player sinePlayer (navi::Settings n)
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.filter.on = false;
        p.settings.amp.envelope.attackMs = 0.0f;
        p.settings.amp.envelope.sustain = 1.0f;
        p.settings.modulation.bpm = 120.0;
        p.settings.navi = n;
        return p;
    }

    // The pitch in the middle of step `k` (0.5 s steps), in semitones from `note`.
    double semitonesAt (const std::vector<float>& x, int k, int note)
    {
        const auto from = (size_t) ((k * 0.5 + 0.15) * sampleRate), to = (size_t) ((k * 0.5 + 0.4) * sampleRate);
        return 12.0 * std::log2 (frequencyOf (x, from, to) / noteHz (note));
    }
}

TEST_CASE ("NAVICOMPUTER: off, or AMOUNT 0, leaves every note untouched bit for bit", "[navicomputer]")
{
    navi::Settings off;
    auto plain = sinePlayer (off);
    plain.on (57);
    const auto reference = plain.render (2.0);

    for (const auto amount : { 0.0f, 0.0005f })
    {
        navi::Settings n;
        n.on = true;
        n.amount = amount;
        n.pattern = 3;   // OCTAVES
        n.division = 3;  // 1/4
        auto p = sinePlayer (n);
        p.on (57);
        CHECK (p.render (2.0) == reference);
    }
    navi::Settings unused;   // set up but switched off
    unused.amount = 1.0f;
    unused.pattern = 6;
    auto p = sinePlayer (unused);
    p.on (57);
    CHECK (p.render (2.0) == reference);
}

TEST_CASE ("NAVICOMPUTER: each step moves the note by its pattern's steps of the scale, in time", "[navicomputer]")
{
    navi::Settings n;
    n.on = true;
    n.amount = 1.0f;
    n.pattern = 0;    // ASCEND: 0, 2, 4, 7, ...
    n.division = 3;   // 1/4: a step every half second at 120 BPM
    n.key = 9;        // A
    n.scale = 2;      // minor
    auto p = sinePlayer (n);
    p.on (57);        // A3
    const auto out = p.render (4.0);
    // A minor from A: the root, a third up (C, +3), a fifth (E, +7), the octave (+12).
    CHECK_THAT (semitonesAt (out[0], 0, 57), WithinAbs (0.0, 0.05));
    CHECK_THAT (semitonesAt (out[0], 1, 57), WithinAbs (3.0, 0.05));
    CHECK_THAT (semitonesAt (out[0], 2, 57), WithinAbs (7.0, 0.05));
    CHECK_THAT (semitonesAt (out[0], 3, 57), WithinAbs (12.0, 0.05));
    CHECK_THAT (semitonesAt (out[0], 4, 57), WithinAbs (0.0, 0.05));
    // The move lands on the step: just before step 1 still the root, just after it the third.
    const auto edge = (size_t) (0.5 * sampleRate);
    CHECK_THAT (12.0 * std::log2 (frequencyOf (out[0], edge - 2400, edge - 100) / noteHz (57)), WithinAbs (0.0, 0.05));
    CHECK_THAT (12.0 * std::log2 (frequencyOf (out[0], edge + 100, edge + 2400) / noteHz (57)), WithinAbs (3.0, 0.05));
}

TEST_CASE ("NAVICOMPUTER: each note moves by its own steps of the scale, so a chord stays in key", "[navicomputer]")
{
    // C major, FIFTHS: step 2 moves four steps of the scale. E's is B (+7); B's is F (+6), not F#.
    navi::Settings n;
    n.on = true;
    n.amount = 1.0f;
    n.pattern = 2;    // FIFTHS: 0, 0, 4, ...
    n.division = 3;
    n.key = 0;
    n.scale = 1;      // major
    for (const auto [note, expected] : { std::pair { 64, 7.0 }, std::pair { 71, 6.0 }, std::pair { 60, 7.0 } })
    {
        INFO (note);
        auto p = sinePlayer (n);
        p.on (note);
        const auto out = p.render (1.6);
        CHECK_THAT (semitonesAt (out[0], 0, note), WithinAbs (0.0, 0.05));
        CHECK_THAT (semitonesAt (out[0], 2, note), WithinAbs (expected, 0.05));
    }
    // A note outside the scale moves in parallel from the scale note under it (C# in C major: from C, a fifth up to G#).
    auto p = sinePlayer (n);
    p.on (61);
    CHECK_THAT (semitonesAt (p.render (1.6)[0], 2, 61), WithinAbs (7.0, 0.05));
}

TEST_CASE ("NAVICOMPUTER: AMOUNT turns steps on in the reveal order; a pentatonic scale takes the nearest step", "[navicomputer]")
{
    // Only the first step revealed (step 15) moves at 1/16.
    for (int k = 0; k < navi::numSteps; ++k)
        CHECK (navi::stepActive (k, 1.0f / 16.0f) == (k == 15));
    CHECK_FALSE (navi::stepActive (0, 0.9f));   // the downbeat last
    CHECK (navi::stepActive (0, 1.0f));
    int moving = 0;
    for (int k = 0; k < navi::numSteps; ++k)
        moving += navi::stepActive (k, 0.5f) ? 1 : 0;
    CHECK (moving == 8);

    // In the plugin's terms: at AMOUNT 0.25 (four steps: 15, 7, 11, 3) OCTAVES' step 3 moves, step 1 doesn't.
    navi::Settings n;
    n.on = true;
    n.amount = 0.25f;
    n.pattern = 3;    // OCTAVES: 0, 7, 0, 7
    n.division = 3;
    n.scale = 2;
    auto p = sinePlayer (n);
    p.on (57);
    const auto out = p.render (2.2);
    CHECK_THAT (semitonesAt (out[0], 1, 57), WithinAbs (0.0, 0.05));
    CHECK_THAT (semitonesAt (out[0], 3, 57), WithinAbs (12.0, 0.05));

    // A seven-note move in a five-note scale: the octave stays the octave, a fifth the nearest step.
    CHECK (navi::scaleSteps (7, 11) == 5);    // minor pentatonic: an octave is five steps
    CHECK (navi::scaleSteps (-7, 11) == -5);
    CHECK (navi::scaleSteps (4, 11) == 3);    // 4 x 5 / 7 = 2.86: the third step (the fifth)
    CHECK (navi::semitonesFor (57, 4, { true, 1.0f, 0, 0, 9, 11 }) == 7);   // A minor pentatonic: A's fifth is E
}

TEST_CASE ("NAVICOMPUTER: GLIDE slides into each move; CHAOS changes steps the same way every time", "[navicomputer]")
{
    navi::Settings n;
    n.on = true;
    n.amount = 1.0f;
    n.pattern = 3;   // OCTAVES
    n.division = 3;
    n.scale = 2;
    n.glide = 1.0f;
    auto p = sinePlayer (n);
    p.on (57);
    const auto out = p.render (1.2);
    // 20 ms into step 1 it's on its way up (the slide's time constant is a tenth of the step, 50 ms: there by about
    // 40% of the step).
    const auto early = 12.0 * std::log2 (frequencyOf (out[0], (size_t) (0.505 * sampleRate), (size_t) (0.525 * sampleRate)) / noteHz (57));
    CHECK (early > 2.0);
    CHECK (early < 11.0);
    CHECK_THAT (12.0 * std::log2 (frequencyOf (out[0], (size_t) (0.85 * sampleRate), (size_t) (0.99 * sampleRate)) / noteHz (57)), WithinAbs (12.0, 0.05));

    // CHAOS: a bar's moves are fixed (the same bar, the same choice), and at full most differ from the pattern.
    navi::Settings c = n;
    c.chaos = 1.0f;
    int changed = 0;
    for (std::int64_t step = 0; step < 64; ++step)
    {
        CHECK (navi::moveAt (c, step) == navi::moveAt (c, step));
        changed += navi::moveAt (c, step) != navi::patterns[3][(size_t) (step % 16)] ? 1 : 0;
    }
    CHECK (changed > 20);
}

TEST_CASE ("NAVICOMPUTER: with the song playing, the step follows the song position", "[navicomputer]")
{
    navi::Settings n;
    n.on = true;
    n.amount = 1.0f;
    n.pattern = 0;    // ASCEND
    n.division = 3;   // quarters
    n.key = 9;
    n.scale = 2;
    auto p = sinePlayer (n);
    p.settings.modulation.ppq = 2.0;   // the song at beat 3 (step 2: the fifth), held there for the render's first block
    p.on (57);
    std::vector<float> out;
    for (int b = 0; b < 40; ++b)
    {
        std::vector<float> left (blockSize), right (blockSize);
        p.engine.process (left.data(), right.data(), blockSize, p.pending, 0, p.settings);
        p.settings.modulation.ppq += blockSize * 2.0 / sampleRate;   // 120 BPM
        out.insert (out.end(), left.begin(), left.end());
    }
    CHECK_THAT (12.0 * std::log2 (frequencyOf (out, 2000, 9000) / noteHz (57)), WithinAbs (7.0, 0.05));
    CHECK (p.engine.getNaviStep() == 2);
}

TEST_CASE ("NAVICOMPUTER in the plugin: parameters, matrix destinations, MACRO 3 back to untouched, its page", "[navicomputer][plugin]")
{
    for (const auto* id : { ParamIDs::naviAmount, ParamIDs::naviChaos, ParamIDs::naviGlide })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Navicomputer");
    }
    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    for (const auto* id : { ParamIDs::naviOn, ParamIDs::naviAmount, ParamIDs::naviPattern, ParamIDs::naviDivision, ParamIDs::naviKey,
                            ParamIDs::naviScale, ParamIDs::naviChaos, ParamIDs::naviGlide })
        CHECK (processor.getParameters().getParameter (id) != nullptr);
    const auto defaults = processor.getNaviSettings();
    CHECK_FALSE (defaults.on);
    CHECK (defaults.amount == 0.0f);

    const auto set = [&processor] (const char* id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    const auto play = [&processor] (double seconds)
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }
        return out;
    };
    processor.seedRandomForTest (1);
    processor.getKeyboardState().noteOn (1, 57, 0.9f);
    set (ParamIDs::naviOn, 1.0f);
    set (ParamIDs::naviPattern, 3.0f);   // OCTAVES
    set (ParamIDs::naviAmount, 1.0f);
    play (0.6);
    CHECK (processor.getNaviSettings().moving());
    CHECK (processor.getNaviStep() >= 0);
    set (ParamIDs::naviAmount, 0.0f);
    play (0.2);
    CHECK (processor.getNaviStep() == -1);

    // Its page shows the 16 steps and its controls.
    NaviComputerPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 8);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
    CHECK (signatureNames().contains ("NAVICOMPUTER"));
}
