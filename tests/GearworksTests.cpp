// GEARWORKS (0.91.0): the sound caught in the machinery. Off and AMOUNT 0 untouched bit for bit; the gears' teeth cut
// on exactly their steps at any tempo, the pattern repeating every turn; the level kept; METAL rings at 2.76 x the
// lowest note; CLANK strikes on the teeth; silence and a silent side stay silent; turned down it rests untouched; in
// the plugin its matrix destinations, MACRO 3, the SIGNATURE slot and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "dsp/effects/Gearworks.h"
#include "ui/GearworksPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{
    using G = effects::Gearworks;
    using Settings = G::Settings;

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
        int clanks = 0;
    };

    Run runGears (std::vector<float> left, std::vector<float> right, Settings s, double bpm = 120.0, float note = 57.0f,
                  int changeAt = -1, Settings changed = {})
    {
        G fx;
        fx.prepare (sampleRate, blockSize);
        fx.seedRandom (1);
        for (size_t start = 0; start < left.size(); start += blockSize)
        {
            const auto n = std::min ((size_t) blockSize, left.size() - start);
            float* channels[] = { left.data() + start, right.data() + start };
            const auto& now = changeAt >= 0 && (int) start >= changeAt ? changed : s;
            fx.setNotes (&note, 1);
            fx.process (juce::dsp::AudioBlock<float> (channels, 2, n), now, bpm, (double) start * bpm / 60.0 / sampleRate);
        }
        return { { left, right }, fx.isResting(), fx.getClanks() };
    }

    // Only the gears: no metal, grind, steam or clank.
    Settings gearsOnly()
    {
        Settings s;
        s.on = true;
        s.amount = 1.0f;
        s.ratio = 0;      // 3 : 4
        s.division = 0;   // 1/16
        s.bite = 1.0f;
        s.metal = s.grind = s.steam = s.clank = 0.0f;
        return s;
    }

    double rmsDb (const std::vector<float>& x, size_t from, size_t length)
    {
        double sum = 0.0;
        for (size_t i = from; i < from + length && i < x.size(); ++i)
            sum += (double) x[i] * x[i];
        return 10.0 * std::log10 (sum / (double) juce::jmax ((size_t) 1, length) + 1.0e-20);
    }
}

TEST_CASE ("GEARWORKS: the gears' teeth, a turn, the share cut", "[gearworks]")
{
    // 3 : 4: a tooth on steps 2, 5, 8, 11 (every 3) and 3, 7, 11 (every 4); 11 both; a turn of 12.
    CHECK (G::turnSteps (0) == 12);
    CHECK (G::teethAt (2, 0) == 1);
    CHECK (G::teethAt (3, 0) == 2);
    CHECK (G::teethAt (11, 0) == 3);
    CHECK (G::teethAt (0, 0) == 0);
    CHECK (G::teethAt (12 + 2, 0) == 1);   // and again next turn
    CHECK (G::cutShare (0) == 0.5f);       // 2 3 5 7 8 11 of 12
    CHECK (G::turnSteps (3) == 35);        // 5 : 7
    CHECK (G::ratioNames.size() == (int) G::ratios.size());
}

TEST_CASE ("GEARWORKS: off and AMOUNT 0 leave the sound untouched bit for bit", "[gearworks]")
{
    const auto in = sine (220.0, 3.0);
    auto s = gearsOnly();
    s.on = false;
    auto off = runGears (in, in, s);
    CHECK (off.out[0] == in);
    CHECK (off.out[1] == in);
    s.on = true;
    s.amount = 0.0f;
    auto zero = runGears (in, in, s);
    CHECK (zero.out[0] == in);
    CHECK (zero.resting);
}

TEST_CASE ("GEARWORKS: each tooth cuts its own step, in time with the song at any tempo", "[gearworks]")
{
    for (const auto bpm : { 90.0, 140.0 })
    {
        INFO (bpm << " BPM");
        const auto in = sine (880.0, 8.0);   // a tone far above a step's rate, so a step's level is the gate's
        const auto run = runGears (in, in, gearsOnly(), bpm);
        const auto stepSamples = 0.25 * 60.0 / bpm * sampleRate;
        double cutDb = 0.0, openDb = 0.0;
        int cuts = 0, opens = 0;
        for (int k = 4; k < 40; ++k)   // after AMOUNT has settled
        {
            // The middle of the step (the 1.5 / 4 ms edges left out).
            const auto from = (size_t) (stepSamples * (k + 0.2));
            const auto length = (size_t) (stepSamples * 0.6);
            const auto db = rmsDb (run.out[0], from, length);
            if (G::teethAt (k, 0) != 0)
            {
                cutDb += db;
                ++cuts;
            }
            else
            {
                openDb += db;
                ++opens;
            }
        }
        cutDb /= cuts;
        openDb /= opens;
        INFO ("cut steps " << cutDb << " dB, open " << openDb << " dB");
        CHECK (openDb - cutDb > 18.0);   // BITE 1: to 8%, about 22 dB down
    }
}

TEST_CASE ("GEARWORKS: the level kept over a whole turn", "[gearworks]")
{
    const auto in = sine (220.0, 8.0);
    for (const auto bite : { 0.5f, 1.0f })
    {
        auto s = gearsOnly();
        s.bite = bite;
        s.metal = 0.6f;
        s.grind = 0.3f;
        const auto run = runGears (in, in, s);
        const auto turn = (size_t) (12 * 0.25 * 0.5 * sampleRate);   // 12 sixteenths at 120 BPM
        const auto before = rmsDb (in, sampleRate, 4 * turn);
        const auto after = rmsDb (run.out[0], sampleRate, 4 * turn);
        INFO ("BITE " << bite << ": " << after - before << " dB");
        CHECK (std::abs (after - before) < 2.0);
    }
}

TEST_CASE ("GEARWORKS: METAL rings at 2.76 x the lowest note", "[gearworks]")
{
    // A3 (220 Hz) through it: the ring's partner is 607 Hz, so sidebands at 827 and 387 Hz.
    const auto in = sine (220.0, 4.0);
    auto s = gearsOnly();
    s.bite = 0.0f;
    s.metal = 1.0f;
    const auto run = runGears (in, in, s);
    const auto from = (size_t) sampleRate, length = (size_t) (2 * sampleRate);
    const auto side = levelAt (run.out[0], 220.0 + 2.76 * 220.0, from, length);
    const auto plainSide = levelAt (in, 220.0 + 2.76 * 220.0, from, length);
    const auto low = levelAt (run.out[0], 2.76 * 220.0 - 220.0, from, length);
    INFO ("sidebands " << juce::Decibels::gainToDecibels (side) << " / " << juce::Decibels::gainToDecibels (low)
                        << " dB, plain " << juce::Decibels::gainToDecibels (plainSide));
    CHECK (side > 0.05);
    CHECK (low > 0.05);
    CHECK (plainSide < 0.001);
}

TEST_CASE ("GEARWORKS: CLANK strikes on the teeth, and silence and a silent side stay silent", "[gearworks]")
{
    auto s = gearsOnly();
    s.bite = 0.0f;
    s.clank = 1.0f;
    s.steam = 1.0f;
    s.metal = 0.5f;
    s.grind = 0.5f;
    // A note: a clank on each tooth (6 a turn of 12 sixteenths: 16 in 4 s at 120 BPM), at the first gear's 1.2 kHz.
    const auto in = sine (110.0, 4.0);
    const auto run = runGears (in, in, s);
    CHECK (run.clanks >= 15);
    CHECK (levelAt (run.out[0], 1200.0, (size_t) sampleRate, (size_t) (2 * sampleRate)) > 0.003);
    // Silence: nothing at all.
    const std::vector<float> silence (in.size(), 0.0f);
    const auto quiet = runGears (silence, silence, s);
    CHECK (*std::max_element (quiet.out[0].begin(), quiet.out[0].end()) == 0.0f);
    // A silent right side stays silent.
    const auto oneSide = runGears (in, silence, s);
    float peak = 0.0f;
    for (auto v : oneSide.out[1])
        peak = std::max (peak, std::abs (v));
    CHECK (peak == 0.0f);
}

TEST_CASE ("GEARWORKS: turned down it rests untouched", "[gearworks]")
{
    const auto in = sine (220.0, 6.0);
    auto s = gearsOnly();
    s.clank = 1.0f;
    s.steam = 1.0f;
    auto off = s;
    off.amount = 0.0f;
    const auto run = runGears (in, in, s, 120.0, 57.0f, (int) (2 * sampleRate), off);
    CHECK (run.resting);
    // The last second: untouched.
    for (size_t i = (size_t) (5 * sampleRate); i < in.size(); ++i)
        if (run.out[0][i] != in[i])
        {
            FAIL ("not untouched at sample " << i);
            break;
        }
}

TEST_CASE ("CPU: GEARWORKS", "[gearworks][cpu]")
{
    const auto in = sine (220.0, 8.0);
    auto s = gearsOnly();
    s.bite = 0.8f;
    s.metal = s.grind = s.steam = s.clank = 1.0f;
    const auto begin = std::chrono::steady_clock::now();
    runGears (in, in, s);
    const auto percent = 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - begin).count() / 8.0;
    WARN ("GEARWORKS, stereo: " << percent << "% of a core (the test's own input included)");
    CHECK (percent < 10.0);
}

TEST_CASE ("GEARWORKS in the plugin: matrix destinations, MACRO 3 to rest, the SIGNATURE slot and its page", "[gearworks][plugin]")
{
    for (const auto* id : { ParamIDs::gearAmount, ParamIDs::gearMetal, ParamIDs::gearGrind })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Gearworks");
    }
    CHECK (signatureNames().contains ("GEARWORKS"));
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
    double peak = 0.0;
    const auto play = [&] (double seconds)
    {
        juce::MidiBuffer midi;
        for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            peak = std::max (peak, (double) buffer.getMagnitude (0, blockSize));
        }
    };
    processor.getKeyboardState().noteOn (1, 57, 0.9f);
    set (ParamIDs::gearOn, 1.0f);
    auto m = processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::gearAmount, 1.0f, false, 0.0f, 0.0f });
    processor.setModMatrix (m);
    set (ParamIDs::macros[2], 1.0f);
    play (3.0);
    CHECK (processor.getGearworks().getAmount() > 0.9f);
    CHECK (processor.getGearworks().getClanks() > 5);
    CHECK (peak > 0.01);
    set (ParamIDs::macros[2], 0.0f);
    play (2.0);
    CHECK (processor.getGearworks().isResting());

    GearworksPage page (processor);
    page.setSize (1200, 700);
    CHECK (page.getShownControls().size() == 9);
    juce::Image image (juce::Image::ARGB, 1200, 700, true);
    juce::Graphics g (image);
    page.paintEntireComponent (g, false);
}
