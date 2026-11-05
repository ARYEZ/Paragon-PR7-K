// LEVIATHAN in PR7-K (0.68.0): off and at MIX 0 change nothing; on, a held A3 gains a voice an octave under it
// (110 Hz, which a sine at A3 doesn't have); MIX, GROWL and DRIFT are mod-matrix destinations (FX Leviathan), and
// MACRO 3 brings it in and out; the SIGNATURE slot's fourteenth effect and its page.
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

TEST_CASE ("LEVIATHAN in the plugin: off and at MIX 0 change nothing; on, a held note gains a voice an octave under it", "[leviathan]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::leviathanMix, 1.0f);   // off: MIX doesn't matter
    zero.set (ParamIDs::leviathanOn, 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (off.play (1.5, 1.0) == reference);
    CHECK (zero.play (1.5, 1.0) == reference);

    Plugin p;
    p.set (ParamIDs::leviathanOn, 1.0f);
    p.set (ParamIDs::leviathanMix, 1.0f);
    p.set (ParamIDs::leviathanDelay, 0.0f);
    p.set (ParamIDs::leviathanGrowl, 0.0f);
    const auto out = p.play (2.5, 2.5, 57);   // A3, a sine (220 Hz)
    const auto dry = plain.play (2.5, 2.5, 57);
    const auto below = [] (const std::vector<float>& x) { return db (levelAt (x, 110.0, (size_t) (1.0 * sampleRate), (size_t) (1.2 * sampleRate))); };
    INFO ("110 Hz with LEVIATHAN " << below (out) << " dB, without " << below (dry) << " dB");
    CHECK (below (out) > below (dry) + 30.0);
}

TEST_CASE ("LEVIATHAN: MIX, GROWL and DRIFT are mod-matrix destinations (FX Leviathan); MACRO 3 brings it in and out", "[leviathan]")
{
    for (const auto* id : { ParamIDs::leviathanMix, ParamIDs::leviathanGrowl, ParamIDs::leviathanDrift })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Leviathan");
    }
    const auto routed = [] (Plugin& p)
    {
        p.set (ParamIDs::leviathanOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::leviathanMix, 0.7f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
    };
    Plugin plain, at0, at1;
    routed (at0);
    routed (at1);
    at1.set (ParamIDs::macros[2], 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (at0.play (1.5, 1.0) == reference);
    CHECK (at1.play (1.5, 1.0) != reference);

    // Up and back down (the matrix eases it, never quite to 0): untouched again.
    Plugin p, still;
    routed (p);
    p.set (ParamIDs::macros[2], 1.0f);
    auto out = p.play (1.28, 6.0);
    p.set (ParamIDs::macros[2], 0.0f);
    const auto after = p.play (4.48, 0.0);
    out.insert (out.end(), after.begin(), after.end());
    still.processor.getKeyboardState().noteOn (1, 69, 0.9f);
    const auto plainOut = still.play (5.76, 0.0);
    REQUIRE (out.size() == plainOut.size());
    auto differs = 0.0f;
    for (auto i = (size_t) (3.0 * sampleRate); i < out.size(); ++i)
        differs = std::max (differs, std::abs (out[i] - plainOut[i]));
    CHECK (differs == 0.0f);
}

TEST_CASE ("LEVIATHAN: the SIGNATURE slot's fourteenth effect, and its page", "[leviathan][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("LEVIATHAN"));
    auto& unit = fx.getModFxUnit (16);
    CHECK (unit.getSpec().name == "LEVIATHAN");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getLeviathanPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 7);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
