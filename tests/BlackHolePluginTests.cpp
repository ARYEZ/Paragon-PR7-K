// BLACKHOLE in PR7-K (0.62.0): off and MIX 0 change nothing; on, a note leaves a haze after it; HOLD, PULL, TONE
// and MIX are mod-matrix destinations (FX Blackhole), and MACRO 3 can bring it in; the SIGNATURE slot's eighth
// effect and its page.
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

    float peakIn (const std::vector<float>& x, size_t from, size_t to)
    {
        auto p = 0.0f;
        for (auto i = from; i < to && i < x.size(); ++i)
            p = std::max (p, std::abs (x[i]));
        return p;
    }
}

TEST_CASE ("BLACKHOLE in the plugin: off and MIX 0 change nothing; on, a short note leaves a haze after it", "[blackhole]")
{
    Plugin plain, off, zero;
    off.set (ParamIDs::blackholeMix, 1.0f);   // off: MIX doesn't matter
    zero.set (ParamIDs::blackholeOn, 1.0f);   // on at MIX 0
    const auto reference = plain.play (1.0, 0.1);
    CHECK (off.play (1.0, 0.1) == reference);
    CHECK (zero.play (1.0, 0.1) == reference);

    // A 0.1 s note: without the haze, silence after it; with it, the haze hanging on.
    Plugin p;
    p.set (ParamIDs::blackholeOn, 1.0f);
    p.set (ParamIDs::blackholeMix, 1.0f);
    p.set (ParamIDs::blackholeHold, 0.6f);
    const auto out = p.play (1.0, 0.1);
    const auto after = (size_t) (0.3 * sampleRate);
    INFO ("after the note: plain " << peakIn (reference, after, reference.size()) << ", with the haze " << peakIn (out, after, out.size()));
    CHECK (peakIn (reference, after, reference.size()) < 1.0e-4f);
    CHECK (peakIn (out, after, out.size()) > 0.01f);
}

TEST_CASE ("BLACKHOLE: HOLD, PULL, TONE and MIX are mod-matrix destinations (FX Blackhole); MACRO 3 brings it in", "[blackhole]")
{
    for (const auto* id : { ParamIDs::blackholeHold, ParamIDs::blackholePull, ParamIDs::blackholeTone, ParamIDs::blackholeMix })
    {
        REQUIRE (ModDestinations::find (id) != nullptr);
        CHECK (ModDestinations::find (id)->group == "FX Blackhole");
    }
    const auto withMacro = [] (float macro)
    {
        Plugin p;
        p.set (ParamIDs::blackholeOn, 1.0f);
        auto m = p.processor.getModMatrix();
        m.routes.push_back ({ true, "macro3", ParamIDs::blackholeMix, 1.0f, false, 0.0f, 0.0f });
        p.processor.setModMatrix (m);
        p.set (ParamIDs::macros[2], macro);
        return p.play (1.0, 0.1);
    };
    Plugin plain;
    const auto reference = plain.play (1.0, 0.1);
    CHECK (withMacro (0.0f) == reference);
    CHECK (withMacro (1.0f) != reference);
}

TEST_CASE ("BLACKHOLE: the SIGNATURE slot's eighth effect, and its page", "[blackhole][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1600, 900);
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    view->showPage (MainView::Page::fx);
    auto& fx = view->getFxPage();
    fx.show (FxPage::View::modFx);
    fx.showSignature (signatureNames().indexOf ("BLACKHOLE"));
    auto& unit = fx.getModFxUnit (10);
    CHECK (unit.getSpec().name == "BLACKHOLE");
    CHECK (unit.isVisible());
    CHECK (unit.getBounds() == fx.getModFxUnit (3).getBounds());
    unit.getSpec().onEdit();
    auto& page = fx.getBlackHolePage();
    CHECK (page.isVisible());
    CHECK (page.getShownControls().size() == 7);
    CHECK (! page.getDisplay().getBounds().isEmpty());
}
