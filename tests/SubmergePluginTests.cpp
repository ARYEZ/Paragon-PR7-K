// AFTERMATH in PR7-K (0.79.0): off and at SEAL 0 change nothing; sealed in, a held note darkens; SEAL, RING and HUM are
// mod-matrix destinations (FX Submerge), MACRO 3 seals in and back, untouched again once settled; the SIGNATURE
// slot's twentieth effect and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "ui/SubmergePage.h"
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
        std::array<std::vector<float>, 2> play (double seconds, int note = 57)
        {
            std::array<std::vector<float>, 2> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            processor.getKeyboardState().noteOn (1, note, 0.9f);
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
                for (int c = 0; c < 2; ++c)
                    out[(size_t) c].insert (out[(size_t) c].end(), buffer.getReadPointer (c), buffer.getReadPointer (c) + blockSize);
            }
            return out;
        }
    };

    double highShare (const std::vector<float>& x, size_t from)
    {
        const auto a = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * 6000.0 / sampleRate);
        double low = 0.0, high = 0.0, all = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            low += (x[i] - low) * a;
            if (i >= from)
            {
                high += (x[i] - low) * (x[i] - low);
                all += (double) x[i] * x[i];
            }
        }
        return high / juce::jmax (1.0e-20, all);
    }
}

TEST_CASE ("SUBMERGE in the plugin: off and DEPTH 0 change nothing; under, a note darkens", "[submerge][plugin]")
{
    Plugin plain;
    const auto reference = plain.play (1.0);

    Plugin resting;
    resting.set (ParamIDs::submergeOn, 1.0f);
    resting.set (ParamIDs::submergeCurrent, 1.0f);
    CHECK (resting.play (1.0) == reference);

    Plugin sunk;
    sunk.set (ParamIDs::submergeOn, 1.0f);
    sunk.set (ParamIDs::submergeDepth, 1.0f);
    const auto shut = sunk.play (1.0);
    const auto from = (size_t) (0.3 * sampleRate);
    INFO ("above 6 kHz: dry " << highShare (reference[0], from) << ", under " << highShare (shut[0], from));
    CHECK (highShare (shut[0], from) < 0.5 * highShare (reference[0], from));
}

TEST_CASE ("SUBMERGE in the plugin: MACRO 3 sinks it through the matrix and back to untouched", "[submerge][plugin]")
{
    for (const auto* id : { ParamIDs::submergeDepth, ParamIDs::submergeBubbles, ParamIDs::submergeCurrent })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Submerge");
    }

    Plugin plain;
    const auto reference = plain.play (12.0);

    Plugin p;
    p.set (ParamIDs::submergeOn, 1.0f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::submergeDepth, 1.0f, false, 0.0f, 0.0f });
    p.processor.setModMatrix (m);
    p.set (ParamIDs::macros[2], 1.0f);
    std::array<std::vector<float>, 2> out;
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    p.processor.getKeyboardState().noteOn (1, 57, 0.9f);
    for (int b = 0; b < (int) (12.0 * sampleRate / blockSize); ++b)
    {
        if (b * blockSize >= (int) (1.0 * sampleRate))
            p.set (ParamIDs::macros[2], 0.0f);
        buffer.clear();
        p.processor.processBlock (buffer, midi);
        out[0].insert (out[0].end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
    }
    const auto at = (size_t) (0.5 * sampleRate), late = (size_t) (11.0 * sampleRate);
    CHECK (std::abs (out[0][at] - reference[0][at]) + std::abs (out[0][at + 100] - reference[0][at + 100]) > 0.0f);
    CHECK (std::equal (out[0].begin() + (long) late, out[0].end(), reference[0].begin() + (long) late));
}

TEST_CASE ("SUBMERGE: the SIGNATURE slot's twenty-first effect, with its page", "[submerge][ui]")
{
    CHECK (signatureNames()[20] == "SUBMERGE");
    ParagonX1AudioProcessor processor;
    SubmergePage page (processor);
    page.setSize (1200, 600);
    CHECK (page.getShownControls().size() == 7);
    for (auto* c : page.getShownControls())
        CHECK (! c->getBounds().isEmpty());
}
