// AFTERMATH in PR7-K (0.79.0): off and at SEAL 0 change nothing; sealed in, a held note darkens; SEAL, RING and HUM are
// mod-matrix destinations (FX Lockstep), MACRO 3 seals in and back, untouched again once settled; the SIGNATURE
// slot's twentieth effect and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "ui/LockstepPage.h"
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
}

TEST_CASE ("LOCKSTEP in the plugin: off and AMOUNT 0 change nothing; drilled, a held note is cut into steps", "[lockstep][plugin]")
{
    Plugin plain;
    const auto reference = plain.play (1.0);

    Plugin resting;
    resting.set (ParamIDs::lockstepOn, 1.0f);
    resting.set (ParamIDs::lockstepSqueeze, 1.0f);
    CHECK (resting.play (1.0) == reference);

    Plugin drilled;
    drilled.set (ParamIDs::lockstepOn, 1.0f);
    drilled.set (ParamIDs::lockstepAmount, 1.0f);
    drilled.set (ParamIDs::lockstepStamp, 0.0f);
    const auto cut = drilled.play (2.0);
    // 10 ms slices of the held note's second second: some open, some shut.
    double loudest = 0.0, quietest = 1.0e9;
    for (size_t k = (size_t) sampleRate; k + 480 <= cut[0].size(); k += 480)
    {
        double p = 0.0;
        for (size_t i = k; i < k + 480; ++i)
            p += (double) cut[0][i] * cut[0][i];
        loudest = juce::jmax (loudest, p);
        quietest = juce::jmin (quietest, p);
    }
    INFO ("loudest slice " << 10.0 * std::log10 (loudest) << " dB, quietest " << 10.0 * std::log10 (quietest + 1e-20) << " dB");
    CHECK (loudest > 1000.0 * quietest);
}

TEST_CASE ("LOCKSTEP in the plugin: MACRO 3 drills it through the matrix and back to untouched", "[lockstep][plugin]")
{
    for (const auto* id : { ParamIDs::lockstepAmount, ParamIDs::lockstepStamp, ParamIDs::lockstepSqueeze })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Lockstep");
    }

    Plugin plain;
    const auto reference = plain.play (12.0);

    Plugin p;
    p.set (ParamIDs::lockstepOn, 1.0f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::lockstepAmount, 1.0f, false, 0.0f, 0.0f });
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

TEST_CASE ("LOCKSTEP: the SIGNATURE slot's twenty-second effect, with its page", "[lockstep][ui]")
{
    CHECK (signatureNames()[21] == "LOCKSTEP");
    ParagonX1AudioProcessor processor;
    LockstepPage page (processor);
    page.setSize (1200, 600);
    CHECK (page.getShownControls().size() == 7);
    for (auto* c : page.getShownControls())
        CHECK (! c->getBounds().isEmpty());
}
