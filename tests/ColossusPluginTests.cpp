// COLOSSUS in PR7-K (0.82.0): off and at SIZE 0 change nothing; at full, a held note gains its octave under and a hall
// that rings on after it; SIZE, DEPTH and HALL are mod-matrix destinations (FX Colossus), MACRO 3 takes it up and back,
// untouched again once settled; the SIGNATURE slot's twenty-third effect and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "ui/ColossusPage.h"
#include "ui/RackSpecs.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    struct Plugin
    {
        ParagonX1AudioProcessor processor;
        Plugin()
        {
            processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
            processor.prepareToPlay (sampleRate, blockSize);
            processor.seedRandomForTest (1);
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        std::array<std::vector<float>, 2> play (double seconds, int note = 57, double releaseAt = 1.0e9)
        {
            std::array<std::vector<float>, 2> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            processor.getKeyboardState().noteOn (1, note, 0.9f);
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                if ((double) b * blockSize >= releaseAt * sampleRate)   // in double: 1e9 s in an int is out of range (MSVC and Clang released at once)
                    processor.getKeyboardState().noteOff (1, note, 0.0f);
                buffer.clear();
                processor.processBlock (buffer, midi);
                for (int c = 0; c < 2; ++c)
                    out[(size_t) c].insert (out[(size_t) c].end(), buffer.getReadPointer (c), buffer.getReadPointer (c) + blockSize);
            }
            return out;
        }
    };
}

namespace
{
    double powerOf (const std::vector<float>& x, size_t from, size_t to)
    {
        double p = 0.0;
        for (auto i = from; i < juce::jmin (to, x.size()); ++i)
            p += (double) x[i] * x[i];
        return p / (double) juce::jmax ((size_t) 1, juce::jmin (to, x.size()) - from);
    }
}

TEST_CASE ("COLOSSUS in the plugin: off and SIZE 0 change nothing; at full a note gains a hall that rings on", "[colossus][plugin]")
{
    Plugin plain;
    const auto reference = plain.play (4.0, 57, 1.5);

    Plugin resting;
    resting.set (ParamIDs::colossusOn, 1.0f);
    resting.set (ParamIDs::colossusWeight, 1.0f);
    CHECK (resting.play (4.0, 57, 1.5) == reference);

    Plugin big;
    big.set (ParamIDs::colossusOn, 1.0f);
    big.set (ParamIDs::colossusSize, 1.0f);
    const auto out = big.play (4.0, 57, 1.5);
    // After the note has died away, the hall still rings.
    const auto from = (size_t) (2.6 * sampleRate), to = (size_t) (3.2 * sampleRate);
    const auto dryTail = powerOf (reference[0], from, to), bigTail = powerOf (out[0], from, to);
    INFO ("tail: plain " << 10.0 * std::log10 (dryTail + 1e-20) << " dB, COLOSSUS " << 10.0 * std::log10 (bigTail + 1e-20) << " dB");
    CHECK (bigTail > 100.0 * (dryTail + 1.0e-12));
    // While held, about as loud as the plain note (the level kept, the hall added).
    const auto heldDb = 10.0 * std::log10 (powerOf (out[0], (size_t) (0.6 * sampleRate), (size_t) (1.4 * sampleRate))
                                           / powerOf (reference[0], (size_t) (0.6 * sampleRate), (size_t) (1.4 * sampleRate)));
    INFO ("held: " << heldDb << " dB over the plain note");
    CHECK (heldDb > -3.0);
    CHECK (heldDb < 6.0);
}

TEST_CASE ("COLOSSUS in the plugin: MACRO 3 takes it up through the matrix and back to untouched", "[colossus][plugin]")
{
    for (const auto* id : { ParamIDs::colossusSize, ParamIDs::colossusDepth, ParamIDs::colossusHall })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Colossus");
    }

    Plugin plain;
    const auto reference = plain.play (16.0);

    Plugin p;
    p.set (ParamIDs::colossusOn, 1.0f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::colossusSize, 1.0f, false, 0.0f, 0.0f });
    p.processor.setModMatrix (m);
    p.set (ParamIDs::macros[2], 1.0f);
    std::array<std::vector<float>, 2> out;
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    p.processor.getKeyboardState().noteOn (1, 57, 0.9f);
    for (int b = 0; b < (int) (16.0 * sampleRate / blockSize); ++b)
    {
        if (b * blockSize >= (int) (1.0 * sampleRate))
            p.set (ParamIDs::macros[2], 0.0f);
        buffer.clear();
        p.processor.processBlock (buffer, midi);
        out[0].insert (out[0].end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
    }
    const auto at = (size_t) (0.5 * sampleRate), late = (size_t) (15.0 * sampleRate);
    CHECK (std::abs (out[0][at] - reference[0][at]) + std::abs (out[0][at + 100] - reference[0][at + 100]) > 0.0f);
    // The hall (up to 10 s) dies away, then the sound passes untouched again.
    CHECK (p.processor.getColossus().isResting());
    CHECK (std::equal (out[0].begin() + (long) late, out[0].end(), reference[0].begin() + (long) late));
}

TEST_CASE ("COLOSSUS: the SIGNATURE slot's twenty-third effect, with its page", "[colossus][ui]")
{
    CHECK (signatureNames()[22] == "COLOSSUS");
    ParagonX1AudioProcessor processor;
    ColossusPage page (processor);
    page.setSize (1200, 600);
    CHECK (page.getShownControls().size() == 8);
    for (auto* c : page.getShownControls())
        CHECK (! c->getBounds().isEmpty());
}
