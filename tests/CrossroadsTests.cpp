// CROSSROADS (0.90.0): the sound pulled between light and dark in time with the song. The engine half: the pulled note
// always lands in key (an outside note counts from the scale note under it); AMOUNT pulls nothing below halfway, then
// up to PULL's steps; off or below halfway every note is untouched bit for bit; on the dark side the note sits its
// steps down, back home on the light side, sliding by BEND. The effect half: off and AMOUNT 0 untouched bit for bit;
// the light side adds an octave up, the dark side an octave down; the level kept; silence stays silent; turned down it
// rests untouched. In the plugin: parameters, matrix destinations, MACRO 3 to rest, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/Crossroads.h"
#include "dsp/synth/Crossroads.h"
#include "ui/CrossroadsPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    using namespace synth;

    constexpr int aMinorKey = 9, minorScale = 2;

    Player sinePlayer (crossroads::Settings c)
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.filter.on = false;
        p.settings.amp.envelope.attackMs = 0.0f;
        p.settings.amp.envelope.sustain = 1.0f;
        p.settings.modulation.bpm = 120.0;   // a bar is 2 s
        p.settings.cross = c;
        return p;
    }

    crossroads::Settings pulling (float amount, int pull = 1)
    {
        crossroads::Settings c;
        c.on = true;
        c.amount = amount;
        c.length = 1;   // 1 BAR a side
        c.key = aMinorKey;
        c.scale = minorScale;
        c.pull = pull;
        c.bend = 0.0f;
        return c;
    }

    double semitonesBetween (const std::vector<float>& x, double fromSeconds, double toSeconds, int note)
    {
        return 12.0 * std::log2 (frequencyOf (x, (size_t) (fromSeconds * sampleRate), (size_t) (toSeconds * sampleRate)) / noteHz (note));
    }

    std::vector<float> sine (double hz, double seconds, float level = 0.3f)
    {
        std::vector<float> x ((size_t) (seconds * sampleRate));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate);
        return x;
    }

    // The level of `hz` in x[from, to) (a Goertzel over the stretch), dB.
    double toneDb (const std::vector<float>& x, size_t from, size_t to, double hz)
    {
        const auto w = juce::MathConstants<double>::twoPi * hz / sampleRate;
        double s1 = 0.0, s2 = 0.0;
        for (auto i = from; i < to; ++i)
        {
            const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) (i - from) / (double) (to - from));
            const auto s = x[i] * window + 2.0 * std::cos (w) * s1 - s2;
            s2 = s1;
            s1 = s;
        }
        const auto power = s1 * s1 + s2 * s2 - 2.0 * std::cos (w) * s1 * s2;
        return 10.0 * std::log10 (power / juce::square ((double) (to - from) / 4.0) + 1.0e-20);
    }

    double rmsDb (const std::vector<float>& x, size_t from, size_t to)
    {
        double sum = 0.0;
        for (auto i = from; i < to; ++i)
            sum += (double) x[i] * x[i];
        return 10.0 * std::log10 (sum / (double) (to - from) + 1.0e-20);
    }

    struct Run
    {
        std::array<std::vector<float>, 2> out;
        bool resting = false;
    };

    Run runCross (std::vector<float> left, std::vector<float> right, effects::Crossroads::Settings s, double bpm = 120.0,
                  int changeAt = -1, effects::Crossroads::Settings changed = {})
    {
        effects::Crossroads fx;
        fx.prepare (sampleRate, blockSize);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            const auto& now = changeAt >= 0 && (int) start >= changeAt ? changed : s;
            fx.process (juce::dsp::AudioBlock<float> (channels, 2, n), now, bpm, (double) start * bpm / 60.0 / sampleRate);
        }
        return { { left, right }, fx.isResting() };
    }

    effects::Crossroads::Settings fullEffect()
    {
        effects::Crossroads::Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.length = 1;
        s.light = 1.0f;
        s.dark = 1.0f;
        return s;
    }
}

TEST_CASE ("CROSSROADS: the pulled note always lands in key; AMOUNT pulls from halfway, up to PULL's steps", "[crossroads]")
{
    using crossroads::pulledNote;
    // A minor (A B C D E F G): A4 one step down is G, two F, four D; C one step down is B.
    CHECK (pulledNote (69, 1, aMinorKey, minorScale) == 67);
    CHECK (pulledNote (69, 2, aMinorKey, minorScale) == 65);
    CHECK (pulledNote (69, 4, aMinorKey, minorScale) == 62);
    CHECK (pulledNote (72, 1, aMinorKey, minorScale) == 71);
    CHECK (pulledNote (64, 2, aMinorKey, minorScale) == 60);   // E down a third: C
    // An outside note (C#) lands in key: from the C under it, one step down is B (not the out-of-key A#).
    CHECK (pulledNote (73, 1, aMinorKey, minorScale) == 71);
    // Every pulled note of every key and scale is in that scale.
    for (int scale = 0; scale < arp::scaleNames.size(); ++scale)
    {
        std::array<int, 12> notes {};
        const auto count = sirocco::scaleNotes (scale, notes);
        if (count == 0)
            continue;
        for (int key = 0; key < 12; ++key)
            for (int note = 36; note < 96; ++note)
                for (const auto steps : { 1, 2, 4 })
                {
                    const auto pulled = pulledNote (note, steps, key, scale);
                    INFO (arp::scaleNames[scale] << " key " << key << " note " << note << " steps " << steps);
                    REQUIRE (pulled < note);
                    const auto degree = ((pulled - key) % 12 + 12) % 12;
                    REQUIRE (std::find (notes.begin(), notes.begin() + count, degree) != notes.begin() + count);
                }
    }
    // THIRD (index 1, two steps): one from halfway, two past three quarters.
    CHECK (crossroads::stepsFor (0.49f, 1) == 0);
    CHECK (crossroads::stepsFor (0.5f, 1) == 1);
    CHECK (crossroads::stepsFor (0.74f, 1) == 1);
    CHECK (crossroads::stepsFor (0.8f, 1) == 2);
    CHECK (crossroads::stepsFor (1.0f, 1) == 2);
    // STEP (index 0): one; FIFTH (index 2, four steps): a step for each eighth past halfway.
    CHECK (crossroads::stepsFor (1.0f, 0) == 1);
    CHECK (crossroads::stepsFor (0.6f, 2) == 1);
    CHECK (crossroads::stepsFor (0.74f, 2) == 2);
    CHECK (crossroads::stepsFor (0.8f, 2) == 3);
    CHECK (crossroads::stepsFor (1.0f, 2) == 4);
    CHECK_FALSE (crossroads::darkAt (1.0, 1));
    CHECK (crossroads::darkAt (5.0, 1));
    CHECK_FALSE (crossroads::darkAt (9.0, 1));
}

TEST_CASE ("CROSSROADS in the engine: off, or AMOUNT under halfway, leaves every note untouched bit for bit", "[crossroads]")
{
    auto plain = sinePlayer ({});
    plain.on (57);
    const auto reference = plain.render (5.0);
    for (const auto amount : { 0.0f, 0.3f, 0.49f })
    {
        INFO (amount);
        auto p = sinePlayer (pulling (amount));
        p.on (57);
        CHECK (p.render (5.0) == reference);
    }
    auto off = pulling (1.0f);
    off.on = false;
    auto p = sinePlayer (off);
    p.on (57);
    CHECK (p.render (5.0) == reference);
}

TEST_CASE ("CROSSROADS in the engine: on the dark side every note sits its steps down the scale, home on the light side", "[crossroads]")
{
    // A3 in A minor, a bar a side (2 s at 120 BPM): light, dark (pulled a THIRD at the top: F, -4), light, dark.
    auto p = sinePlayer (pulling (1.0f, 1));
    p.on (57);
    const auto out = p.render (8.0);
    CHECK_THAT (semitonesBetween (out[0], 0.5, 1.5, 57), WithinAbs (0.0, 0.05));
    CHECK_THAT (semitonesBetween (out[0], 2.5, 3.5, 57), WithinAbs (-4.0, 0.05));
    CHECK_THAT (semitonesBetween (out[0], 4.5, 5.5, 57), WithinAbs (0.0, 0.05));
    CHECK_THAT (semitonesBetween (out[0], 6.5, 7.5, 57), WithinAbs (-4.0, 0.05));
    // At 60%: one step (G, -2).
    auto one = sinePlayer (pulling (0.6f, 1));
    one.on (57);
    const auto stepped = one.render (4.0);
    CHECK_THAT (semitonesBetween (stepped[0], 2.5, 3.5, 57), WithinAbs (-2.0, 0.05));
    // Another note by its own steps: E4 pulled a third lands on C4 (-4), in key.
    auto chord = sinePlayer (pulling (1.0f, 1));
    chord.on (64);
    const auto e = chord.render (4.0);
    CHECK_THAT (semitonesBetween (e[0], 2.5, 3.5, 64), WithinAbs (-4.0, 0.05));
}

TEST_CASE ("CROSSROADS in the engine: BEND slides into the pull", "[crossroads]")
{
    auto quick = sinePlayer (pulling (1.0f, 1));
    quick.on (57);
    const auto fast = quick.render (3.0);
    auto slowSettings = pulling (1.0f, 1);
    slowSettings.bend = 1.0f;   // a 400 ms time constant
    auto slow = sinePlayer (slowSettings);
    slow.on (57);
    const auto bent = slow.render (3.5);
    // 50-150 ms into the dark side: the quick one is there, the slow one still on its way down.
    CHECK_THAT (semitonesBetween (fast[0], 2.05, 2.15, 57), WithinAbs (-4.0, 0.1));
    const auto midway = semitonesBetween (bent[0], 2.05, 2.15, 57);
    CHECK (midway > -3.0);
    CHECK (midway < -0.3);
    // ...and it arrives.
    CHECK_THAT (semitonesBetween (bent[0], 3.3, 3.5, 57), WithinAbs (-4.0, 0.2));
}

TEST_CASE ("CROSSROADS' effect: off and AMOUNT 0 leave the sound untouched bit for bit, and it rests", "[crossroads]")
{
    const auto left = sine (440.0, 5.0), right = sine (330.0, 5.0);
    auto off = fullEffect();
    off.on = false;
    auto run = runCross (left, right, off);
    CHECK (run.out[0] == left);
    CHECK (run.out[1] == right);
    CHECK (run.resting);
    auto zero = fullEffect();
    zero.amount = 0.0005f;
    run = runCross (left, right, zero);
    CHECK (run.out[0] == left);
    CHECK (run.resting);
    // Turned up, then down: back to resting, untouched again.
    auto down = fullEffect();
    down.amount = 0.0f;
    run = runCross (left, right, fullEffect(), 120.0, (int) (2.0 * sampleRate), down);
    CHECK (run.resting);
    for (auto i = (size_t) (4.0 * sampleRate); i < left.size(); ++i)
        REQUIRE (run.out[0][i] == left[i]);
}

TEST_CASE ("CROSSROADS' effect: the light side adds an octave up, the dark side an octave down, the level kept", "[crossroads]")
{
    const auto in = sine (440.0, 8.0);
    const auto run = runCross (in, in, fullEffect());
    const auto bar = (size_t) (2.0 * sampleRate);
    // Light (bar 1, 3) against dark (bar 2, 4), measured inside each bar.
    for (int pair = 0; pair < 2; ++pair)
    {
        const auto lightFrom = (size_t) (2 * pair) * bar + bar / 4, lightTo = lightFrom + bar / 2;
        const auto darkFrom = lightFrom + bar, darkTo = lightTo + bar;
        INFO (pair);
        CHECK (toneDb (run.out[0], lightFrom, lightTo, 880.0) > toneDb (run.out[0], darkFrom, darkTo, 880.0) + 12.0);
        CHECK (toneDb (run.out[0], darkFrom, darkTo, 220.0) > toneDb (run.out[0], lightFrom, lightTo, 220.0) + 12.0);
        CHECK (toneDb (run.out[0], lightFrom, lightTo, 880.0) > toneDb (in, lightFrom, lightTo, 880.0) + 30.0);
        CHECK (toneDb (run.out[0], darkFrom, darkTo, 220.0) > toneDb (in, darkFrom, darkTo, 220.0) + 30.0);
        // The level kept within 2 dB on both sides.
        CHECK (std::abs (rmsDb (run.out[0], lightFrom, lightTo) - rmsDb (in, lightFrom, lightTo)) < 2.0);
        CHECK (std::abs (rmsDb (run.out[0], darkFrom, darkTo) - rmsDb (in, darkFrom, darkTo)) < 2.0);
    }
    // In time with the song at another tempo: at 90 BPM a bar is 2.67 s; the dark side starts there.
    const auto slow = runCross (in, in, fullEffect(), 90.0);
    const auto bar90 = (size_t) (4.0 * 60.0 / 90.0 * sampleRate);
    CHECK (toneDb (slow.out[0], bar90 + bar90 / 4, bar90 + 3 * bar90 / 4, 220.0) > toneDb (slow.out[0], bar90 / 4, 3 * bar90 / 4, 220.0) + 12.0);
}

TEST_CASE ("CROSSROADS' effect: silence and a silent side stay silent", "[crossroads]")
{
    const std::vector<float> silence ((size_t) (5.0 * sampleRate), 0.0f);
    const auto in = sine (440.0, 5.0);
    const auto run = runCross (in, silence, fullEffect());
    for (const auto v : run.out[1])
        REQUIRE (v == 0.0f);
    const auto quiet = runCross (silence, silence, fullEffect());
    for (const auto v : quiet.out[0])
        REQUIRE (v == 0.0f);
}

TEST_CASE ("CROSSROADS in the plugin: parameters, matrix destinations, MACRO 3 to rest, the SIGNATURE slot and its page", "[crossroads][plugin]")
{
    for (const auto* id : { ParamIDs::crossAmount, ParamIDs::crossLight, ParamIDs::crossDark })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Crossroads");
    }
    CHECK (signatureNames().contains ("CROSSROADS"));
    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    for (const auto* id : { ParamIDs::crossOn, ParamIDs::crossAmount, ParamIDs::crossLength, ParamIDs::crossKey, ParamIDs::crossScale,
                            ParamIDs::crossPull, ParamIDs::crossBend, ParamIDs::crossLight, ParamIDs::crossDark })
        CHECK (processor.getParameters().getParameter (id) != nullptr);
    CHECK_FALSE (processor.getCrossroadsPull().on);
    CHECK (processor.getCrossroadsSettings().amount == 0.0f);
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
    set (ParamIDs::crossOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::crossAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    set (ParamIDs::macros[2], 1.0f);
    play (3.0);   // into the dark side (its own count at 120 BPM: a bar of light first)
    CHECK (processor.getCrossroads().getAmount() > 0.9f);
    CHECK (processor.getCrossroadsPull().pulling());
    CHECK (processor.getCrossroadsSide() == 1);
    set (ParamIDs::macros[2], 0.0f);
    play (2.0);
    CHECK (processor.getCrossroads().isResting());

    CrossroadsPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 9);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
