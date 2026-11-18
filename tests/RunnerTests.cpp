// RUNNER (1.0.0): every held note struck again in time with the song, the speed stepping up with every fifth of
// AMOUNT (1/4, 1/8, 1/16, 1/32, 1/64). Off or AMOUNT 0: bit for bit untouched; the strikes land on each speed's grid
// (at two tempos, and following the song's position); a change of speed lands on the new grid without a stumble;
// ACCENT softens the strikes off the beat; a held sound is struck too (the LIFT); a released note isn't; in the
// plugin its matrix destinations, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/synth/Runner.h"
#include "ui/RackSpecs.h"
#include "ui/RunnerPage.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    using namespace synth;

    // A pure sine, struck and dying away to 10%, so a strike shows as the level jumping back up.
    Player runPlayer (runner::Settings r, double bpm = 60.0, float sustain = 0.1f, float decayMs = 40.0f)
    {
        Player p;
        p.osc().wave = OscWave::sine;
        p.settings.filter.on = false;
        p.settings.amp.envelope.attackMs = 0.3f;
        p.settings.amp.envelope.decayMs = decayMs;
        p.settings.amp.envelope.sustain = sustain;
        p.settings.amp.envelope.releaseMs = 20.0f;
        p.settings.modulation.bpm = bpm;
        p.settings.runner = r;
        return p;
    }

    runner::Settings run (float amount)
    {
        runner::Settings r;
        r.on = true;
        r.amount = amount;
        r.accent = 0.0f;
        r.lift = 0.5f;
        return r;
    }

    // The level just after time `t` against just before (windows `w` long): a strike shows as a big rise.
    double riseAt (const std::vector<float>& x, double t, double w = 0.004)
    {
        const auto before = rms (x, (size_t) ((t - w - 0.0015) * sampleRate), (size_t) ((t - 0.0015) * sampleRate));
        const auto after = rms (x, (size_t) ((t + 0.0008) * sampleRate), (size_t) ((t + 0.0008 + w) * sampleRate));
        return db (after / juce::jmax (1.0e-9, before));
    }

    float peakOf (const std::vector<float>& x, double from, double to)
    {
        float peak = 0.0f;
        for (auto i = (size_t) (from * sampleRate); i < (size_t) (to * sampleRate) && i < x.size(); ++i)
            peak = std::max (peak, std::abs (x[i]));
        return peak;
    }
}

TEST_CASE ("RUNNER: AMOUNT steps the speed every fifth of the knob, from 1/4 to 1/64", "[runner]")
{
    CHECK (runner::speedAt (0.0f) == 0);
    CHECK (runner::speedAt (0.0009f) == 0);
    CHECK (runner::speedAt (0.01f) == 1);
    CHECK (runner::speedAt (0.199f) == 1);
    CHECK (runner::speedAt (0.2f) == 2);
    CHECK (runner::speedAt (0.39f) == 2);
    CHECK (runner::speedAt (0.41f) == 3);
    CHECK (runner::speedAt (0.6f) == 4);
    CHECK (runner::speedAt (0.79f) == 4);
    CHECK (runner::speedAt (0.8f) == 5);
    CHECK (runner::speedAt (1.0f) == 5);
    CHECK (runner::beatsPerStrike (1) == 1.0);
    CHECK (runner::beatsPerStrike (5) == 1.0 / 16.0);
    CHECK (runner::speedNames[runner::speedAt (1.0f)] == "1/64");
}

TEST_CASE ("RUNNER: off, or AMOUNT 0, leaves every note untouched bit for bit", "[runner]")
{
    runner::Settings off;
    auto plain = runPlayer (off, 60.0, 0.6f);
    plain.on (48);
    plain.on (55);
    const auto reference = plain.render (3.0);
    for (const auto amount : { 0.0f, 0.0005f })
    {
        auto p = runPlayer (run (amount), 60.0, 0.6f);
        p.on (48);
        p.on (55);
        CHECK (p.render (3.0) == reference);
        CHECK (p.engine.getRunnerStrikes() == 0);
    }
    auto unused = run (1.0f);
    unused.on = false;
    auto p = runPlayer (unused, 60.0, 0.6f);
    p.on (48);
    p.on (55);
    CHECK (p.render (3.0) == reference);
}

TEST_CASE ("RUNNER: each speed strikes on its own grid, at two tempos", "[runner]")
{
    for (const auto bpm : { 60.0, 128.0 })
        for (int speed = 1; speed <= 5; ++speed)
        {
            const auto amount = 0.2f * (float) speed - 0.1f;
            REQUIRE (runner::speedAt (amount) == speed);
            const auto every = 60.0 / bpm * runner::beatsPerStrike (speed);   // seconds a strike
            auto p = runPlayer (run (amount), bpm, 0.1f, (float) juce::jmin (40.0, every * 1000.0 * 0.4));
            p.on (57);
            const auto seconds = 1.2;
            const auto out = p.render (seconds);
            INFO ("BPM " << bpm << ", speed " << runner::speedNames[speed]);
            const auto w = juce::jmin (0.004, every * 0.25);
            int checked = 0;
            for (int k = 1; k * every < seconds - 0.02 && checked < 8; ++k, ++checked)
            {
                INFO ("strike " << k);
                CHECK (riseAt (out[0], k * every, w) > 4.0);
            }
            CHECK (checked >= 1);
            // Halfway between two strikes nothing new: the level only falls. (1.2.0: halfway between the first two
            // strikes inside the render: at 60 BPM in quarters 1.5 strikes is past its 1.2 s and read past the end.)
            const auto halfway = 1.5 * every + w + 0.002 < seconds ? 1.5 * every : 0.5 * every;
            CHECK (riseAt (out[0], halfway, w) < 1.5);   // (a 4 ms window reads a steady tone up to 0.7 dB apart)
            const auto expected = (int) std::floor ((seconds - 1.0e-6) / every - 1.0e-9);   // the key's own strike at 0 isn't counted
            CHECK (std::abs (p.engine.getRunnerStrikes() - expected) <= 1);
        }
}

TEST_CASE ("RUNNER follows the song's position while it plays", "[runner]")
{
    // At 120 BPM the song starts at beat 0.3: the first quarter-note strike lands at beat 1, 0.35 s in.
    auto p = runPlayer (run (0.1f), 120.0);
    p.on (57);
    std::array<std::vector<float>, 2> out;
    const auto total = (int) (1.6 * sampleRate);
    for (auto& c : out)
        c.resize ((size_t) total);
    for (int start = 0; start < total; start += blockSize)
    {
        const auto n = juce::jmin (blockSize, total - start);
        p.settings.modulation.ppq = 0.3 + (double) start / sampleRate * 2.0;
        p.engine.process (out[0].data() + start, out[1].data() + start, n, p.pending, 0, p.settings);
    }
    for (const auto beat : { 1.0, 2.0, 3.0 })
    {
        INFO ("beat " << beat);
        CHECK (riseAt (out[0], (beat - 0.3) * 0.5) > 4.0);
    }
    CHECK (riseAt (out[0], 0.5 * 0.5) < 1.5);   // beat 0.8: nothing
}

TEST_CASE ("RUNNER: turning AMOUNT up lands on the new speed's grid without a stumble", "[runner]")
{
    // 60 BPM: quarters for a second, then sixteenths: strikes at 1.25, 1.5, 1.75 s (on the old grid's beat too).
    auto p = runPlayer (run (0.1f), 60.0, 0.1f, 30.0f);
    p.on (57);
    const auto first = p.render (1.1);
    CHECK (riseAt (first[0], 1.0) > 4.0);
    const auto before = p.engine.getRunnerStrikes();
    p.settings.runner.amount = 0.5f;   // 1/16
    const auto next = p.render (1.0);
    // Time 1.1 s is 0 here.
    for (const auto t : { 1.25, 1.5, 1.75, 2.0 })
    {
        INFO ("at " << t);
        CHECK (riseAt (next[0], t - 1.1) > 4.0);
    }
    CHECK (riseAt (next[0], 1.375 - 1.1) < 1.5);   // between sixteenths: nothing
    CHECK (p.engine.getRunnerStrikes() - before == 4);   // 1.25 to 2.0: no strike twice, none lost
}

TEST_CASE ("RUNNER: ACCENT strikes off the beat softer", "[runner]")
{
    // Sixteenths at 60 BPM, a note that dies fast: each strike's peak is its velocity.
    const auto peaks = [] (float accent)
    {
        auto r = run (0.5f);
        r.accent = accent;
        auto p = runPlayer (r, 60.0, 0.0f, 30.0f);
        p.settings.amp.envelope.sustain = 0.05f;   // something left to strike
        p.settings.amp.velocity = 1.0f;
        p.on (57, 1.0f);
        const auto out = p.render (2.2);
        return std::pair { peakOf (out[0], 1.0, 1.1), peakOf (out[0], 1.25, 1.35) };   // a beat, then an off-beat
    };
    const auto [beat0, off0] = peaks (0.0f);
    const auto [beat1, off1] = peaks (1.0f);
    CHECK (std::abs (db (off0 / beat0)) < 0.5);   // no accent: all alike
    CHECK (db (off1 / beat1) < -4.0);             // full accent: the off-beat at 40% velocity
    CHECK (std::abs (db (beat1 / beat0)) < 0.5);  // the beat itself unchanged
}

TEST_CASE ("RUNNER: a held sound is struck too, by the LIFT", "[runner]")
{
    auto r = run (0.3f);   // eighths: every 0.5 s at 60 BPM
    r.lift = 1.0f;
    auto p = runPlayer (r, 60.0, 1.0f);
    p.on (57);
    const auto out = p.render (2.2);
    for (int k = 1; k < 4; ++k)
    {
        const auto t = 0.5 * k;
        const auto mid = rms (out[0], (size_t) ((t - 0.25) * sampleRate), (size_t) ((t - 0.23) * sampleRate));
        const auto dip = rms (out[0], (size_t) ((t - 0.0015) * sampleRate), (size_t) ((t + 0.0002) * sampleRate));
        INFO ("strike " << k);
        CHECK (db (dip / mid) < -6.0);
    }
    r.lift = 0.0f;
    auto flat = runPlayer (r, 60.0, 1.0f);
    flat.on (57);
    const auto even = flat.render (2.2);
    const auto mid = rms (even[0], (size_t) (0.75 * sampleRate), (size_t) (0.77 * sampleRate));
    const auto at = rms (even[0], (size_t) (0.9985 * sampleRate), (size_t) (1.0002 * sampleRate));
    CHECK (std::abs (db (at / mid)) < 1.0);
}

TEST_CASE ("RUNNER lets go when the key does", "[runner]")
{
    auto p = runPlayer (run (0.5f), 60.0, 0.6f);
    p.on (57);
    p.render (1.0);
    const auto struck = p.engine.getRunnerStrikes();
    CHECK (struck >= 3);
    p.off (57);
    p.render (0.3);
    const auto after = p.engine.getRunnerStrikes();
    const auto tail = p.render (1.0);
    CHECK (p.engine.getRunnerStrikes() == after);   // released: no more strikes
    CHECK (peakOf (tail[0], 0.5, 1.0) < 1.0e-4f);
}

TEST_CASE ("RUNNER in the plugin: matrix destinations, the SIGNATURE slot and its page", "[runner][plugin]")
{
    for (const auto* id : { ParamIDs::runnerAmount, ParamIDs::runnerAccent, ParamIDs::runnerLift })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Runner");
    }
    CHECK (signatureNames().contains ("RUNNER"));
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
    set (ParamIDs::runnerOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::runnerAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    processor.getKeyboardState().noteOn (1, 57, 0.9f);
    play (0.5);
    CHECK (processor.getRunnerStrikes() == 0);   // MACRO 3 at its left: nothing struck
    set (ParamIDs::macros[2], 0.5f);
    play (2.0);
    CHECK (processor.getRunnerSettings().speed() == 3);   // the matrix's AMOUNT: half the knob, 1/16
    CHECK (processor.getRunnerStrikes() > 10);
    CHECK (signatureOn (processor, signatureNames().indexOf ("RUNNER")));

    RunnerPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 4);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
