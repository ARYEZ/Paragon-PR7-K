// REMNANT in PR7-K (0.64.0): off and MIX 0 change nothing; on, a held A4 keeps sounding as it was with its layers
// beneath it; MIX, TONE and TAPER are mod-matrix destinations (FX Remnant), and MACRO 3 can bring it in; the
// SIGNATURE slot's tenth effect and its page.
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

TEST_CASE ("REMNANT in the plugin: off and MIX 0 change nothing; on, a held A4 has its layers beneath it", "[remnant]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::remnantMix, 1.0f);   // off: MIX doesn't matter
    zero.set (ParamIDs::remnantOn, 1.0f);   // on at MIX 0
    const auto reference = plain.play (1.5, 1.0);
    CHECK (off.play (1.5, 1.0) == reference);
    CHECK (zero.play (1.5, 1.0) == reference);

    Plugin p;
    p.set (ParamIDs::remnantOn, 1.0f);
    p.set (ParamIDs::remnantMix, 1.0f);
    p.set (ParamIDs::remnantLayers, 1.0f);   // two layers
    const auto out = p.play (4.0, 4.0);
    const auto from = (size_t) (1.5 * sampleRate), length = (size_t) (2.0 * sampleRate);
    const auto note = db (levelAt (out, 440.0, from, length)), first = db (levelAt (out, 220.0, from, length)), second = db (levelAt (out, 110.0, from, length));
    INFO ("A4 " << note << " dB, an octave down " << first << ", two " << second);
    CHECK (first > note - 18.0);
    CHECK (second > note - 24.0);
}

TEST_CASE ("REMNANT: MIX, TONE and TAPER are mod-matrix destinations (FX Remnant); MACRO 3 brings it in and out", "[remnant]")
{
    for (const auto* id : { ParamIDs::remnantMix, ParamIDs::remnantTone, ParamIDs::remnantTaper })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Remnant");
    }
    const auto withMacro = [] (float macro)
    {
        Plugin p;
        p.set (ParamIDs::remnantOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::remnantMix, 0.6f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
        p.set (ParamIDs::macros[2], macro);
        return p.play (2.0, 2.0);
    };
    Plugin plain;
    const auto reference = plain.play (2.0, 2.0);
    CHECK (withMacro (0.0f) == reference);
    CHECK (withMacro (1.0f) != reference);

    // Up and back down (the matrix eases it, never quite to 0): the sound untouched again.
    Plugin p, still;
    p.set (ParamIDs::remnantOn, 1.0f);
    auto m = p.processor.getModMatrix();
    m.routes.push_back ({ true, "macro3", ParamIDs::remnantMix, 0.6f, false, 0.0f, 0.0f });
    p.processor.setModMatrix (m);
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

TEST_CASE ("REMNANT: the SIGNATURE slot's tenth effect, and its page", "[remnant][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("REMNANT"));
    auto& unit = fx.getModFxUnit (12);
    CHECK (unit.getSpec().name == "REMNANT");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getRemnantPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 8);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
