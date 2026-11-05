// BLOOM in PR7-K (0.74.0): off and at AMOUNT 0 change nothing; on, a held note gains its strings and hall, and the
// note rings on after the key is let go; AMOUNT, RESONANCE and SIZE are mod-matrix destinations (FX Bloom), and MACRO 3
// takes it in and back, untouched again once settled; the SIGNATURE slot's seventeenth effect and its page.
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
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        std::vector<float> play (double seconds, double held, int note = 57)
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

TEST_CASE ("BLOOM in the plugin: off and at AMOUNT 0 change nothing; on, a held note rings on after the key", "[bloom]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::bloomAmount, 1.0f);   // off: AMOUNT doesn't matter
    zero.set (ParamIDs::bloomOn, 1.0f);
    const auto reference = plain.play (2.0, 1.0);
    CHECK (off.play (2.0, 1.0) == reference);
    CHECK (zero.play (2.0, 1.0) == reference);

    // A sine A3 held 1 s: with BLOOM all the way, something is still there a second after the key (and the AMP's
    // release) let go: the strings and the hall; without, silence.
    Plugin p;
    p.set (ParamIDs::bloomOn, 1.0f);
    p.set (ParamIDs::bloomAmount, 1.0f);
    const auto out = p.play (3.0, 1.0);
    Plugin fresh;
    const auto dry = fresh.play (3.0, 1.0);
    const auto from = (size_t) (2.2 * sampleRate), to = std::min (out.size(), dry.size());
    INFO ("after the key: with BLOOM " << db (rms (out, from, to)) << " dB, without " << db (rms (dry, from, to)) << " dB");
    CHECK (db (rms (out, from, to)) > -50.0);
    CHECK (rms (dry, from, to) < 1.0e-5);
    // Its own pitch rings on (220 Hz).
    CHECK (levelAt (out, 220.0, from, to - from) > 10.0 * levelAt (dry, 220.0, from, to - from) + 1.0e-6);
}

TEST_CASE ("BLOOM: AMOUNT, RESONANCE and SIZE are mod-matrix destinations (FX Bloom); MACRO 3 blooms and comes back", "[bloom]")
{
    for (const auto* id : { ParamIDs::bloomAmount, ParamIDs::bloomResonance, ParamIDs::bloomSize })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Bloom");
    }
    const auto routed = [] (Plugin& p)
    {
        p.set (ParamIDs::bloomOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::bloomAmount, 1.0f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
    };
    Plugin plain, at0, at1;
    routed (at0);
    routed (at1);
    at1.set (ParamIDs::macros[2], 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (at0.play (1.5, 1.0) == reference);
    CHECK (at1.play (1.5, 1.0) != reference);

    // Up and back down (the matrix eases it, never quite to 0): untouched again once it has settled (its strings and
    // hall emptied the moment AMOUNT is back at rest).
    Plugin p, still;
    routed (p);
    p.set (ParamIDs::macros[2], 1.0f);
    auto out = p.play (1.28, 8.0);
    p.set (ParamIDs::macros[2], 0.0f);
    const auto after = p.play (6.4, 0.0);
    out.insert (out.end(), after.begin(), after.end());
    still.processor.getKeyboardState().noteOn (1, 57, 0.9f);
    const auto plainOut = still.play (7.68, 0.0);
    REQUIRE (out.size() == plainOut.size());
    auto differs = 0.0f;
    for (auto i = (size_t) (5.5 * sampleRate); i < out.size(); ++i)
        differs = std::max (differs, std::abs (out[i] - plainOut[i]));
    CHECK (differs == 0.0f);
}

TEST_CASE ("BLOOM: the SIGNATURE slot's seventeenth effect, and its page", "[bloom][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("BLOOM"));
    CHECK (signatureNames().indexOf ("BLOOM") == 16);
    auto& unit = fx.getModFxUnit (19);
    CHECK (unit.getSpec().name == "BLOOM");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getBloomPage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 8);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
