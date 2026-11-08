// JACK-IN in PR7-K (0.85.0): off and at AMOUNT 0 change nothing; at full a held note is cut up; AMOUNT, CHAOS and CRUSH
// are mod-matrix destinations (FX Jack-In), MACRO 3 takes it over and back, untouched again once the last taken slice
// has faded; the SIGNATURE slot's twenty-fifth effect and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginProcessor.h"
#include "ui/JackInPage.h"
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
        std::array<std::vector<float>, 2> play (double seconds, int note = 57, double macroDownAt = 1.0e9)
        {
            std::array<std::vector<float>, 2> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            processor.getKeyboardState().noteOn (1, note, 0.9f);
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
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
}

TEST_CASE ("JACK-IN in the plugin: off and AMOUNT 0 change nothing; at full a held note is cut up", "[jackin][plugin]")
{
    Plugin plain;
    const auto reference = plain.play (3.0);

    Plugin resting;
    resting.set (ParamIDs::jackinOn, 1.0f);
    resting.set (ParamIDs::jackinChaos, 1.0f);
    CHECK (resting.play (3.0) == reference);

    Plugin taken;
    taken.set (ParamIDs::jackinOn, 1.0f);
    taken.set (ParamIDs::jackinAmount, 1.0f);
    taken.set (ParamIDs::jackinDrops, 1.0f);
    const auto out = taken.play (3.0);
    CHECK (out != reference);
    const auto& fx = taken.processor.getJackIn();
    CHECK (fx.getSliceCount() > 20);   // 3 s of sixteenths at 120 BPM: 24
    int dry = fx.getMoveCount (effects::JackIn::Move::dry);
    CHECK ((double) (fx.getSliceCount() - dry) / fx.getSliceCount() > 0.7);
}

TEST_CASE ("JACK-IN in the plugin: MACRO 3 takes it over through the matrix and back to untouched", "[jackin][plugin]")
{
    for (const auto* id : { ParamIDs::jackinAmount, ParamIDs::jackinChaos, ParamIDs::jackinCrush })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Jack-In");
    }

    Plugin plain;
    const auto reference = plain.play (6.0);

    Plugin p;
    p.set (ParamIDs::jackinOn, 1.0f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::jackinAmount, 1.0f, false, 0.0f, 0.0f });
    p.processor.setModMatrix (m);
    p.set (ParamIDs::macros[2], 1.0f);
    const auto out = p.play (6.0, 57, 2.0);
    double moved = 0.0;
    for (auto i = (size_t) (0.5 * sampleRate); i < (size_t) (1.5 * sampleRate); ++i)
        moved += std::abs (out[0][i] - reference[0][i]);
    CHECK (moved > 1.0);
    CHECK (p.processor.getJackIn().isResting());
    const auto late = (size_t) (4.0 * sampleRate);
    CHECK (std::equal (out[0].begin() + (long) late, out[0].end(), reference[0].begin() + (long) late));
}

TEST_CASE ("JACK-IN: the SIGNATURE slot's twenty-fifth effect, with its page", "[jackin][ui]")
{
    CHECK (signatureNames()[24] == "JACK-IN");
    ParagonX1AudioProcessor processor;
    JackInPage page (processor);
    page.setSize (1200, 600);
    CHECK (page.getShownControls().size() == 7);
    for (auto* c : page.getShownControls())
        CHECK (! c->getBounds().isEmpty());
}
