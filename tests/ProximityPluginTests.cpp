// PROXIMITY in PR7-K (0.65.0): off and at 0 change nothing; on, a note's hammer thump lands as the note starts
// (keys and arps); NOISE, ROOM and CLOSE are mod-matrix destinations (FX Proximity), and MACRO 3 brings it in and out;
// the SIGNATURE slot's eleventh effect and its page.
#include "SynthTestUtils.h"

#include "ModDestinations.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/FxPage.h"
#include "ui/MainView.h"
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
            set (ParamIDs::osc (0, "Wave"), (float) (int) synth::OscWave::sine);
            set (ParamIDs::ampRelease, 5.0f);
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        std::vector<float> play (double seconds, double held, int note = 69)
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            if (held > 0.0)
                processor.getKeyboardState().noteOn (1, note, 0.9f);
            auto released = held <= 0.0;
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                if (! released && b * blockSize >= held * sampleRate)
                {
                    processor.getKeyboardState().noteOff (1, note, 0.0f);
                    released = true;
                }
                for (int c = 0; c < 2; ++c)
                    juce::FloatVectorOperations::clear (buffer.getWritePointer (c), blockSize);
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }
            return out;
        }
    };
}

TEST_CASE ("PROXIMITY in the plugin: off and at 0 change nothing; on, the thump lands as the note starts", "[proximity]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::proximityNoise, 1.0f);   // off: NOISE doesn't matter
    zero.set (ParamIDs::proximityOn, 1.0f);
    zero.set (ParamIDs::proximityNoise, 0.0f);
    zero.set (ParamIDs::proximityRoom, 0.0f);
    zero.set (ParamIDs::proximityClose, 0.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (off.play (1.5, 1.0) == reference);
    CHECK (zero.play (1.5, 1.0) == reference);

    Plugin p;
    p.set (ParamIDs::proximityOn, 1.0f);
    p.set (ParamIDs::proximityNoise, 1.0f);
    p.set (ParamIDs::proximityRoom, 0.0f);
    p.set (ParamIDs::proximityClose, 0.0f);
    const auto out = p.play (1.5, 1.0);
    // The difference from the plain sound: the thump, right at the start, and the release's click a second in.
    std::vector<float> touches;
    for (size_t i = 0; i < out.size(); ++i)
        touches.push_back (out[i] - reference[i]);
    float first = 0.0f, release = 0.0f;
    for (size_t i = 0; i < (size_t) (0.05 * sampleRate); ++i) first = std::max (first, std::abs (touches[i]));
    for (size_t i = (size_t) (1.0 * sampleRate); i < (size_t) (1.05 * sampleRate); ++i) release = std::max (release, std::abs (touches[i]));
    float between = 0.0f;
    for (size_t i = (size_t) (0.5 * sampleRate); i < (size_t) (0.95 * sampleRate); ++i) between = std::max (between, std::abs (touches[i]));
    INFO ("thump " << first << ", between " << between << ", release " << release);
    CHECK (first > 0.05f);
    CHECK (release > 0.005f);
    CHECK (between < 1.0e-6f);
}

TEST_CASE ("PROXIMITY: NOISE, ROOM and CLOSE are mod-matrix destinations (FX Proximity); MACRO 3 brings it in and out", "[proximity]")
{
    for (const auto* id : { ParamIDs::proximityNoise, ParamIDs::proximityRoom, ParamIDs::proximityClose })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Proximity");
    }
    const auto routed = [] (Plugin& p)
    {
        p.set (ParamIDs::proximityOn, 1.0f);
        p.set (ParamIDs::proximityNoise, 0.0f);
        p.set (ParamIDs::proximityRoom, 0.0f);
        p.set (ParamIDs::proximityClose, 0.0f);
        auto m = p.processor.getModMatrix();
        for (const auto* id : { ParamIDs::proximityNoise, ParamIDs::proximityRoom, ParamIDs::proximityClose })
            m.routes.push_back ({ true, "macro3", id, 0.6f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
    };
    Plugin plain, at0, at1;
    routed (at0);
    routed (at1);
    at1.set (ParamIDs::macros[2], 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (at0.play (1.5, 1.0) == reference);
    CHECK (at1.play (1.5, 1.0) != reference);

    // Up and back down (the matrix eases it, never quite to 0): untouched again once the room has faded.
    Plugin p, still;
    routed (p);
    p.set (ParamIDs::macros[2], 1.0f);
    auto out = p.play (1.28, 1.0);
    p.set (ParamIDs::macros[2], 0.0f);
    const auto after = p.play (4.48, 0.0);
    out.insert (out.end(), after.begin(), after.end());
    const auto plainOut = still.play (5.76, 1.0);
    REQUIRE (out.size() == plainOut.size());
    auto differs = 0.0f;
    for (auto i = (size_t) (3.0 * sampleRate); i < out.size(); ++i)
        differs = std::max (differs, std::abs (out[i] - plainOut[i]));
    CHECK (differs == 0.0f);
}

TEST_CASE ("PROXIMITY: the SIGNATURE slot's eleventh effect, and its page", "[proximity][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("PROXIMITY"));
    auto& unit = fx.getModFxUnit (13);
    CHECK (unit.getSpec().name == "PROXIMITY");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getProximityPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 7);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
