// CANYON in PR7-K (0.84.0): off and at AMOUNT 0 change nothing; at full a short note comes back off the walls after it
// has stopped; AMOUNT, SIZE and REPEATS are mod-matrix destinations (FX Canyon), MACRO 3 takes it up and back,
// untouched again once the echoes have died; the SIGNATURE slot's twenty-fourth effect and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "ui/CanyonPage.h"
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
        std::array<std::vector<float>, 2> play (double seconds, int note = 57, double releaseAt = 1.0e9,
                                                double macroDownAt = 1.0e9)
        {
            std::array<std::vector<float>, 2> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            processor.getKeyboardState().noteOn (1, note, 0.9f);
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                if ((double) b * blockSize >= releaseAt * sampleRate)
                    processor.getKeyboardState().noteOff (1, note, 0.0f);
                if ((double) b * blockSize >= macroDownAt * sampleRate)
                    set (ParamIDs::macros[2], 0.0f);
                buffer.clear();
                processor.processBlock (buffer, midi);
                for (int c = 0; c < 2; ++c)
                    out[(size_t) c].insert (out[(size_t) c].end(), buffer.getReadPointer (c), buffer.getReadPointer (c) + blockSize);
            }
            return out;
        }
    };

    double powerOf (const std::vector<float>& x, size_t from, size_t to)
    {
        double p = 0.0;
        for (auto i = from; i < juce::jmin (to, x.size()); ++i)
            p += (double) x[i] * x[i];
        return p / (double) juce::jmax ((size_t) 1, juce::jmin (to, x.size()) - from);
    }
}

TEST_CASE ("CANYON in the plugin: off and AMOUNT 0 change nothing; at full a note comes back after it stops", "[canyon][plugin]")
{
    Plugin plain;
    const auto reference = plain.play (4.0, 57, 0.3);

    Plugin resting;
    resting.set (ParamIDs::canyonOn, 1.0f);
    resting.set (ParamIDs::canyonRepeats, 1.0f);
    CHECK (resting.play (4.0, 57, 0.3) == reference);

    Plugin canyon;
    canyon.set (ParamIDs::canyonOn, 1.0f);
    canyon.set (ParamIDs::canyonAmount, 1.0f);
    const auto out = canyon.play (4.0, 57, 0.3);
    // After the note has died, the far wall's reply (a half note at the default 120 BPM: 1 s) comes back.
    const auto from = (size_t) (1.0 * sampleRate), to = (size_t) (1.4 * sampleRate);
    const auto dry = powerOf (reference[0], from, to), wet = powerOf (out[0], from, to);
    INFO ("at the far reply: plain " << 10.0 * std::log10 (dry + 1e-20) << " dB, CANYON " << 10.0 * std::log10 (wet + 1e-20) << " dB");
    CHECK (wet > 100.0 * (dry + 1.0e-12));
}

TEST_CASE ("CANYON in the plugin: MACRO 3 takes it up through the matrix and back to untouched", "[canyon][plugin]")
{
    for (const auto* id : { ParamIDs::canyonAmount, ParamIDs::canyonSize, ParamIDs::canyonRepeats })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Canyon");
    }

    Plugin plain;
    const auto reference = plain.play (14.0);

    Plugin p;
    p.set (ParamIDs::canyonOn, 1.0f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::canyonAmount, 1.0f, false, 0.0f, 0.0f });
    p.processor.setModMatrix (m);
    p.set (ParamIDs::macros[2], 1.0f);
    const auto out = p.play (14.0, 57, 1.0e9, 1.0);
    const auto at = (size_t) (0.6 * sampleRate), late = (size_t) (13.0 * sampleRate);
    double moved = 0.0;
    for (auto i = at; i < at + 2000; ++i)
        moved += std::abs (out[0][i] - reference[0][i]);
    CHECK (moved > 0.0);
    // The echoes die away, then the sound passes untouched again.
    CHECK (p.processor.getCanyon().isResting());
    CHECK (std::equal (out[0].begin() + (long) late, out[0].end(), reference[0].begin() + (long) late));
}

TEST_CASE ("CANYON: the SIGNATURE slot's twenty-fourth effect, with its page", "[canyon][ui]")
{
    CHECK (signatureNames()[23] == "CANYON");
    ParagonX1AudioProcessor processor;
    CanyonPage page (processor);
    page.setSize (1200, 600);
    CHECK (page.getShownControls().size() == 7);
    for (auto* c : page.getShownControls())
        CHECK (! c->getBounds().isEmpty());
}
