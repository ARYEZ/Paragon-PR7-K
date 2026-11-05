// HAZE in PR7-K (0.66.0): off and at DEPTH 0 change nothing; on, a held note's level moves with the pass; DEPTH,
// DOPPLER and WHOOSH are mod-matrix destinations (FX Haze), and MACRO 3 brings it in and out; the SIGNATURE slot's
// twelfth effect and its page.
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

TEST_CASE ("HAZE in the plugin: off and at DEPTH 0 change nothing; on, a held note's level moves with the pass", "[haze]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::hazeDepth, 1.0f);   // off: DEPTH doesn't matter
    zero.set (ParamIDs::hazeOn, 1.0f);
    const auto reference = plain.play (1.5, 1.0);
    CHECK (off.play (1.5, 1.0) == reference);
    CHECK (zero.play (1.5, 1.0) == reference);

    Plugin p;
    p.set (ParamIDs::hazeOn, 1.0f);
    p.set (ParamIDs::hazeDepth, 1.0f);
    p.set (ParamIDs::hazeDoppler, 0.0f);
    p.set (ParamIDs::hazeWhoosh, 0.0f);
    const auto out = p.play (4.0, 4.0);   // the song stopped: its own count, a pass a bar at 120 BPM (the default tempo)
    const auto level = [&out] (double from, double to) { return db (rms (out, (size_t) (from * sampleRate), (size_t) (to * sampleRate))); };
    INFO ("near the edge " << level (0.1, 0.3) << " dB, passing " << level (0.9, 1.1) << " dB");
    CHECK (level (0.9, 1.1) > level (0.1, 0.3) + 6.0);
}

TEST_CASE ("HAZE: DEPTH, DOPPLER and WHOOSH are mod-matrix destinations (FX Haze); MACRO 3 brings it in and out", "[haze]")
{
    for (const auto* id : { ParamIDs::hazeDepth, ParamIDs::hazeDoppler, ParamIDs::hazeWhoosh })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Haze");
    }
    const auto routed = [] (Plugin& p)
    {
        p.set (ParamIDs::hazeOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::hazeDepth, 0.7f, false, 0.0f, 0.0f });
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

TEST_CASE ("HAZE: the SIGNATURE slot's twelfth effect, and its page", "[haze][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("HAZE"));
    auto& unit = fx.getModFxUnit (14);
    CHECK (unit.getSpec().name == "HAZE");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getHazePage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 7);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
